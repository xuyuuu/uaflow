#include <unistd.h>                                                                   
#include <syslog.h>                                                                   
#include <errno.h>                                                                    
#include <string.h>                                                                   
#include <stdint.h>

#include <sys/stat.h>                                                                 
#include <sys/types.h>                                                                
#include <libxml/xmlreader.h>

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
#include <rte_malloc.h>
#include <rte_rwlock.h>

#include "../flow_core/flow_core_list.h"
#include "../flow_core/flow_core_ahocorasick.h"
#include "../flow_core/flow_core_types.h"
#include "flow_config_xml.h"
#include "flow_config_parser.h"

/* idx list */
static int flow_config_rx_idx_size;
static flow_config_idx_t * flow_config_rx_idx_list[RTE_MAX_ETHPORTS];

static int flow_config_tx_idx_size;
static flow_config_idx_t * flow_config_tx_idx_list[RTE_MAX_ETHPORTS];

/*list hook head*/
static struct list_head flow_config_item_head[FLOW_CONFIG_SLOT_SIZE];
static struct list_head flow_config_key_head[FLOW_CONFIG_SLOT_SIZE];

/* ac structure point*/
static struct ac_trie *flow_config_actrie[FLOW_LCORE_MAX_SLOT];

/*scan number*/
static uint32_t flow_config_scan_ddos_scan_number;
/*scan millisenond*/
static uint32_t flow_config_scan_ddos_scan_millisecond;

static int flow_config_module_init(void)
{
	int ret = FLOW_CONFIG_PARSER_OK, i;

	for (i = 0; i < FLOW_CONFIG_SLOT_SIZE; i++)
	{
		INIT_LIST_HEAD(&flow_config_item_head[i]);
		INIT_LIST_HEAD(&flow_config_key_head[i]);
	}

	for (i = 0; i < FLOW_LCORE_MAX_SLOT; i++)
	{
		flow_config_actrie[i] = ac_trie_create();
	}

	return ret;
}

static int rte_eth_dev_attach_self_inside_checkmac(const char *device, char *smac)
{
	int ret = 0;
	FILE *fp = NULL;
	char buff[2048] = {0}, *p1 = NULL, *p2 = NULL;
	const char *file = FLOW_CONFIG_CHECK_MACADDRESS;

	fp = fopen(file, "r");
	if(!fp)
	{
		ret = -1;
		goto out;
	}

	while(!feof(fp))
	{
		fgets(buff, sizeof(buff), fp);
		p1 = strstr(buff, "ATTR{address}==");
		p2 = strstr(buff, "NAME=");
		if(!p1 || !p2)
			continue;

		p1 += sizeof("ATTR{address}==");
		p2 += sizeof("NAME=");
		if(!strncasecmp(p1, smac, strlen(smac)) &&
			!strncasecmp(p2, device, strlen(device)))	
		{
			goto out;
		}
	}
	ret = -1;

out:
	if(fp)
		fclose(fp);

	return ret;
}


static int rte_eth_dev_attach_self(const char *device, uint8_t *idx)
{
	int nb_ports, i;
	struct ether_addr eth_addr;
	char smac[ETHER_ADDR_FMT_SIZE];

	nb_ports = rte_eth_dev_count();
	for(i = 0; i < nb_ports; i++)
	{
		memset(smac, 0x0, sizeof(smac));
		memset(&eth_addr, 0x0, sizeof(struct ether_addr));
		//rte_eth_dev_info_get(i, &dev_info);
		rte_eth_macaddr_get(i, &eth_addr);
        	ether_format_addr(smac, ETHER_ADDR_FMT_SIZE, &eth_addr);

		if(!rte_eth_dev_attach_self_inside_checkmac(device, smac))
		{
			*idx = i;
			goto next;
		}
	}
	*idx = 0;

next:
	return 0;
}

