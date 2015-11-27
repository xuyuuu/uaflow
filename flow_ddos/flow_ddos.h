#ifndef flow_ddos_h
#define flow_ddos_h

#define FLOW_DDOS_NODE_LIST_MAX_SZ 10240 
#define FLOW_DDOS_NODE_LIST_MASK (FLOW_DDOS_NODE_LIST_MAX_SZ - 1)

#define FLOW_DDOS_EVERYNODE_ENTRY_MAX_SZ 1024 
#define FLOW_DDOS_EVERYNODE_ENTRY_MASK (FLOW_DDOS_EVERYNODE_ENTRY_MAX_SZ - 1)

#define FLOW_DDOS_MEMPOOL_EACH_ENTRY_SZ (sizeof(struct flow_ddos_node))

struct flow_ddos_entry
{
	uint16_t port;
	union
	{
		uint32_t addrv4;
		uint8_t addrv6[16];
	};
}__rte_cache_aligned;

struct flow_ddos_node
{
	/*list node*/
	struct list_head head;
	union
	{
		uint32_t addrv4;
		uint8_t addrv6[16];
	};
	uint32_t modtime;
	uint32_t n_packets;
	struct flow_ddos_entry entry[FLOW_DDOS_EVERYNODE_ENTRY_MAX_SZ];
}__rte_cache_aligned;

struct flow_ddos_module
{
	int(*init)(void);
	int(*v4detect)(struct ipv4_hdr *ipv4_hdr, struct tcp_hdr *tcp_hdr);
	int(*v6detect)(struct ipv6_hdr *ipv6_hdr, struct tcp_hdr *tcp_hdr);
};

extern struct flow_ddos_module flow_ddos_module;


#endif
