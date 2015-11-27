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
#include <rte_rwlock.h>

#include "../flow_core/flow_core_types.h"
#include "../flow_core/flow_core_list.h"
#include "../flow_config/flow_config_parser.h"
#include "../flow_ua/flow_ua_common.h"
#include "../flow_ddos/flow_ddos.h"
#include "../flow_current/flow_current.h"
#include "../flow_dpdk/flow_dpdk.h"
#include "flow_forward.h"
#include "flow_forward_defsend.h"

static flow_forward_mbuf_table_t flow_forward_global_mbuf_table[FLOW_LCORE_MAX_SLOT];

static int flow_forward_module_inside_send_pkt_default(struct rte_mbuf *pkt_burst)
{
	int ret;
	ret = flow_forward_defsend_module.push(pkt_burst);
	if(ret < 0)
	{
		/*free out missed packets*/
		rte_pktmbuf_free(pkt_burst);
	}
	
	return 0;
}

static int flow_forward_module_inside_ipv4_fwd_pkts(struct rte_mbuf *pkt_burst)
{
	int def = 0;
	struct ether_hdr *eth_hdr;	
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;

	int ether_len;
	int ipv4_totlen, ipv4_headerlen;
	int tcp_totlen, tcp_headerlen;

	eth_hdr = rte_pktmbuf_mtod(pkt_burst, struct ether_hdr *);
	if((!eth_hdr) || (rte_be_to_cpu_16(eth_hdr->ether_type) != ETHER_TYPE_IPv4))
	{
		def = -1;
		goto def;
	}
	ether_len = sizeof(struct ether_hdr);

	ipv4_hdr = rte_pktmbuf_mtod_offset(pkt_burst, struct ipv4_hdr *, ether_len);
	if((!ipv4_hdr) || (ipv4_hdr->next_proto_id != FLOW_IPPROTO_TCP))
	{
		def = -1;
		goto def;
	}
	ipv4_headerlen = (ipv4_hdr->version_ihl & IPV4_HDR_IHL_MASK) << 2;
	ipv4_totlen = rte_be_to_cpu_16(ipv4_hdr->total_length);
	tcp_totlen = ipv4_totlen - ipv4_headerlen;

	tcp_headerlen = sizeof(struct tcp_hdr);
	tcp_hdr = rte_pktmbuf_mtod_offset(pkt_burst, struct tcp_hdr *, ether_len + ipv4_headerlen);
	if((!tcp_hdr) || (tcp_headerlen >= tcp_totlen))
	{
		def = -1;
		goto def;
	}
	tcp_hdr += 1;

	/*check ipv4 ddos detect*/
	if(!(tcp_hdr->tcp_flags & 0xa) && flow_ddos_module.v4detect(ipv4_hdr, tcp_hdr))
	{
		printf("It has a ipv4 ddos detect\n");
	}
	

def:
	if(def)
	{
		flow_forward_module_inside_send_pkt_default(pkt_burst);
	}
	else
	{
		/* parse http  */
		flow_ua_common_module.httphandle(pkt_burst, tcp_hdr, tcp_totlen - tcp_headerlen);
	}

	return 0;
}

static int flow_forward_module_inside_ipv6_fwd_pkts(struct rte_mbuf *pkt_burst)
{
	int def = 0;
	struct ether_hdr *eth_hdr;	
	struct ipv6_hdr *ipv6_hdr;
	struct tcp_hdr *tcp_hdr;

	int ether_len;
	int ipv6_totlen, ipv6_headerlen;
	int tcp_totlen, tcp_headerlen;

	eth_hdr = rte_pktmbuf_mtod(pkt_burst, struct ether_hdr *);
	if((!eth_hdr) || (rte_be_to_cpu_16(eth_hdr->ether_type) != ETHER_TYPE_IPv6))
	{
		def = -1;
		goto def;
	}
	ether_len = sizeof(struct ether_hdr);

	ipv6_hdr = rte_pktmbuf_mtod_offset(pkt_burst, struct ipv6_hdr *, ether_len);
	if((!ipv6_hdr) || (ipv6_hdr->proto != FLOW_IPPROTO_TCP))
	{
		def = -1;
		goto def;
	}
	ipv6_headerlen = sizeof(struct ipv6_hdr);
	ipv6_totlen = rte_be_to_cpu_16(ipv6_hdr->payload_len);
	tcp_totlen = ipv6_totlen - ipv6_headerlen;

	tcp_headerlen = sizeof(struct tcp_hdr);
	tcp_hdr = rte_pktmbuf_mtod_offset(pkt_burst, struct tcp_hdr *, ether_len + ipv6_headerlen);
	if((!tcp_hdr) || (tcp_headerlen >= tcp_totlen))
	{
		def = -1;
		goto def;
	}
	tcp_hdr += 1;

	/*check ipv6 ddos detect*/
	if(flow_ddos_module.v6detect(ipv6_hdr, tcp_hdr))
	{
		printf("It has a ipv6 ddos detect\n");
	}

def:
	if(def)
	{
		flow_forward_module_inside_send_pkt_default(pkt_burst);
	}
	else
	{
		/* parse http  */
		flow_ua_common_module.httphandle(pkt_burst, tcp_hdr, tcp_totlen - tcp_headerlen);	
	}



	return 0;
}


