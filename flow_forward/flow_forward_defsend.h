#ifndef flow_forward_defsend
#define flow_forward_defsend

typedef struct flow_forward_defsend_arg
{
	volatile int *runflag;
	/*lock key*/
	pthread_mutex_t lock;
	pthread_cond_t cond;

	/*tid*/
	pthread_t loopid;
}__attribute__((packed)) flow_forward_defsend_arg_t;


struct flow_forward_defsend_module
{
	int(*init)(void);
	int(*run)(volatile int *flag);
	int(*push)(struct rte_mbuf *pkt_burst);
	int(*pop)(struct rte_mbuf **pkts_burst);
	int(*setbid)(int tid);
};




extern struct flow_forward_defsend_module flow_forward_defsend_module;

#endif
