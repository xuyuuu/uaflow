#ifndef flow_config_parser_h
#define flow_config_parser_h

#define FLOW_CONFIG_DDOS_SCAN_NUMBER_DEFAULT 1000
#define FLOW_CONFIG_DDOS_SCAN_MILLISECOND_DEFAULT 5000

#define FLOW_CONFIG_SLOT_SIZE 4096
#define FLOW_CONFIG_SLOT_MASK (FLOW_CONFIG_SLOT_SIZE - 1)

#define FLOW_CONFIG_PARSER_OK 		0
#define FLOW_CONFIG_PARSER_FAILED	-1

#define FLOW_CONFIG_PARSER_CONFIG_FILE "./UAFlow.conf"

#define FLOW_CONFIG_CHECK_MACADDRESS "/etc/udev/rules.d/70-persistent-net.rules"

typedef struct flow_config_key
{
	/*list hook*/	
	struct list_head node;

	uint32_t itemid;
	int key_length;
	char key[0];

}__attribute__((packed)) flow_config_key_t;

typedef struct flow_config_item
{
	/*list hook*/
	struct list_head node;	

	char name[256];
	int name_length;

	uint8_t type:1;	
	uint32_t itemid;

}__attribute__((packed)) flow_config_item_t;

typedef struct flow_config_idx
{
	uint8_t to:1;
	uint8_t idx;
	uint32_t itemid;
}__attribute__((packed)) flow_config_idx_t;


struct flow_config_module
{
	int (*init)(void);
	int (*load)(void);
	int (*build)(void);
	int (*search)(const char *str, int lcore_id);
	void*(*getrx)(void);
	void*(*gettx)(void);
	int (*getsize)(void);
	int (*gettime)(void);
	int (*getnum)(void);
	int (*dump)(const char *logname);
};


extern struct flow_config_module flow_config_module;

#endif
