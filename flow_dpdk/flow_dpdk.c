#include <stdio.h>
#include <unistd.h>

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

#include "flow_dpdk.h"
#include "../flow_core/flow_core_types.h"
#include "../flow_core/flow_core_list.h"
#include "../flow_config/flow_config_parser.h"
#include "../flow_forward/flow_forward.h"
#include "../flow_forward/flow_forward_defsend.h"

volatile int flow_dpdk_core_run_flag;

static struct rte_mempool * flow_pktmbuf_pool[FLOW_DPDK_NB_SOCKETS];

static volatile int flow_dpdk_rx_queue_mask[FLOW_LCORE_MAX_SLOT];
static volatile int flow_dpdk_tx_queue_mask[FLOW_LCORE_MAX_SLOT];
static volatile int flow_dpdk_rx_queue_num;
static volatile int flow_dpdk_tx_queue_num;

static const struct rte_eth_conf flow_port_conf =
{
        .link_speed     = ETH_LINK_SPEED_AUTONEG,
        .link_duplex    = ETH_LINK_AUTONEG_DUPLEX,
        .rxmode =
        {
                .mq_mode        = ETH_MQ_RX_RSS,
                .max_rx_pkt_len = ETHER_MAX_LEN,
                .split_hdr_size = 0,
                .header_split   = 0, /**< Header Split disabled */
                .hw_ip_checksum = 1, /**< IP checksum offload enabled */
                .hw_vlan_filter = 0, /**< VLAN filtering disabled */
                .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
                .hw_strip_crc   = 0, /**< CRC stripped by hardware */
        },  
        .rx_adv_conf =
        {   
                .rss_conf =
                {   
                        .rss_key = NULL,
                        .rss_hf = (ETH_RSS_IPV4 
                                        | ETH_RSS_TCP 
                                        | ETH_RSS_UDP
					| ETH_RSS_IPV6),
			/*
                        .rss_hf = (ETH_RSS_IPV4 
                                        | ETH_RSS_IPV4_TCP 
                                        | ETH_RSS_IPV4_UDP
					| ETH_RSS_IPV6_TCP
					| ETH_RSS_IPV6_UDP),
			*/
                },  
        },  
        .txmode =
        {   
                .mq_mode = ETH_MQ_TX_NONE,
        },  
};

static const struct rte_eth_txconf flow_tqueue_conf =
{
       .tx_thresh =
        {
                .pthresh = TX_PTHRESH,
                .hthresh = TX_HTHRESH,
                .wthresh = TX_WTHRESH,
        },
        .tx_free_thresh = 1,
        .tx_rs_thresh   = 1,
};

static int flow_dpdk_module_init(void)
{
	int ncpu, ret; 
	char *cpus = NULL;

	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	switch(ncpu)	
	{
		case 4:
			cpus = "0x7";
			break;
		case 6:
			cpus = "0x1f";
			break;
		case 8:
			cpus = "0x7f";
			break;
		case 12:
			cpus = "0x7ff";
			break;
		case 16:
			cpus = "0x7fff";
			break;
		case 18:
			cpus = "0x1ffff";
			break;
		case 24:
			cpus = "0x7fffff";
			break;
		case 32:
			cpus = "0x7fffffff";
			break;
		default:
			cpus = "0x7";
			break;
	}

	if(ncpu == 2)
                rte_exit(EXIT_FAILURE, "Invalid CPU Number\n");

	char * flowargv[5] = 
	{
		"dpdk_flow",
		"-c",
		cpus,
		"-n",
		"4"
	};
	ret = rte_eal_init(sizeof(flowargv)/sizeof(char *), flowargv);
	if (ret < 0) 
                rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	
	/*set bind id for flow_forward_defsend_module*/
	flow_forward_defsend_module.setbid(ncpu - 1);

	return 0;
}

