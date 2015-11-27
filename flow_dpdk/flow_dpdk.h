#ifndef flow_dpdk_h
#define flow_dpdk_h

#define RX_PTHRESH      8 /* Default values of RX prefetch threshold reg. */
#define RX_HTHRESH      8 /* Default values of RX host threshold reg. */
#define RX_WTHRESH      4 /* Default values of RX write-back threshold reg. */

#define TX_PTHRESH      36 /* Default values of TX prefetch threshold reg. */
#define TX_HTHRESH      0  /* Default values of TX host threshold reg. */
#define TX_WTHRESH      16  /* Default values of TX write-back threshold reg. */

#define MEMPOOL_CACHE_SIZE 32 
#define FLOW_DPDK_NB_SOCKETS 8

#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512

#define RTE_MBUF_DEFAULT_DATAROOM 2048
#define RTE_MBUF_DEFAULT_BUF_SIZE \
		(RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM)

#define MAX_PKT_BURST 32

#define NB_MBUF (8192 * 20)
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

struct flow_dpdk_module
{
	int(*init)(void);
	int(*setconf)(void);
	int(*handler)(void);
	int(*getaliq)(void);
	int(*dump)(const char *);
};
extern struct flow_dpdk_module flow_dpdk_module;

extern volatile int flow_dpdk_core_run_flag;

#endif
