/*
 * This is a driver program for the library and other static codes
 */

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include "subscribe_oran_apis.h"

volatile int exit_application = 0;
static void
sigint_handler(int signum)
{
    (void)signum;

    exit_application = 1;
}


void o_ran_app_test()
{

	char option=0;

	int rc = SR_ERR_OK;

	sr_val_t *vals;

	const char *notif_path="/o-ran-trace:trace-log-generated",*notif_node_path= "log-file-name", *notif_node_val="cs";

	do{
		printf("\n1. To send notification\n2.alarm creation \n3.user space set\n4.user space get\n5.remove an alarm\n6.exit the loop:");
		scanf("%c",&option);
		getchar();
		switch(option)
		{
			case '1':

				printf("Send notification %s\n",notif_path);	


				rc=ps5g_mplane_send_notification(oran_srv.sr_sess,notif_path,&notif_node_path,&notif_node_val,1);

				if(rc!=SR_ERR_OK)
				{
					printf("subscribe notification fun call failed\n");
				}
				break;

			case '2':

				printf("alarm creation\n");
				const char *node_val[]={"10","fm","oran-fan","MAJOR","false","2021-08-17T10:18:21Z","fault-management"};
				ps5g_mplane_raise_alarm(oran_srv.sr_sess ,(char **)node_val);	

				const char *node_val1[]={"11","fan","oran-fm","MINOR","false","2021-08-17T10:18:21Z","fault management"};
				ps5g_mplane_raise_alarm(oran_srv.sr_sess,(char **)node_val1);	


				const char *node_val2[]={"12","laa","oran-usermgmt","CRITICAL","false","2021-08-18T10:18:21Z","fault management"};
				ps5g_mplane_raise_alarm(oran_srv.sr_sess,(char **)node_val2);	

				const char *node_val3[]={"12","laa","oran-usermgmt","MAJOR","false","2021-08-18T10:18:21Z","fault management"};
				ps5g_mplane_raise_alarm(oran_srv.sr_sess,(char **)node_val3);
				break;


			case '3':

				printf("Set item\n");
				const char *xpath1="/o-ran-fan:fan-tray/fan-state[name='oran']/present-and-operating";
				const char *val="true";
				rc=ps5g_mplane_set_operational_data(oran_srv.sr_sess, xpath1,val);

				if(rc!=SR_ERR_OK)
				{	
					printf("set data failed\n");

				}
				else
				{
					printf("data set %s\n",xpath1);
				}
				break;

			case '4':
				printf("get item\n");
				rc=ps5g_mplane_get_operational_data(oran_srv.sr_sess,"/o-ran-fan:fan-tray/fan-state[name='oran']/present-and-operating",&vals);

				if(rc==SR_ERR_OK)
					printf("%s %d\n",vals->xpath,vals->data.bool_val);	

				else
					printf("get after set failed %d\n",rc);
				break;
			case '5':
				printf("2nd alarm deletion \n");
		//		const char *node_val5[]={"11","fan","oran-fm","MINOR","false","2021-08-17T10:18:21Z","fault management"};

				ps5g_mplane_remove_alarm(oran_srv.sr_sess,"fan");	
					
				break;
			case '6':

				break;
			default:
				printf("Invalid option selected %x\n",option);
				break;
		}
	}while(option!='6');
}


int main(){
    int rc;

	sr_val_t *vals;
	RuntimeConfig* config=malloc(sizeof(RuntimeConfig));
    /* Initialize the oran library */
    rc = o_ran_lib_init_ctx();
ps5g_mp_data_population(oran_srv.sr_sess,config);

    o_ran_app_test();
    /* loop until ctrl-c is pressed / SIGINT is received */
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, SIG_IGN);
    while (!exit_application) {
        sleep(100);
    }
	

    /* Unsubscribe the watch for module change */
    o_ran_lib_deinit_ctx();
    printf("Application exit requested, exiting.\n");
    return rc;
}