static int
flow_dpdk_module_setconf(void)
{
	int i, nb_ports, nb_rx_queue, nb_tx_queue, nb_lcores,
		rxi, txi, ret;
	struct rte_eth_dev_info dev_info;
	char name[512];

	/* get socket number*/
	nb_ports = rte_eth_dev_count();
        if (nb_ports > RTE_MAX_ETHPORTS)
                nb_ports = RTE_MAX_ETHPORTS;

	/*get lcores number*/
	nb_lcores = rte_lcore_count();


	for(i = 0; i < nb_ports; i++)
	{
		memset(&dev_info, 0x0, sizeof(struct rte_eth_dev_info));
		rte_eth_dev_info_get(i, &dev_info);
		nb_rx_queue = dev_info.max_rx_queues > (nb_lcores - 1) ? (nb_lcores - 1) :
			dev_info.max_rx_queues;
		nb_tx_queue = dev_info.max_tx_queues > (nb_lcores - 1) ? (nb_lcores - 1) :
			dev_info.max_tx_queues;

		flow_dpdk_rx_queue_num = nb_rx_queue;
		flow_dpdk_tx_queue_num = nb_tx_queue;

		memset(name, 0x0, sizeof(name));
		sprintf(name, "flow_dpdk_mempool_%u", i);
		flow_pktmbuf_pool[i] = rte_pktmbuf_pool_create(name, NB_MBUF, MEMPOOL_CACHE_SIZE,
               			0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		/* use other version
		flow_pktmbuf_pool[i] = rte_mempool_create(name ,NB_MBUF, MBUF_SIZE, 
						MEMPOOL_CACHE_SIZE, 
                                                sizeof(struct rte_pktmbuf_pool_private),
                                                rte_pktmbuf_pool_init, NULL, 
						rte_pktmbuf_init, NULL, 
						rte_socket_id(), 0);
		*/
		
		ret = rte_eth_dev_configure(i, nb_rx_queue,
                                        nb_tx_queue, &flow_port_conf);
		if (ret < 0)
		{
                        rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%d\n",
                                ret, i);
		}
		for(rxi = 0; rxi < nb_rx_queue; rxi++)
		{
			ret = rte_eth_rx_queue_setup(i, rxi, RTE_TEST_RX_DESC_DEFAULT,
                                        rte_eth_dev_socket_id(i),
                                        NULL,
                                        flow_pktmbuf_pool[i]);
			if (ret < 0)
			{
                                rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: err=%d,"
                               		"port=%d\n", ret, i);
			}

			flow_dpdk_rx_queue_mask[rxi] = 1;
		}
		for(txi = 0; txi < nb_tx_queue; txi++)
		{
			ret = rte_eth_tx_queue_setup(i, txi, RTE_TEST_TX_DESC_DEFAULT,
                                                     rte_eth_dev_socket_id(i), &flow_tqueue_conf);
			if (ret < 0)
			{
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d,"
						"port=%d\n", ret, i);
			}

			flow_dpdk_tx_queue_mask[txi] = 1;
		}

		/*start device*/
		ret = rte_eth_dev_start(i);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d\n",ret, i);
		/*set promisc*/
		rte_eth_promiscuous_enable(i);
	}
	ret = 0;

	return ret;
}

static int
flow_dpdk_module_cores_loop(__attribute__((unused)) void *dummy)
{
	int lcore_id = rte_lcore_id();
	int queueid = lcore_id - 1;

	if(!flow_dpdk_rx_queue_mask[queueid])
	{
		printf("NULL task, exit now...\n");
		return 0;
	}

	while(!flow_dpdk_core_run_flag)
	{
		/* cap data from pmd  */	
		flow_forward_module.forward(queueid);
	}

	printf("lcore thread exit normal...\n");
	return 0;
}

static int flow_dpdk_module_handler(void)
{
	 rte_eal_mp_remote_launch(flow_dpdk_module_cores_loop, NULL, SKIP_MASTER);

	return 0;
}

static int flow_dpdk_module_getaliq(void)
{
	int txnum = flow_dpdk_tx_queue_num, ret;
	do
	{
		if(rte_is_power_of_2(txnum))
		{
			goto next;
		}	
	}while(txnum--);
	ret = 1;

next:
	ret = txnum - 1;

	return ret;
}

static int flow_dpdk_module_dump_inside_link_status(FILE *log)
{
	int nb_port, i;
	struct rte_eth_link link;
	struct rte_eth_stats stats;
	char buff[1024];

	nb_port = rte_eth_dev_count();
        if (nb_port > RTE_MAX_ETHPORTS)
                nb_port = RTE_MAX_ETHPORTS;

	for(i = 0; i < nb_port; i++)	
	{
		memset(buff, 0x0, sizeof(buff));
		memset(&link, 0x0, sizeof(link));
		memset(&stats, 0x0, sizeof(stats));
		rte_eth_link_get_nowait(i, &link);
		if(rte_eth_stats_get(i, &stats))
			memset(&stats, 0x0, sizeof(stats));
		//rte_eth_stats_get(i, &stats);

		if(link.link_status)
		{
			sprintf(buff, "Port %d Link Up - speed %u Mbps -%s  \n\
recvips %lu, recvibyte %lu, imissips %lu\n\
sendvips %lu, sendbyte %lu, oerrors %lu\n\
**************************************************************\n", 
				i, link.link_speed, (link.link_duplex == ETH_LINK_FULL_DUPLEX)?("full-duplex"):
				("halt-duplex\n"), stats.ipackets, stats.ibytes, stats.imissed,
				stats.opackets, stats.obytes, stats.oerrors);
		}
		else
		{
			sprintf(buff, "Port %d Link Down\n\
**************************************************************\n", i);
		}
		fwrite(buff, strlen(buff), 1, log);
	}

	return 0;
}

static int flow_dpdk_module_dump(const char *logname)
{
	FILE *log = NULL;

	log = fopen(logname, "a");
	if (!log)
	{
		goto out;
	}

	fwrite("\n\n[NIC-LinkStatus]:\n\n", sizeof("\n\n[NIC-LinkStatus]:\n\n") - 1, 1,log);
	flow_dpdk_module_dump_inside_link_status(log);

out:
	if(log)
		fclose(log);
	return 0;
}

struct flow_dpdk_module flow_dpdk_module =
{
	.init		= flow_dpdk_module_init,
	.setconf	= flow_dpdk_module_setconf,
	.handler	= flow_dpdk_module_handler,
	.getaliq	= flow_dpdk_module_getaliq,
	.dump		= flow_dpdk_module_dump
};



