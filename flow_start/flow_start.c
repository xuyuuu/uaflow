#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>

#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "../flow_dpdk/flow_dpdk.h"
#include "../flow_core/flow_core_list.h"
#include "../flow_config/flow_config_parser.h"
#include "../flow_forward/flow_forward_defsend.h"
#include "../flow_ua/flow_ua_common.h"
#include "../flow_kni/flow_kni.h"
#include "../flow_current/flow_current.h"
#include "../flow_forward/flow_forward.h"
#include "../flow_ddos/flow_ddos.h"

#define FLOW_DEFAULT_LOGFILE "/var/log/UAFlow.log"
#define FLOW_DEFAULT_PIDFILE "/var/log/UAFlow.pid"

static volatile int flow_start_dump_configure_signum;

static void flow_usage(void)
{
	printf("Please read guide message carefully...\n\
-------------------------------------------\n\
-h show help message\n\
-v show this program version\n\
-o [-o logfile] dump configure into logfile\n\
-d run in backend[daemon]\n\
-p dump logfile[default /var/log/UAFlow.log]\n\
-------------------------------------------\n");
}

static void flow_version(void)
{
	printf("The Current VERSION of UAFlow IS [V0.1].\n");
}

static void flow_start_sighandler_USR1(int signum)
{
	if(signum == SIGUSR1)
		flow_start_dump_configure_signum = 1;	
}

static void flow_start_sighandler_INT(int signum)
{
	if (signum == SIGINT)
		flow_dpdk_core_run_flag = 1;
}

static int flow_start_siginit(void)
{
	/*signal for dump configure file*/
	signal(SIGUSR1, flow_start_sighandler_USR1);
	/*signal for lcore run switch key*/
	signal(SIGINT, flow_start_sighandler_INT);

	return 0;
}

static int flow_start_wr_pid(void)
{
	int ret;
	FILE *fp;
	char buff[128] = {0};

	fp = fopen(FLOW_DEFAULT_PIDFILE, "w+");
	if(!fp)
	{
		ret = -1;
		goto err;
	}
	sprintf(buff, "uaflow pid:[%d]\n", getpid());
	fwrite(buff, strlen(buff), 1, fp);
	ret = 0;
err:
	if(fp)
		fclose(fp);

	return ret;
}

static int flow_print(void)
{
	int ret;
	FILE *fp;
	char buff[1024] = {0};
	char commond[1024] = {0};
	int id;

	fp = fopen(FLOW_DEFAULT_PIDFILE, "r");
	if(!fp)
	{
		ret = -1;
		goto err;
	}
	fread(buff, sizeof(buff), 1, fp);
	sscanf(buff, "%*[^\[]\[%d", &id);

	/*combination commond*/
	sprintf(commond, "kill -USR1 %d", id);
	system(commond);
err:
	if(fp)
		fclose(fp);

	return ret;
}

int main(int argc, char **argv)
{
	char logname[256] = {0};
	int opt, logmark = -1, backmark = -1;
	while((opt = getopt(argc, argv, "m:hvo:p")) != -1)
	{
		switch (opt)
		{
			case 'h':
				flow_usage();
				return 0;
			case 'v':
				flow_version();
				return 0;
			case 'p':
				flow_print();
				return 0;
			case 'o':
				logmark = 0;
				strncpy(logname, optarg, sizeof(logname) - 1);
				break;
			case 'm':
				if(!strncasecmp(optarg, "back", sizeof("back") - 1))
					backmark = 0;
				break;
			default:
				break;
		}
	}

	if(!backmark)/*run in background*/
		daemon(1, 1);

	if(logmark)/*use default logfile*/
		strncpy(logname, FLOW_DEFAULT_LOGFILE, sizeof(FLOW_DEFAULT_LOGFILE) - 1);

	if(flow_start_wr_pid())
		return 0;

	flow_start_siginit();	

	flow_dpdk_module.init();
	flow_current_module.init();
	flow_ua_common_module.init();
	flow_config_module.init();
	flow_config_module.load();
	flow_config_module.build();
	flow_ddos_module.init();
	flow_forward_defsend_module.init();
#if KNI_TEST
	flow_kni_module.init();
#endif

	/*dpdk set device configure*/
	flow_dpdk_module.setconf();	

	/*start service*/
	flow_dpdk_module.handler();
#if KNI_TEST
	flow_kni_module.run(&flow_dpdk_core_run_flag);
#endif	
	flow_forward_defsend_module.run(&flow_dpdk_core_run_flag);
	
	printf("\n|**************************************************|\n\
|***       UA Flow Service Start Now !          ***|\n\
|**************************************************|\n");
	
	for(;;)
	{
		if(flow_start_dump_configure_signum)
		{
			/*dump configure file*/
			flow_start_dump_configure_signum = 0;
			flow_config_module.dump(logname);
			flow_dpdk_module.dump(logname);
		}
		if(flow_dpdk_core_run_flag)
		{
			sleep(3);
			break;
		}
		/*update time*/
		flow_current_module.update();
		usleep(1000);
	}


	return 0;
}
