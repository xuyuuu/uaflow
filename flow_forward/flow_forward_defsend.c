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

#include "../flow_core/flow_core_list.h"
#include "../flow_core/flow_core_types.h"
#include "../flow_dpdk/flow_dpdk.h"
#include "flow_forward_defsend.h"
#include "flow_forward.h"
#include "../flow_config/flow_config_parser.h"

static int flow_forward_defsend_module_thread_bind_id;
static flow_forward_defsend_arg_t flow_forward_defsend_arg;
//static struct rte_mempool * flow_forward_defsend_pool;
static struct rte_ring *flow_forward_defsend_ring;

static void * flow_forward_defsend_main_loop(void *arg) 
{
	int ret, defaultidx, queueid;
	uint8_t *mtod;
	flow_forward_defsend_arg_t *tmparg = (flow_forward_defsend_arg_t *)arg;

	/*set thread*/
	pthread_t tid;
	tid = pthread_self();
	pthread_detach(tid);
	tmparg->loopid = tid;
	/*set cpu affinity*/
	cpu_set_t cpuset;	
	CPU_ZERO(&cpuset);
	CPU_SET(flow_forward_defsend_module_thread_bind_id, &cpuset);
	pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);

	/*wait signal*/
	struct timespec ts; 
	clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
	pthread_mutex_lock(&tmparg->lock);
	pthread_cond_timedwait(&tmparg->cond, &tmparg->lock, &ts);
	pthread_mutex_unlock(&tmparg->lock);

	flow_config_idx_t ** ptxidx = flow_config_module.gettx();
        defaultidx = ptxidx[0]->idx; /*for default socket*/

	/*mask queueid num*/
	int mask = flow_dpdk_module.getaliq();
	/*loop task*/
	while(!*tmparg->runflag)
	{
		struct rte_mbuf * mbuf; 
		ret = flow_forward_defsend_module.pop(&mbuf);
		if(ret < 0)
			continue;

		/*send by default socket*/
		mtod = rte_pktmbuf_mtod(mbuf, uint8_t *);
		if(mtod)
		{

			queueid = *(mtod + 17) & mask;
			flow_forward_module.send(mbuf, defaultidx, queueid, flow_forward_defsend_module_thread_bind_id);
		}
	}

	return NULL;
}

static int flow_forward_defsend_module_init(void)
{
	int ret;
/*
	flow_forward_defsend_pool= rte_mempool_create("flow_forward_defsend_pool", 1 << 18, sizeof(void *),
							256, sizeof(struct rte_pktmbuf_pool_private), 
							NULL, NULL,NULL, NULL, SOCKET_ID_ANY, 0);
	if(!flow_forward_defsend_pool)
	{
		ret = -1;	
		goto out;
	}
*/

	flow_forward_defsend_ring = rte_ring_create("flow_forward_defsend_ring", 1 << 18, SOCKET_ID_ANY, 0);
	if(!flow_forward_defsend_ring)
	{
		ret = -1;
		goto out;
	}

	memset(&flow_forward_defsend_arg, 0x0, sizeof(flow_forward_defsend_arg_t));	
	pthread_mutex_init(&flow_forward_defsend_arg.lock, NULL);
	pthread_cond_init(&flow_forward_defsend_arg.cond, NULL);

	/*create pthread*/
	pthread_t tid;
	pthread_create(&tid, NULL, flow_forward_defsend_main_loop, &flow_forward_defsend_arg);	

	ret = 0;

out:
	return ret;
}

static int flow_forward_defsend_module_run(volatile int *flag)
{
	flow_forward_defsend_arg.runflag = flag;
	/* detach loop task */
	pthread_cond_signal(&flow_forward_defsend_arg.cond);
	return 0;
}


static int flow_forward_defsend_module_push(struct rte_mbuf *pkt_burst)
{
	return rte_ring_enqueue(flow_forward_defsend_ring, (void *)pkt_burst);
}


static int flow_forward_defsend_module_pop(struct rte_mbuf **pkts_burst)
{
	int ret;
	ret = rte_ring_dequeue(flow_forward_defsend_ring, (void **)pkts_burst);
	if(ret < 0)
		goto err;
	ret = 0;

err:
	return ret;
}

static int flow_forward_defsend_module_setbid(int tid)
{
	if (tid < 0 || tid >= FLOW_LCORE_MAX_SLOT)
		tid = 0; /*tid error, set default tid*/

	flow_forward_defsend_module_thread_bind_id = tid;

	return 0;
}

struct flow_forward_defsend_module flow_forward_defsend_module = 
{
	.init		= flow_forward_defsend_module_init,
	.run		= flow_forward_defsend_module_run,
	.push		= flow_forward_defsend_module_push,
	.pop		= flow_forward_defsend_module_pop,
	.setbid		= flow_forward_defsend_module_setbid
};