static int flow_config_module_load_inside_parse(xmlDocPtr ptr)
{
	int hash = 0, length = 0, i;
	flow_config_item_t *item = NULL;
	flow_config_key_t *key = NULL;
	flow_config_idx_t *curptr = NULL, **headptr = NULL;
	int flow_config_idx_size = 0;

	xmlNodePtr node = NULL;
	/* parse item xml */
	for(node = config_search3(ptr, "UAFlow", "Capmode", "Item"); 
		node != NULL; node = config_search_next(node, "Item"))
	{
		const char *name = config_search_attr_value(node, "name");
		const char * stype = config_search_attr_value(node, "type");
		const char *sid = config_search_attr_value(node, "ID"); 
		if (!name || !stype || !sid)
			goto again1;

		item = rte_zmalloc(NULL, sizeof(flow_config_item_t), 0);
		if(!item)
			goto again1;
		memset(item, 0x0, sizeof(*item));

		if(!strncasecmp(stype, "rx", sizeof("rx") - 1))
			item->type = FLOW_PMD_MODE_RX;
		else
			item->type = FLOW_PMD_MODE_TX;
		item->itemid = atoi(sid);
		strncpy(item->name, name, sizeof(item->name) - 1);
		item->name_length = strlen(item->name);	
		INIT_LIST_HEAD(&item->node);
		// insert item head list
		hash = item->itemid & FLOW_CONFIG_SLOT_MASK;
		list_add_tail(&item->node, &flow_config_item_head[hash]);

		// push idx list
		if(FLOW_PMD_MODE_RX == item->type)
		{
			headptr = flow_config_rx_idx_list;
			curptr = flow_config_rx_idx_list[flow_config_rx_idx_size++] =\
				rte_zmalloc(NULL, sizeof(flow_config_idx_t), 0);
			memset(curptr, 0x0, sizeof(flow_config_idx_t));
			flow_config_idx_size = flow_config_rx_idx_size;
		}
		else
		{
			headptr = flow_config_tx_idx_list;
			curptr = flow_config_tx_idx_list[flow_config_tx_idx_size++] =\
				rte_zmalloc(NULL, sizeof(flow_config_idx_t), 0);
			memset(curptr, 0x0, sizeof(flow_config_idx_t));
			flow_config_idx_size = flow_config_tx_idx_size;
		}

		/* bind dpdk devices */
		curptr->itemid = item->itemid;
		for(i = 0; i < flow_config_idx_size; i++)
		{
			if((curptr->itemid == headptr[i]->itemid) && (!headptr[i]->to))
			{
				headptr[i]->to = 1;
				rte_eth_dev_attach_self(name, &curptr->idx);
				break;
			}
		}

again1:	
		config_attr_free(name);
		config_attr_free(stype);
		config_attr_free(sid);
	}

	/* parse key xml */
	for(node = config_search3(ptr, "UAFlow", "Flow", "UAKey"); 
		node != NULL; node = config_search_next(node, "UAKey"))
	{
		const char * sid = config_search_attr_value(node,"ItemID");
		const char *skey = config_search_attr_value(node, "key");
		if (!sid || !skey)
			goto again2;

		length = sizeof(flow_config_key_t) + strlen(skey) + 1;
		key = rte_zmalloc(NULL, length, 0);
		if (!key)
			goto again2;
		memset(key, 0x0, length);
		
		key->itemid = atoi(sid);
		key->key_length = strlen(skey);	
		strncpy(key->key, skey, key->key_length);
		INIT_LIST_HEAD(&key->node);
		// insert key head list
		hash = item->itemid & FLOW_CONFIG_SLOT_MASK;
		list_add_tail(&key->node, &flow_config_key_head[hash]);

again2:
		config_attr_free(sid);
		config_attr_free(skey);
	}

	node = config_search3(ptr, "UAFlow", "Scan", "DDOS");
	if(node)
	{
		const char *number = config_search_attr_value(node, "CKNumber");
		const char *time   = config_search_attr_value(node, "CKTime");
		if(!number || !time)
		{
			flow_config_scan_ddos_scan_millisecond = FLOW_CONFIG_DDOS_SCAN_MILLISECOND_DEFAULT;
			flow_config_scan_ddos_scan_number = FLOW_CONFIG_DDOS_SCAN_NUMBER_DEFAULT;
			goto again3;
		}
		else
		{
			flow_config_scan_ddos_scan_millisecond = atoi(time);
			flow_config_scan_ddos_scan_number = atoi(number);
		}
again3:
		config_attr_free(number);
		config_attr_free(time);
	}

	return 0;
}

static int flow_config_module_load(void)
{
	int ret = FLOW_CONFIG_PARSER_OK;
	xmlDocPtr ptr = NULL;
	ptr = config_load(FLOW_CONFIG_PARSER_CONFIG_FILE);
	if (!ptr)
	{
		ret = FLOW_CONFIG_PARSER_FAILED;
		goto out;
	}
	if(flow_config_module_load_inside_parse(ptr))
	{
		ret = FLOW_CONFIG_PARSER_FAILED;
		goto out;
	}

out:
	if (ptr)
		config_free(ptr);

	return ret;
}

static flow_config_item_t *
 flow_config_module_build_inside_find_item(flow_config_key_t *key1)
{
	flow_config_item_t *item1 = NULL;
	int hash = key1->itemid & FLOW_CONFIG_SLOT_MASK;

	if(list_empty(&flow_config_item_head[hash]))	
		goto out;

	list_for_each_entry(item1, &flow_config_item_head[hash], node)
	{
		if (item1->itemid == key1->itemid)	
			goto out;
	}
	item1 = NULL;

out:
	return item1;
}

