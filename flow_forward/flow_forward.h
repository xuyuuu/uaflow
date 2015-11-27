#ifndef flow_forward_h
#define flow_forward_h

#define PREFETCH_OFFSET 3

#define SEND_PKT_SLOT 16
#define SEND_PKT_BURST 4 

typedef struct flow_forward_mbuf_table
{
          uint16_t len;
          struct rte_mbuf *m_table[SEND_PKT_SLOT];
}__rte_cache_aligned flow_forward_mbuf_table_t;



struct flow_forward_module
{
	int (*forward)(int queueid);
	int (*send)(struct rte_mbuf *pkt_burst, int portid, int queueid, int coreid);
};


extern struct flow_forward_module flow_forward_module;

#endif
