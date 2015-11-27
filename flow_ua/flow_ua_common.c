#include <stdio.h>

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

#include "flow_ua_common.h"
#include "flow_http_parser.h"
#include "../flow_core/flow_core_types.h"
#include "../flow_core/flow_core_list.h"
#include "../flow_config/flow_config_parser.h"
#include "../flow_forward/flow_forward.h"
#include "../flow_forward/flow_forward_defsend.h"

static http_parser flow_ua_common_parser[FLOW_LCORE_MAX_SLOT];

static http_parser_settings flow_ua_common_parser_settings[FLOW_LCORE_MAX_SLOT];

static int flow_ua_common_module_inside_on_message_begin(http_parser * parser)
{
	flow_ua_common_node_t *node = (flow_ua_common_node_t *)parser->data;
	node->onbeg = 1;
	return 0;
}

static int flow_ua_common_module_inside_on_url(http_parser * parser, 
		const char *at, size_t length)
{
	flow_ua_common_node_t *node = (flow_ua_common_node_t *)parser->data;
	if(at && length > 0)
	{
		node->onurl = 1;
	}	
	return 0;
}

static int flow_ua_common_module_inside_on_status(http_parser *parser,
						const char *at, size_t length)
{
	return 0;
}

static int flow_ua_common_module_inside_on_header_field(http_parser *parser,
						const char *at, size_t length)
{
	flow_ua_common_node_t *node = (flow_ua_common_node_t *)parser->data;
	if(at && length > 0 && node->onbeg && node->onurl && !strncasecmp(at, 
		FLOW_UA_COMMON_PATTERN_STRING,sizeof(FLOW_UA_COMMON_PATTERN_STRING) - 1))
	{
		/*set ua copy mask*/
		node->onua = 1;
	}
	return 0;
}

static int flow_ua_common_module_inside_on_header_value(http_parser *parser,
						const char *at, size_t length)
{
	flow_ua_common_node_t *node = (flow_ua_common_node_t *)parser->data;
	if(at && length > 0 && node->ua)	
	{
		node->ua_len= length > sizeof(node->ua)-1 ? 
					sizeof(node->ua) - 1 : length;
		strncpy(node->ua, at, node->ua_len);
		/*clear ua copy mask*/
		node->onua = 0;
		/*set ua copy complete mask*/
		node->oncplt = 1;
	}

	return 0;
}

static int flow_ua_common_module_inside_on_headers_complete(http_parser *parser)
{
	return 0;
}

static int flow_ua_common_module_inside_on_body(http_parser *parser,
						const char *at, size_t length)
{
	return 0;
}

static int flow_ua_common_module_inside_on_message_complete(http_parser *parser)
{

	return 0;
}

static int flow_ua_common_module_init(void)
{
	int i;

	for(i = 0; i < FLOW_LCORE_MAX_SLOT; i++)
	{
		flow_ua_common_parser_settings[i].on_message_begin=flow_ua_common_module_inside_on_message_begin;
		flow_ua_common_parser_settings[i].on_url=flow_ua_common_module_inside_on_url;
		flow_ua_common_parser_settings[i].on_status=flow_ua_common_module_inside_on_status;
		flow_ua_common_parser_settings[i].on_message_complete=flow_ua_common_module_inside_on_message_complete;
		flow_ua_common_parser_settings[i].on_headers_complete=flow_ua_common_module_inside_on_headers_complete;
		flow_ua_common_parser_settings[i].on_header_field=flow_ua_common_module_inside_on_header_field;
		flow_ua_common_parser_settings[i].on_header_value=flow_ua_common_module_inside_on_header_value;
		flow_ua_common_parser_settings[i].on_body=flow_ua_common_module_inside_on_body;
	}
 

	return 0;
}

static int flow_ua_common_module_httphandle(struct rte_mbuf *pkt_burst, void *data, int length)
{
	int lcore_id, queueid, portid = 0, ret = 0;
	lcore_id = rte_lcore_id();
	queueid = lcore_id - 1;

	/*build a new data structure*/
	flow_ua_common_node_t node;
	memset(&node, 0x0, sizeof(flow_ua_common_node_t));
	flow_ua_common_parser[lcore_id].data = &node;
	http_parser_init(&flow_ua_common_parser[lcore_id], HTTP_REQUEST);	
	http_parser_execute(&flow_ua_common_parser[lcore_id], 
			&flow_ua_common_parser_settings[lcore_id], data, length);

	if(node.oncplt)
	{
		/*search portid for dispatching*/
		portid = flow_config_module.search(node.ua, lcore_id);	
	}

	if(portid <= 0)
	{
		/*send by default port*/
		ret = flow_forward_defsend_module.push(pkt_burst);
		if(ret < 0)
		{
			/*free send cache packets*/
			rte_pktmbuf_free(pkt_burst);
		}
	}
	else
	{
		/*send by dispatch*/
		flow_forward_module.send(pkt_burst, portid, queueid, lcore_id);	
	}

	return 0;
}

struct flow_ua_common_module flow_ua_common_module = 
{
	.init		= flow_ua_common_module_init,
	.httphandle	= flow_ua_common_module_httphandle
};