static int flow_config_module_build(void)
{
	int i = 0, j = 0, s, t;
	flow_config_key_t * key1 = NULL;
	flow_config_item_t *item1 = NULL;
	flow_config_idx_t *idx = NULL;

	AC_PATTERN_t patt;

	for(i = 0; i < FLOW_CONFIG_SLOT_SIZE; i++)
	{
		if(list_empty(&flow_config_key_head[i]))
			continue;
		list_for_each_entry(key1, &flow_config_key_head[i], node)
		{
			if( key1->key_length<=0)	
				continue;
			item1 = flow_config_module_build_inside_find_item(key1);
			if (!item1)
				continue;

			if(item1->type == FLOW_PMD_MODE_RX)
				continue;

			memset(&patt, 0x0, sizeof(patt));
			patt.ptext.astring = key1->key;
                	patt.ptext.length = key1->key_length;

                	patt.rtext.astring = NULL;
                	patt.rtext.length = 0; 
                	patt.id.u.number = ++j;
                	patt.id.type = AC_PATTID_TYPE_NUMBER;

			/*find idx*/
			for(t = 0; t < flow_config_tx_idx_size; t++)
			{
				idx = flow_config_tx_idx_list[t];	
				if ((idx->itemid == key1->itemid) && (idx->to & 0x1))				
				{
					patt.idx = idx->idx;
					goto next;
				}
			}
			patt.idx = 0; /* default device id*/
next:

			/*other private data*/
			strncpy(patt.name, item1->name, item1->name_length);
			patt.itemid = key1->itemid;
			patt.name_length = item1->name_length;

			for(s = 0; s < FLOW_LCORE_MAX_SLOT; s++)
			{
				ac_trie_add(flow_config_actrie[s], &patt, 0);
			}
		}
	}
	for(s = 0; s < FLOW_LCORE_MAX_SLOT; s++)
		ac_trie_finalize(flow_config_actrie[s]);

	return 0;
}


static int flow_config_module_search(const char *str, int lcore_id)
{
	int ret;
	const char *ua;
	AC_MATCH_t match;
	AC_TEXT_t ac_text;
	AC_PATTERN_t *pattern;

	ua = str;
	ac_text.astring = ua;
	ac_text.length = strlen(ac_text.astring);
	ac_trie_settext(flow_config_actrie[lcore_id], &ac_text, 0);
	while ((match = ac_trie_findnext(flow_config_actrie[lcore_id])).size)
	{
		pattern = &match.patterns[0];
		ret = pattern->idx;
		goto find;
	}
	ret = -1;
find:
	return ret;
}

static int flow_config_module_dump(const char *logname)
{
	int i;
	FILE *log = NULL;
	char buff[1024] = {0};

	log = fopen(logname, "w");
	if (!log)
		goto out;

	flow_config_item_t *item = NULL;
	flow_config_key_t *key = NULL;

	fwrite("[Capmode]:\n", sizeof("[Capmode]:\n") - 1, 1,log);
	for(i = 0; i < FLOW_CONFIG_SLOT_SIZE; i++)
	{
		if(list_empty(&flow_config_item_head[i]))
			continue;
		list_for_each_entry(item, &flow_config_item_head[i], node)
		{
			memset(buff, sizeof(buff), 0);
			if(item->type & 0x1)
				sprintf(buff, "TX [%s] ID:[%u]\n", item->name, item->itemid);
			else
				sprintf(buff, "RX [%s] ID:[%u]\n", item->name, item->itemid);
			fwrite(buff, strlen(buff), 1, log);
		}
	}

	fwrite("[Flow]:\n", sizeof("[Flow]:\n") - 1, 1, log);
	for(i = 0; i < FLOW_CONFIG_SLOT_SIZE; i++)
	{
		if(list_empty(&flow_config_key_head[i]))
			continue;
		list_for_each_entry(key, &flow_config_key_head[i], node)
		{
			sprintf(buff, "ID:[%u] key:[%s]\n",key->itemid, key->key);
			fwrite(buff, strlen(buff), 1, log);
		}
	}


out:
	if(log)
		fclose(log);

	return 0;
}

static void * flow_config_module_getrx(void)
{
	return flow_config_rx_idx_list;
}

static void * flow_config_module_gettx(void)
{
	return flow_config_tx_idx_list;
}

static int flow_config_module_getsize(void)
{
	return flow_config_rx_idx_size;
}

static int flow_config_module_gettime(void)
{
	return flow_config_scan_ddos_scan_millisecond;
}

static int flow_config_module_getnum(void)
{
	return flow_config_scan_ddos_scan_number;
}

struct flow_config_module flow_config_module =
{
	.init    = flow_config_module_init,
	.load    = flow_config_module_load,
	.build   = flow_config_module_build,
	.search  = flow_config_module_search,
	.getrx   = flow_config_module_getrx,
	.gettx   = flow_config_module_gettx,
	.getsize = flow_config_module_getsize,
	.gettime = flow_config_module_gettime,
	.getnum  = flow_config_module_getnum,
	.dump  	 = flow_config_module_dump
};


