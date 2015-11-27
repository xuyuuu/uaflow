#ifndef flow_kni_h
#define flow_kni_h

#define FLOW_KNI_MASTER_CORE_ID 0
#define FLOW_MAX_PACKET_SZ 2048
#define FLOW_PKT_BURST_SZ 256 

typedef struct flow_kni_arg
{
	volatile int *runflag;
	/*lock key*/
	pthread_mutex_t lock;
	pthread_cond_t cond;

	/*tid*/
	pthread_t loopid;	
	/*portid*/
	uint16_t portid;
}__attribute__((packed)) flow_kni_arg_t;

struct flow_kni_module
{
	int (*init)(void);	
	int (*run)(volatile int *flag);
};

extern struct flow_kni_module flow_kni_module;

#endif
