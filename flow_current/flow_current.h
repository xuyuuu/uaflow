#ifndef flow_current_h
#define flow_current_h

/*flush cpu cache*/
#define flow_compiler_barrier() do {\
        asm volatile ("" : : : "memory");\
} while(0)

struct flow_current_module
{
	int(*init)(void);
	int(*update)(void);
	uint32_t(*get)(void);
};


extern struct flow_current_module flow_current_module;

#endif
