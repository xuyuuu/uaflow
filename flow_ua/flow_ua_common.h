#ifndef flow_ua_common_h
#define flow_ua_common_h

#define FLOW_UA_COMMON_PATTERN_STRING "User-Agent:"

typedef struct flow_ua_common_node
{
	uint32_t onbeg :1;
	uint32_t onurl :1;
	uint32_t onua  :1;
	uint32_t oncplt:1;

	char ua[1024];
	int ua_len;

}__attribute__((packed)) flow_ua_common_node_t;

struct flow_ua_common_module
{
	int(*init)(void);
	int(*httphandle)(struct rte_mbuf *pkt_burst, void *data, int length);
};


extern struct flow_ua_common_module flow_ua_common_module;

#endif
