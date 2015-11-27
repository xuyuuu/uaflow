#include <stdio.h>
#include <sys/time.h>

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

#include "flow_current.h"

static uint32_t flow_current_time;
static rte_rwlock_t flow_current_time_lck; 

static uint32_t flow_current_module_inside_getime(void)
{
	uint32_t tmp;
	struct timeval tv; 
	gettimeofday(&tv, 0); 
	tmp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	return tmp;
}

static int flow_current_module_init(void)
{
	/*init rwlock*/
	rte_rwlock_init(&flow_current_time_lck);

	/*init global time*/
	flow_current_time = flow_current_module_inside_getime(); 

	return 0;
}

static int flow_current_module_update(void)
{
	rte_rwlock_write_lock(&flow_current_time_lck);

	/*update current time*/
	flow_current_time = flow_current_module_inside_getime(); 

	rte_rwlock_write_unlock(&flow_current_time_lck);

	return 0; 
}

static uint32_t flow_current_module_get(void)
{
	uint32_t ret;

	rte_rwlock_read_lock(&flow_current_time_lck);
	/*flush cpu cache*/
	flow_compiler_barrier();
	ret = flow_current_time;
	rte_rwlock_read_unlock(&flow_current_time_lck);

	return ret;
}

struct flow_current_module flow_current_module = 
{
	.init		= flow_current_module_init,
	.update		= flow_current_module_update,
	.get		= flow_current_module_get
};




