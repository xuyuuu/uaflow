#include <stdio.h>
#include <stdint.h>

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
#include <rte_jhash.h>
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
#include "../flow_current/flow_current.h"
#include "../flow_config/flow_config_parser.h"
#include "flow_ddos.h"

/*list*/
static struct list_head flow_ddos_v4_list[FLOW_LCORE_MAX_SLOT][FLOW_DDOS_NODE_LIST_MAX_SZ];
static struct list_head flow_ddos_v6_list[FLOW_LCORE_MAX_SLOT][FLOW_DDOS_NODE_LIST_MAX_SZ];

/*mempool*/
static struct rte_mempool * flow_ddos_pool;

/*random initval*/
static uint32_t flow_ddos_hash_initval[FLOW_LCORE_MAX_SLOT];

/*ddos scan number*/
static uint32_t flow_ddos_scan_number;
/*ddos scan millisecond*/
static uint32_t flow_ddos_scan_millisecond;

static int flow_ddos_module_init(void)
{
	int i, j, ret;
	for(i = 0; i < FLOW_LCORE_MAX_SLOT; i++)
	{
		for(j = 0; j < FLOW_DDOS_NODE_LIST_MAX_SZ; j++)
		{
			INIT_LIST_HEAD(&(flow_ddos_v4_list[i][j]));
			INIT_LIST_HEAD(&(flow_ddos_v6_list[i][j]));
		}
	}

	flow_ddos_scan_number = flow_config_module.getnum();
	flow_ddos_scan_millisecond = flow_config_module.gettime();

	uint32_t initval = rte_rand() & 0xffffffff;
	for(i = 0; i < FLOW_LCORE_MAX_SLOT; i++)
	{
		flow_ddos_hash_initval[i] = initval;
	}

	/*multiple core safety*/
	flow_ddos_pool = rte_mempool_create("flow_ddos_pool", 1 << 16, 
					FLOW_DDOS_MEMPOOL_EACH_ENTRY_SZ,
					32, sizeof(struct rte_pktmbuf_pool_private), 
					NULL, NULL,NULL, NULL, SOCKET_ID_ANY, 0);
	if(!flow_ddos_pool)
	{
		ret = -1;	
		goto out;
	}
	ret = 0;	
out:
	return ret;
}

static int flow_ddos_module_v4detect(struct ipv4_hdr *ipv4_hdr,struct tcp_hdr *tcp_hdr)
{
	uint32_t coreid, hash, mtime = 3000000000;
	int cnt = 0, ret = 0, res = 0, i;
	struct flow_ddos_node *item;
	struct flow_ddos_node *old = NULL;

	coreid = rte_lcore_id();

	hash = rte_jhash_1word(ipv4_hdr->src_addr, flow_ddos_hash_initval[coreid]) 
							& FLOW_DDOS_NODE_LIST_MASK;
	list_for_each_entry(item, &(flow_ddos_v4_list[coreid][hash]), head)
	{
		if(item->addrv4 == ipv4_hdr->src_addr)
			goto next;

		else if(item->modtime < mtime)
		{
			mtime = item->modtime;
			old = item;
		}
		cnt++;
	}
	item = NULL;
next:
	if(!item) /*cannot find*/
	{
		if(cnt == 10)	
			item = old;
		else
		{
			ret = rte_mempool_get(flow_ddos_pool, (void **)&item);
			if(ret < 0)
				goto out;
		}
		memset(item, 0x0, FLOW_DDOS_MEMPOOL_EACH_ENTRY_SZ);
		INIT_LIST_HEAD(&item->head);
		item->modtime = flow_current_module.get();
		item->n_packets = 0;
		item->addrv4 = ipv4_hdr->src_addr;

		list_add_tail(&item->head, &(flow_ddos_v4_list[coreid][hash]));
	}
	/*item is not null*/
	if((item->modtime - flow_current_module.get()) * 1000 > flow_ddos_scan_millisecond)	
	{
		item->n_packets = 0;
	}
	item->modtime = flow_current_module.get();
	for(i = 0; i < item->n_packets; i++)
	{
		if (item->entry[i].port == tcp_hdr->src_port &&
			item->entry[i].addrv4 == ipv4_hdr->src_addr)
			return res;
	}
	item->entry[item->n_packets].port = tcp_hdr->src_port;
	item->entry[item->n_packets].addrv4 = ipv4_hdr->src_addr;
	item->n_packets++;

	if(item->n_packets > flow_ddos_scan_number)
	{
		res = 1;
		item->n_packets = 0;
	}

out:	
	return res;
}

static int flow_ddos_module_v6detect(struct ipv6_hdr *ipv6_hdr,struct tcp_hdr *tcp_hdr)
{
	uint32_t coreid, hash, mtime = 3000000000;
	int cnt = 0, ret = 0, res = 0, i;
	struct flow_ddos_node *item = NULL;
	struct flow_ddos_node *old = NULL;
	struct list_head *this = NULL;

	coreid = rte_lcore_id();
	this = flow_ddos_v6_list[coreid];

	hash = rte_jhash(ipv6_hdr->src_addr, 16, flow_ddos_hash_initval[coreid]) 
							& FLOW_DDOS_NODE_LIST_MASK;
	list_for_each_entry(item, &this[hash], head)
	{
		if(!memcmp(item->addrv6, ipv6_hdr->src_addr, 16))
			goto next;

		else if(item->modtime < mtime)
		{
			mtime = item->modtime;
			old = item;
		}
		cnt++;
	}
	item = NULL;
next:
	if(!item) /*cannot find*/
	{
		if(cnt == 10)	
			item = old;
		else
		{
			ret = rte_mempool_get(flow_ddos_pool, (void **)&item);
			if(ret < 0)
				goto out;
		}
		item->modtime = flow_current_module.get();
		item->n_packets = 0;
		memcpy(item->addrv6, ipv6_hdr->src_addr, 16);
		INIT_LIST_HEAD(&item->head);
		list_add_tail(&item->head, &this[hash]);
	}
	/*item is not null*/
	if((item->modtime - flow_current_module.get())*1000 > flow_ddos_scan_millisecond)	
	{
		item->n_packets = 0;
	}
	item->modtime = flow_current_module.get();
	for(i = 0; i < item->n_packets; i++)
	{
		if (item->entry[i].port == tcp_hdr->src_port &&
			!memcmp(item->entry[i].addrv6, ipv6_hdr->src_addr, 16))
			return res;
	}
	item->entry[item->n_packets].port = tcp_hdr->src_port;
	memcpy(item->entry[item->n_packets].addrv6, ipv6_hdr->src_addr, 16);
	item->n_packets++;

	if(item->n_packets > flow_ddos_scan_number)
	{
		res = 1;
		item->n_packets = 0;
	}
out:	
	return res;
}


struct flow_ddos_module flow_ddos_module = 
{
	.init		= flow_ddos_module_init,
	.v4detect	= flow_ddos_module_v4detect,
	.v6detect	= flow_ddos_module_v6detect
};



