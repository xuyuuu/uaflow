#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include <rte_common.h> 
#include <rte_byteorder.h> 
#include <rte_log.h> 
#include <rte_memory.h> 
#include <rte_memcpy.h> 
#include <rte_memzone.h> 
#include <rte_tailq.h> 
#include <rte_eal.h> 
#include <rte_per_lcore.h> 
#include <rte_launch.h> 
#include <rte_atomic.h> 
#include <rte_cycles.h> 
#include <rte_prefetch.h> 
#include <rte_launch.h>
#include <rte_lcore.h> 
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h> 
#include <rte_interrupts.h> 
#include <rte_pci.h> 
#include <rte_random.h> 
#include <rte_debug.h> 
#include <rte_ether.h> 
#include <rte_ethdev.h> 
#include <rte_ring.h> 
#include <rte_mempool.h> 
#include <rte_mbuf.h> 
#include <rte_ip.h> 
#include <rte_tcp.h> 
#include <rte_udp.h> 
#include <rte_string_fns.h> 
#include <rte_kni.h>

#include "flow_kni.h"
#include "../flow_core/flow_core_list.h"
#include "../flow_dpdk/flow_dpdk.h"
#include "../flow_config/flow_config_parser.h"

/* Mempool for KNI mbuf*/
static struct rte_mempool * flow_kni_pktmbuf_pool;
static flow_kni_arg_t flow_kni_arg;

static struct rte_kni * flow_kni;

static void flow_kni_module_inside_kni_burst_free_mbufs(struct rte_mbuf **pkts, \
						unsigned num) 
{
	unsigned i;
	if(NULL == pkts)
		return;

	for(i = 0; i < num; i++)
	{
		rte_pktmbuf_free(pkts[i]);
	}
}

static void *flow_kni_main_loop(void *arg)
{
	uint16_t knigid;
	flow_kni_arg_t *tmparg = (flow_kni_arg_t *)arg;

	knigid = tmparg->portid;
	/*set thread*/
	pthread_t tid;
	tid = pthread_self();
	pthread_detach(tid);
	tmparg->loopid = tid;
	/*bind master core*/
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(FLOW_KNI_MASTER_CORE_ID, &cpuset);
	pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);

	/*wait signal*/
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
	pthread_mutex_lock(&tmparg->lock);	
	pthread_cond_timedwait(&tmparg->cond, &tmparg->lock, &ts);
	pthread_mutex_unlock(&tmparg->lock);

	/*loop task*/
	uint32_t num, nb_tx;
	struct rte_mbuf * pkts_burst[FLOW_PKT_BURST_SZ];
	while(!(*tmparg->runflag))
	{
		num = rte_kni_rx_burst(flow_kni, pkts_burst, FLOW_PKT_BURST_SZ);
		if(0 == num)
			continue;

		if(unlikely(num > FLOW_PKT_BURST_SZ))
		{
			rte_exit(EXIT_FAILURE, "Could not read packets from kni successfully\n");
			return NULL;
		}

		nb_tx = rte_eth_tx_burst(knigid, 0, pkts_burst, (uint16_t)num);
		if (unlikely(nb_tx < num))
		{
			flow_kni_module_inside_kni_burst_free_mbufs(&pkts_burst[nb_tx], num - nb_tx);
		}
	}

	/*destroy kni source*/
	rte_kni_release(flow_kni);	

	return NULL;
}

static int flow_kni_module_run(volatile int *flag)
{
	flow_kni_arg.runflag = flag;
	/*detach loop task*/
	pthread_cond_signal(&flow_kni_arg.cond);
	return 0;
}



static int flow_kni_module_inside_kni_init(int portid)
{
	/*default start one kernel kni thread*/
	struct rte_kni_ops ops;
	memset(&ops, 0x0, sizeof(ops));
	ops.port_id = portid;
	//ops.change_mtu = flow_kni_module_inside_change_mtu;
	//ops.config_network_if = flow_kni_module_inside_config_network_if;

	struct rte_eth_dev_info dev_info;
	memset(&dev_info, 0x0, sizeof(dev_info));
	rte_eth_dev_info_get(portid, &dev_info);

	struct rte_kni_conf conf;
	memset(&conf, 0x0, sizeof(struct rte_kni_conf));
	conf.core_id = FLOW_KNI_MASTER_CORE_ID;

	/*bind master core*/
	conf.force_bind = 1;
	/*set kni name*/
	snprintf(conf.name, RTE_KNI_NAMESIZE, "FlowEth%u", portid);
	conf.mbuf_size = FLOW_MAX_PACKET_SZ;
	conf.addr = dev_info.pci_dev->addr;
	conf.id = dev_info.pci_dev->id;

	flow_kni = rte_kni_alloc(flow_kni_pktmbuf_pool, &conf, &ops);	
	if(!flow_kni)
		rte_exit(EXIT_FAILURE,"Fail to create kni for port: %d\n", portid);

	return 0;
}

/* Initialize KNI*/
static int flow_kni_module_init(void)
{
	const int nkni = 1;
	uint16_t portid;
	pthread_t tid;

	flow_config_idx_t ** prxidx = flow_config_module.getrx();
	portid = prxidx[0]->idx; 	

	memset(&flow_kni_arg, 0x0, sizeof(flow_kni_arg_t));
	pthread_mutex_init(&flow_kni_arg.lock, NULL);
	pthread_cond_init(&flow_kni_arg.cond, NULL);
	flow_kni_arg.portid = portid;

	/*set mempool*/
	flow_kni_pktmbuf_pool = rte_pktmbuf_pool_create("flow_kni_mbuf_pool", NB_MBUF,
		MEMPOOL_CACHE_SIZE, 0, MBUF_SIZE, SOCKET_ID_ANY);
	if(flow_kni_pktmbuf_pool== NULL) {
		rte_exit(EXIT_FAILURE, "Could not initialise kni mbuf pool\n");
		return -1;
	}

	/*set one kni thread*/
	rte_kni_init(nkni);
	flow_kni_module_inside_kni_init(portid);
	
	/*create task thread*/
	pthread_create(&tid, NULL, flow_kni_main_loop, &flow_kni_arg);	

	return 0;
}


struct flow_kni_module flow_kni_module = 
{
	.init		= flow_kni_module_init,
	.run		= flow_kni_module_run
};