static int flow_forward_module_inside_forward(struct rte_mbuf *pkt_burst)
{
	uint32_t ol_flag = pkt_burst->ol_flags;
	if(ol_flag & PKT_RX_IPV4_HDR)
	{
		/*hand ipv4 packet*/
		flow_forward_module_inside_ipv4_fwd_pkts(pkt_burst);
	}
	else if(ol_flag & PKT_RX_IPV6_HDR)
	{
		/*hand ipv6 packet*/
		flow_forward_module_inside_ipv6_fwd_pkts(pkt_burst);
	}
	else
	{
		flow_forward_module_inside_send_pkt_default(pkt_burst);
	}

	return 0;
}

static int flow_forward_module_forward(int queueid)
{
	int rxsize, i, j, nb_rx;
	uint8_t idx;

	/*receive rte_mbuf*/
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	flow_config_idx_t ** prxidx = flow_config_module.getrx();
	rxsize = flow_config_module.getsize();

	for(i = 0, idx = prxidx[i]->idx; i < rxsize; i++)
	{
		nb_rx=rte_eth_rx_burst(idx, queueid, pkts_burst, MAX_PKT_BURST);
		if(nb_rx == 0)
			continue;

		/* Prefetch first packet */
		for(j=0; j<PREFETCH_OFFSET && j<nb_rx; j++) {
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j], void *));
		}

		/* Prefetch and forward already prefetched packets */
		for(j=0; j<(nb_rx - PREFETCH_OFFSET); j++) {
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j + PREFETCH_OFFSET], void *));
			flow_forward_module_inside_forward(pkts_burst[j]);
		}

		/* Forward remaining prefetched packets */
		for (; j<nb_rx; j++) {
			flow_forward_module_inside_forward(pkts_burst[j]);
		}
	}

	return 0;
}

static int flow_forward_module_send(struct rte_mbuf *pkt_burst, int portid, int queueid, int coreid)
{
	int ret, n;
        uint16_t len;
	uint32_t lcore_id;
	struct rte_mbuf ** mbuf;

	if(coreid < 0)/* default send thread */
        	lcore_id = rte_lcore_id();
	else
		lcore_id = coreid;

	ret = rte_eth_tx_burst(portid, queueid, &pkt_burst, 1);
	if(ret <= 0)
		rte_pktmbuf_free(pkt_burst);
#if 0
        len = flow_forward_global_mbuf_table[lcore_id].len;
        flow_forward_global_mbuf_table[lcore_id].m_table[len] = pkt_burst;
        len++;

	/* enough pkts to be sent */
	if (unlikely(len == SEND_PKT_BURST)) {
		n = flow_forward_global_mbuf_table[lcore_id].len;
		mbuf = flow_forward_global_mbuf_table[lcore_id].m_table;
		ret = rte_eth_tx_burst(portid, queueid, mbuf, n);
		if (unlikely(ret < n)) {
			do
			{
				rte_pktmbuf_free(mbuf[ret]);
			} while (++ret < n);
		}

		len = 0;
        }
        flow_forward_global_mbuf_table[lcore_id].len = len;
#endif

        return 0;
}

struct flow_forward_module flow_forward_module =
{
	.forward	= flow_forward_module_forward,
	.send		= flow_forward_module_send,
};



