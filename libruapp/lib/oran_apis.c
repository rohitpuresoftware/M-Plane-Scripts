#include <stdio.h>
#include <malloc.h>

#include "oran_apis.h"
#define CONFIG_PRINT (1)
/**
 * @This is a temp function, just to validate values
 */

/*
variables for file uplaod operation

*/
char file_ul_remote_path[MAX_PATH_LEN];
char file_ul_remote_path_cp[MAX_PATH_LEN]={0};
char file_ul_error[MAX_PATH_LEN]={0};
char *file_ul_remote_path_substring[5];
char *file_ul_local_path={NULL};
ssh_session file_ul_session;
const char *file_ul_path="/o-ran-file-management:file-upload-notification";
const char *file_ul_node_path[]={"local-logical-file-path","remote-file-path","status","reject-reason"};
char *file_ul_password={NULL};	

/*
variables for file download operation
*/

ssh_session file_dwn_session;
char *file_dwn_remote_path_substr[5]={NULL};
char file_dwn_error[MAX_PATH_LEN]={0};
char *file_dwn_password={NULL};
char *file_dwn_local_path={NULL};
char file_dwn_remote_path[MAX_PATH_LEN]={0};
char temp[MAX_PATH_LEN]={0};
const char *f_dwn_path="/o-ran-file-management:file-download-event";
int file_download_flag=0;

int file_upload_flag=0;
/*
   variables for software-downalod */

char sw_dl_error[MAX_PATH_LEN]={0};
ssh_session sw_dl_session;
const char *sw_dl_path="/o-ran-software-management:download-event";
const char *sw_dl_node_path[]={"file-name","status","error-message"};
char *sw_dl_remote_path_substring[5]={NULL};
char *sw_dl_password=NULL;
char sw_dl_remote_path[MAX_PATH_LEN]={0};
int software_dl_flag=0;

	void
print_val(const notification_val_t *value)
{
	if (NULL == value) {
		return;
	}

	printf("%s ", value->xpath);

	switch (value->type) {
		case CONTAINER_T:
		case CONTAINER_PRESENCE_T:
			printf("(container)");
			break;
		case LIST_T:
			printf("(list instance)");
			break;
		case STRING_T:
			printf("= %s", value->data.string_val);
			break;
		case BOOL_T:
			printf("= %s", value->data.bool_val ? "true" : "false");
			break;
		case DECIMAL64_T:
			printf("= %g", value->data.decimal64_val);
			break;
		case INT8_T:
			printf("= %" PRId8, value->data.int8_val);
			break;
		case INT16_T:
			printf("= %" PRId16, value->data.int16_val);
			break;
		case INT32_T:
			printf("= %" PRId32, value->data.int32_val);
			break;
		case INT64_T:
			printf("= %" PRId64, value->data.int64_val);
			break;
		case UINT8_T:
			printf("= %" PRIu8, value->data.uint8_val);
			break;
		case UINT16_T:
			printf("= %" PRIu16, value->data.uint16_val);
			break;
		case UINT32_T:
			printf("= %" PRIu32, value->data.uint32_val);
			break;
		case UINT64_T:
			printf("= %" PRIu64, value->data.uint64_val);
			break;
		case IDENTITYREF_T:
			printf("= %s", value->data.identityref_val);
			break;
		case INSTANCEID_T:
			printf("= %s", value->data.instanceid_val);
			break;
		case BITS_T:
			printf("= %s", value->data.bits_val);
			break;
		case BINARY_T:
			printf("= %s", value->data.binary_val);
			break;
		case ENUM_T:
			printf("= %s", value->data.enum_val);
			break;
		case LEAF_EMPTY_T:
			printf("(empty leaf)");
			break;
		default:
			printf("(unprintable)");
			break;
	}

	switch (value->type) {
		case UNKNOWN_T:
		case CONTAINER_T:
		case CONTAINER_PRESENCE_T:
		case LIST_T:
		case LEAF_EMPTY_T:
			printf("\n");
			break;
		default:
			printf("%s\n", value->dflt ? " [default]" : "");
			break;
	}
}

/**
 * @This is a temp function, just to validate values
 */
	void
print_change(notification_change_oper_t op,
		notification_val_t *old_val,
		notification_val_t *new_val)
{
	switch (op) {
		case OP_CREATED:
			printf("CREATED: ");
			print_val(new_val);
			break;
		case OP_DELETED:
			printf("DELETED: ");
			print_val(old_val);
			break;
		case OP_MODIFIED:
			printf("MODIFIED: ");
			print_val(old_val);
			printf("to ");
			print_val(new_val);
			break;
		case OP_MOVED:
			printf("MOVED: %s\n", new_val->xpath);
			break;
	}
}


	int
ps5g_mplane_change_notification(modified_data_t *in, int count)
{
	int i = 0;
	printf("Count: %d\n", count);
	for(i = 0; i < count; i++)
	{
		print_change(in[i].oper, in[i].old_val, in[i].new_val);
	}
	return 0;
}

 
void *software_download_thread(void *arg)
{
	int rc=0;
	while(1)
	{
		if(software_dl_flag)
		{
			sw_dl_session=ps5g_mp_connect_ssh(sw_dl_remote_path_substring[1],NULL,0,sw_dl_password,&sw_dl_error[0]);	
			if(sw_dl_session==NULL){
			
				const char *node_val[]={sw_dl_remote_path,"AUTHENTICATION_ERROR",sw_dl_error};
				ps5g_mplane_send_notification(oran_srv.sr_sess,sw_dl_path,sw_dl_node_path,node_val,3);
			}
			else{
			
			rc=ps5g_mp_download_from_remote(sw_dl_session,SW_MGMT_PATH,sw_dl_remote_path,&sw_dl_error[0]);
			if(rc!=1){
				const char *node_val[]={strrchr(sw_dl_remote_path,'/')+1,"COMPLETED"};

				ps5g_mplane_send_notification(oran_srv.sr_sess,sw_dl_path,sw_dl_node_path,node_val,2);
			}
			else{
				const char *node_val[]={sw_dl_remote_path,"FILE_NOT_FOUND",sw_dl_error};

				ps5g_mplane_send_notification(oran_srv.sr_sess,sw_dl_path,sw_dl_node_path,node_val,4);
			}
			 }
			ssh_disconnect(sw_dl_session);
			ssh_free(sw_dl_session);
			free(sw_dl_remote_path_substring[1]);
			free(sw_dl_password);
			software_dl_flag=0;
		}
		else{
			usleep(1000);
		}
	}
}

int
ps5g_mplane_software_download(ru_sw_pkg_in_t *in,
		ru_sw_pkg_out_t **out)
{
	int rc = 0;
	ru_sw_pkg_out_t *output = NULL;

	printf("URI:%s\n", in->sw_download_in.remote_file_path);

	//	printf("password :%s\n",in->sw_download_in.credentials.cred_password.password._password);
	//	printf("Filling output content will be sent back to NMS\n");


	char temp[MAX_PATH_LEN]={0};
	sprintf(temp,"%s",strrchr(in->sw_download_in.remote_file_path,':')+1);

	//	printf("port %s path %s\n", temp,strchr(temp,'/'));

	sprintf(sw_dl_remote_path,"%s",strchr(temp,'/'));

	const char s[3] = ":/";
	int i=0; 

	//	strcpy(temp,in->sw_download_in.remote_file_path);

	sw_dl_remote_path_substring[i++]=strtok(in->sw_download_in.remote_file_path,s);

	//to extract user name sw_dl_remote_path_substring[1]
	while( sw_dl_remote_path_substring[i-1] != NULL ) {
		//	  printf( " %s\n", sw_dl_remote_path_substring[i-1] );

		sw_dl_remote_path_substring[i++] = strtok(NULL, s);
		if(i>1)
			break;
	}

	sw_dl_session=ps5g_mp_connect_ssh(sw_dl_remote_path_substring[1],NULL,0,in->sw_download_in.credentials.cred_password.password._password,&sw_dl_error[0]);	
	

	output = malloc(sizeof(ru_sw_pkg_out_t));
	output->type = in->type;

	if(sw_dl_session==NULL){
		output->sw_download_out.status = FAILURE;
		output->sw_download_out. error_message= strdup(sw_dl_error);
		goto end;
	}

	rc=ps5g_mp_download_accept(sw_dl_session,SW_MGMT_PATH,sw_dl_remote_path,&sw_dl_error[0]); 
	/* assume start process failed */
	if(rc == 1){
		output->sw_download_out.status = FAILED;
		//	output->sw_download_out.error_message = strdup("Failed to start software download");
		output->sw_download_out.error_message = strdup(sw_dl_error);
		goto end;	
	}
	else{
		output->sw_download_out.status = STARTED;
		sw_dl_remote_path_substring[1]=strdup(sw_dl_remote_path_substring[1]);
		sw_dl_password=strdup(in->sw_download_in.credentials.cred_password.password._password);	
		software_dl_flag=1;


		}

	
	ssh_disconnect(sw_dl_session);
	ssh_free(sw_dl_session);
end:
	*out = output;
	return 0;
}

	int
ps5g_mplane_software_install(ru_sw_pkg_in_t *in,
		ru_sw_pkg_out_t **out)
{
	int rc = 0;
	ru_sw_pkg_out_t *output = NULL;

	printf("Install from slot: %s\n", in->sw_install_in.slot_name);

	output = malloc(sizeof(ru_sw_pkg_out_t));
	output->type = in->type;

	/* assume start process failed */
	rc = -1;
	if(rc == -1)
	{
		output->sw_install_out.status = FAILED;
		output->sw_install_out.error_message = strdup("Failed to start software installation");
	}
	else
	{
		output->sw_install_out.status = STARTED;
	}
	*out = output;
	return 0;
}

	int
ps5g_mplane_software_activate(ru_sw_pkg_in_t *in,
		ru_sw_pkg_out_t **out)
{
	int rc = 0;
	ru_sw_pkg_out_t *output = NULL;

	printf("Slot-name: %s\n", in->sw_activate_in.slot_name);

	output = malloc(sizeof(ru_sw_pkg_out_t));
	output->type = in->type;

	/* assume start process failed */
	rc = -1;
	if(rc == -1)
	{
		output->sw_activate_out.status = FAILED;
		output->sw_activate_out.error_message = strdup("Failed to start software activation");
	}
	else
	{
		output->sw_activate_out.status = STARTED;
	}
	*out = output;
	return 0;
}


void *file_upload_thread(void *arg)
{
	while(1)
	{
		if(file_upload_flag)
		{
			int rc=SUCCESS;

			file_ul_session=ps5g_mp_connect_ssh(file_ul_remote_path_substring[1],NULL,0,file_ul_password,&file_ul_error[0]);	

			if(file_ul_session==NULL){

				const char *node_val[]={file_ul_local_path,file_ul_remote_path,"FAILURE",file_ul_error};
				ps5g_mplane_send_notification(oran_srv.sr_sess ,file_ul_path,file_ul_node_path,node_val,4);
			}
			else
			{			

				rc=ps5g_mp_upload_to_remote(file_ul_session,file_ul_local_path,file_ul_remote_path,file_ul_error);

				if(rc!=1)
				{
					const char *node_val[]={file_ul_local_path,file_ul_remote_path_cp,"SUCCESS"};
					ps5g_mplane_send_notification(oran_srv.sr_sess,file_ul_path,file_ul_node_path,node_val,3);

				}	
				else
				{
					const char *node_val[]={file_ul_local_path,file_ul_remote_path,"FAILURE",file_ul_error};
					ps5g_mplane_send_notification(oran_srv.sr_sess ,file_ul_path,file_ul_node_path,node_val,4);

				}			

				ssh_disconnect(file_ul_session);
				ssh_free(file_ul_session);
			}	
			free(file_ul_password);
			free(file_ul_local_path);
			free(file_ul_remote_path_substring[1]);
			printf("File upload is done\n");
			file_upload_flag=0;
		}	
		else
			usleep(1000);
	}


}
	int
ps5g_mplane_file_upload(ru_file_mgmt_in_t *in,
		ru_file_mgmt_out_t **out)
{
	int rc = SUCCESS;
	ru_file_mgmt_out_t *output;

	printf("Local PATH: %s\n", in->file_upload_in.path.local_logical_file_path);
	printf("Remote PATH: %s\n", in->file_upload_in.path.remote_file_path);
	file_ul_password=strdup(in->file_upload_in.credentials.cred_password.password._password);
	file_ul_local_path=strdup(in->file_upload_in.path.local_logical_file_path);
	printf("password :%s\n",in->file_upload_in.credentials.cred_password.password._password);

	//	char *file_ul_remote_path_cp=strrchr(in->file_upload_in.path.remote_file_path,':')+1;
	//	printf("port %s path %s\n", file_ul_remote_path_cp,strchr(file_ul_remote_path_cp,'/'));

	const char s[] = ":/";
	int i=0; 
	strcpy(file_ul_remote_path_cp,in->file_upload_in.path.remote_file_path);

	file_ul_remote_path_substring[i++]=strtok(in->file_upload_in.path.remote_file_path,s);


	while( file_ul_remote_path_substring[i-1] != NULL ) {
		//		 printf( "-- %s\n", file_ul_remote_path_substring[i-1] );

		file_ul_remote_path_substring[i++] = strtok(NULL, s);
	}

	file_ul_remote_path_substring[1]=strdup(file_ul_remote_path_substring[1]);


	sprintf(file_ul_remote_path,"%s%s",file_ul_remote_path_substring[3],strrchr(in->file_upload_in.path.local_logical_file_path,'/'));

	printf("remote_path %s\n",file_ul_remote_path);

	file_ul_session=ps5g_mp_connect_ssh(file_ul_remote_path_substring[1],NULL,0,in->file_upload_in.credentials.cred_password.password._password,&file_ul_error[0]);	

	output = malloc(sizeof(ru_file_mgmt_out_t));
	output->type = in->type;

	if(file_ul_session==NULL){
		output->file_upload_out.reject_reason = strdup(file_ul_error);

		output->file_upload_out.status = FAILURE;
		goto end;	
	}	
	rc=ps5g_mp_file_upload_accept(file_ul_session,in->file_upload_in.path.local_logical_file_path,file_ul_remote_path,&file_ul_error[0]);


	/* assume start process failed */
	if(rc == 1)
	{

		output->file_upload_out.status = FAILURE;
		output->file_upload_out.reject_reason = strdup(file_ul_error);
		goto end;
	}
	else
	{
		output->file_upload_out.status = SUCCESS;
		file_upload_flag=1;
	}
	ssh_disconnect(file_ul_session);
	ssh_free(file_ul_session);
end:
	*out = output;
	return 0;
}

void remove_star(char *str)
{
	int len = strlen(str),i=0,j=0;

	for(i = 0; i < len; i++)
	{
		if(str[i] == '*')
		{
			for(j = i; j < len; j++)
			{
				str[j] = str[j + 1]; 
			}
			len--;
			i--;    
		}
	}    
}

	int
ps5g_mplane_retrieve_file_list(ru_file_mgmt_in_t *in,
		ru_file_mgmt_out_t **out)
{
	ru_file_mgmt_out_t *output;
	char **file_list = NULL;

	DIR *d;	
	struct dirent *dir;	
	int no_of_files=0;
	struct stat st;
	printf("Logical PATH: %s\n", in->file_retrieve_in.logical_path);
	printf("File Filter: %s\n", in->file_retrieve_in.file_name_filter);

	output = malloc(sizeof(ru_file_mgmt_out_t));
	output->type = in->type;

	stat(in->file_retrieve_in.logical_path,&st);
#if 1	

	/*check for Directory or not */	

	if(S_ISDIR(st.st_mode) )
	{
		/*if logical path is directory*/		
		d=opendir(in->file_retrieve_in.logical_path);

		if(d)
		{

			while((dir=readdir(d))!=NULL)
			{			
				if(dir->d_type == DT_REG)
				{


					if(strchr(in->file_retrieve_in.file_name_filter,'*')!=NULL)
					{	
						remove_star(&in->file_retrieve_in.file_name_filter[0]);

					}

					if(strstr(dir->d_name,in->file_retrieve_in.file_name_filter)!=NULL)	
					{				
						file_list=realloc(file_list,sizeof(char *)*( no_of_files+1));

						file_list[no_of_files]=malloc(strlen(dir->d_name)+1);
						file_list[no_of_files++]=strdup(dir->d_name);


						//printf("%s\n",file_list[no_of_files-1]);	
					}
				}					

			}

			file_list[no_of_files] = NULL;
			output->file_retrieve_out.status = SUCCESS;
		}
		output->file_retrieve_out.file_list = file_list;

	}
	else{
		/*if logical path is not a directory*/

		output->file_retrieve_out.status = FAILURE;
		output->file_retrieve_out.reject_reason = strdup("Failed retrieve file list");
	}

#endif
	*out = output;

	return 0;
}

void* file_download_thread(void *arg)
{
	while(1)
	{
		//				printf("file_download_flag %d\n",file_download_flag);
		if(file_download_flag)
		{

			//	printf("password :%s %d** %s %s\n",password,__LINE__,file_dwn_remote_path,temp);
			int rc = SUCCESS;

			file_dwn_session=ps5g_mp_connect_ssh(file_dwn_remote_path_substr[1],NULL,0,file_dwn_password,&file_dwn_error[0]);

			if(file_dwn_session==NULL){

				sprintf(file_dwn_error,"%s",ssh_get_error(file_dwn_session));
				const char *node_val[]={temp,"FAILURE",file_dwn_error};
				ps5g_mplane_send_notification(oran_srv.sr_sess,f_dwn_path,file_ul_node_path,node_val,4);
			}

			rc=ps5g_mp_download_from_remote(file_dwn_session,file_dwn_local_path,file_dwn_remote_path,&file_dwn_error[0]);
			if(rc!=1)	 
			{

				const char *node_val[]={file_dwn_local_path,temp,"SUCCESS"};

				ps5g_mplane_send_notification(oran_srv.sr_sess,f_dwn_path,file_ul_node_path,node_val,3);
			}
			else
			{
				const char *node_val[]={file_dwn_local_path,temp,"FAILURE",file_dwn_error};

				ps5g_mplane_send_notification(oran_srv.sr_sess,f_dwn_path,file_ul_node_path,node_val,9);

			}
			ssh_disconnect(file_dwn_session);
			ssh_free(file_dwn_session);
			free(file_dwn_password);
			free(file_dwn_local_path);
			file_download_flag=0;
			printf("file-downlaod completed\n");
		}
		else
			usleep(1000);
	}
	return 0;
}


	int
ps5g_mplane_file_download(ru_file_mgmt_in_t *in,
		ru_file_mgmt_out_t **out)
{
	int rc = SUCCESS;
	ru_file_mgmt_out_t *output;
	printf("file-download rpc hit\n");
	printf("Local PATH: %s\n", in->file_download_in.path.local_logical_file_path);
	printf("Remote PATH: %s\n", in->file_download_in.path.remote_file_path);
	file_dwn_password=strdup(in->file_download_in.credentials.cred_password.password._password);
	file_dwn_local_path=strdup(in->file_download_in.path.local_logical_file_path);

	//printf("password :%s\n",in->file_download_in.credentials.cred_password.password._password);


	sprintf(temp,"%s",strrchr(in->file_download_in.path.remote_file_path,':')+1);

	sprintf(file_dwn_remote_path,"%s",strchr(temp,'/'));

	//	printf("file_dwn_remote_path %s %d\n",file_dwn_remote_path,__LINE__);

	const char s[3] = ":/";
	int i=0; 

	strcpy(temp,in->file_download_in.path.remote_file_path);

	file_dwn_remote_path_substr[i++]=strtok(in->file_download_in.path.remote_file_path,s);

	while( file_dwn_remote_path_substr[i-1] != NULL ) {
		//	  printf( " %s\n", file_dwn_remote_path_substr[i-1] );

		file_dwn_remote_path_substr[i++] = strtok(NULL, s);
		if(i>1)
			break;
	}

	file_dwn_remote_path_substr[1]=strdup(file_dwn_remote_path_substr[1]);
	file_dwn_session=ps5g_mp_connect_ssh(file_dwn_remote_path_substr[1],NULL,0,in->file_download_in.credentials.cred_password.password._password,&file_dwn_error[0]);	

	output = malloc(sizeof(ru_file_mgmt_out_t));
	output->type = in->type;

	if(file_dwn_session==NULL){
		output->file_download_out.status = FAILURE;
		output->file_download_out.reject_reason = strdup(file_dwn_error);
		goto end;
	}

	rc=ps5g_mp_download_accept(file_dwn_session,in->file_download_in.path.local_logical_file_path,file_dwn_remote_path,&file_dwn_error[0]); 

	/* assume start process failed */
	//	rc = -1;
	if(rc == 1)
	{
		output->file_download_out.status = FAILURE;
		output->file_download_out.reject_reason = strdup(file_dwn_error);
		goto end;
	}
	else
	{
		output->file_download_out.status = SUCCESS;

		file_download_flag=1;
	}

	ssh_disconnect(file_dwn_session);
	ssh_free(file_dwn_session);
end:
	*out = output;
	return 0;
}

/*******to create file pointer in the format PS_trace_dd_mm_yyyy_hh_mm**********/

FILE *create_trace_file(char *filename)
{

	time_t t = time(NULL);

	struct tm *tm = localtime(&t);

	char s[64]={0};

	strftime(s,strlen("DD_MMM_YYYY_HH:MM:SS")+1,"%d_%b_%Y_%H_%M_%S",tm);

	sprintf(filename,"/home/trace/ps_trace_%s.txt",s);

	FILE *fp=fopen(filename,"w+");
	return fp;  
}

/*******write trace_log_to file**********/

int write_trace_log(FILE *fp)
{
	char trace_log_data[16384]={0};
	char temp[5];

	int rc=0;
	int i=1;

	for(;(i%4096!=0);i++)
	{
		sprintf(temp,"%02x\n",i);
		strcat(trace_log_data,temp);

	}
	i++;

	int trace_log_size=strlen(trace_log_data);
	rc=fwrite(trace_log_data,trace_log_size,1,fp);	

	fclose(fp);

	if(rc!=trace_log_size)	
	{
		goto error;
	}

	return 0;

error:
	return -1;	
}

/******trace start*********/

	int
ps5g_mp_trace_start(ru_trace_switch_in_t *in,
		ru_trace_out_t **out)
{	
	ru_trace_out_t *output = NULL;
	printf("Trace flag enabled \n");


	output=malloc(sizeof(ru_trace_switch_in_t));
//	printf("Command Type: %d\n", in->type);

#if 1
//	if(atoi(MemForTrace)==START)	/*check whether trace running or nor**/
	if(trace_start==1)
	{		
		output->status=FAILURE;
		output->reject_reason=strdup("Trace is already collecting");

		printf("Trace is already collecting\n");

	}
	else
	{
		output->status=SUCCESS;
		trace_start=1;
#if 0
		sprintf(MemForTrace,"%d",START);

		pid_t pid=fork();
		if(pid==0)
		{
			char stop=0;	

			while(1)
			{

				stop=atoi(MemForTrace);		

				FILE *fp;
				char filename[MAX_PATH_LEN]={0};				

				fp=create_trace_file(&filename[0]); //create a file with name format home/trace/ps_trace_DD_MMM_YYYY_hh_mm_ss.txt 

				write_trace_log(fp);		//creating dummy trace file 
				const char *node_path[]={"log-file-name","is-notification-last"};

				char is_last[25];	


				/***checking whether last file or not********/	

				(stop!=STOP)?sprintf(is_last,"%s","false"):sprintf(is_last,"%s","true");


				const char *node_val[]={filename,is_last};

				ps5g_mplane_send_notification(in->session,"/o-ran-trace:trace-log-generated",node_path,node_val,2);	
				if(stop==STOP)
				{
					break;	

				}	
				sleep(1);
			}

			printf("sent last log notification\n");
			exit(0);	
		}
#endif
	}
#endif
	*out=output;	
	return SR_ERR_OK;
}

/*******trace stop*****************/

	int
ps5g_mp_trace_stop(ru_trace_switch_in_t *in,
		ru_trace_out_t **out)
{
	ru_trace_out_t *output = NULL;
//	int stop=STOP;
//	stop=atoi(MemForTrace);	
	output=malloc(sizeof(ru_trace_switch_in_t));

//	printf("in ps5g_mp_trace_stop \n");
	if(trace_start==0){

		output->status=FAILURE;
		output->reject_reason=strdup("Trace is not running");
		printf("Trace is not running , trace stop failed\n");	

	}

	else{
//		sprintf(MemForTrace,"%d",STOP);		
		output->status=SUCCESS;
		trace_start=0;
		printf("Trace stopped \n");
	}

	*out=output;	
	return 0;


}



	int
ps5g_mplane_start_ruapp(ruapp_switch_in_t *in,
		ruapp_switch_out_t **out)
{
	ruapp_switch_out_t *output = NULL;
	printf("Command Type: %d\n", in->type);
	printf("Filling output content-- %lu\n",sizeof(ruapp_switch_out_t));

	output = malloc(sizeof(ruapp_switch_out_t));
	output->type = in->type;
	output->ruapp_start_out.status = -1;
	if (output->ruapp_start_out.status != 0)
	{
		output->ruapp_start_out.error_message = strdup("Failed to start");
	}
	*out = output;
	return 0;
}
	int 
ps5g_mplane_get_running_data(sr_session_ctx_t *session,const char *xpath,  sr_val_t **vals)
{
	/* switch to running data store */	
	sr_datastore_t ds_org=sr_session_get_ds(session);	//get current running data_store
	sr_datastore_t ds;
	ds =SR_DS_RUNNING;
	sr_session_switch_ds(session, ds);
	int c=SR_ERR_OK;
	c=sr_get_item(session,xpath,0,vals);
	if(c!=SR_ERR_OK)
		printf("get item failed for %s with %d\n",xpath,c);
	sr_session_switch_ds(session,ds_org);
	return c;
}
	int 
ps5g_mplane_set_running_data(sr_session_ctx_t *session,const char *xpath,const char *val)
{
	int rc = SR_ERR_OK;
	sr_datastore_t ds;
	sr_datastore_t ds_org=sr_session_get_ds(session);	//get current running data_store
	ds =SR_DS_RUNNING;//SR_DS_OPERATIONAL;// SR_DS_RUNNING;
	sr_session_switch_ds(session, ds);

	rc=sr_set_item_str(session, xpath, val, NULL, 0);

	if(rc!=SR_ERR_OK)
		printf("setting value failed\n");	

	rc=sr_apply_changes(session, 0);
	if(rc!=SR_ERR_OK)
		printf("setting value failed\n");	
	sr_session_switch_ds(session,ds_org);

	return rc;
}
	int 
ps5g_mplane_get_operational_data(sr_session_ctx_t *session,const char *xpath,  sr_val_t **vals)
{
	/* switch to running data store */	
	sr_datastore_t ds_org=sr_session_get_ds(session);	//get current data_store
	sr_datastore_t ds;
	ds =SR_DS_OPERATIONAL;
	sr_session_switch_ds(session, ds);
	int c=SR_ERR_OK;
	c=sr_get_item(session,xpath,0,vals);
	sr_session_switch_ds(session,ds_org);
	return c;
}
	int 
ps5g_mplane_set_operational_data(sr_session_ctx_t *session,const char *xpath,const char *val)
{
	int rc = SR_ERR_OK;
	sr_datastore_t ds;
	sr_datastore_t ds_org=sr_session_get_ds(session);	//get current data_store
	ds =SR_DS_OPERATIONAL;// SR_DS_RUNNING;
	sr_session_switch_ds(session, ds);

	rc=sr_set_item_str(session, xpath, val, NULL, 0);

	if(rc!=SR_ERR_OK)
		printf("setting value failed\n");	

	rc=sr_apply_changes(session, 0);
	if(rc!=SR_ERR_OK)
		printf("setting value failed\n");	
	sr_session_switch_ds(session,ds_org);

	return rc;
}

	int 
ps5g_mplane_send_notifications(sr_session_ctx_t *session,const char *path,const char *node_path,const char *node_val)
{

	int rc = SR_ERR_OK;
	struct lyd_node *notif = NULL;
	const struct ly_ctx *ctx;	

	ctx = sr_get_context(sr_session_get_connection(session));	

	rc = lyd_new_path(NULL, ctx, path, NULL, 0,&notif);
	if (rc!=LY_SUCCESS) {
		printf("Creating notification \"%s\" failed %d.\n", path,rc);
		goto cleanup;
	}
	if(node_path!=NULL)
		/* add the input value */
		if (node_path) {
			if (lyd_new_path(notif, NULL, node_path, (void *)node_val, 0, 0)!=LY_SUCCESS) 
			{
				printf("Creating value \"%s\" failed.\n", node_path);
				goto cleanup;
			}
		} 	
	/* send the notification */
	rc=sr_event_notif_send_tree(session, notif,0,0);
	if (rc != SR_ERR_OK) {
		printf("event notif_send_failed");
		goto cleanup;
	}

	//sr_event_notif_subscribe(oran_srv.sr_sess, mod_name, xpath, 0, 0, notif_cb, NULL, 0, &subscription);
cleanup:
	lyd_free_all(notif);
	return rc;
}

/*********to send notification*******/
	int 
ps5g_mplane_send_notification(sr_session_ctx_t *session,const char *path,const char *node_path[],const char* node_val[],int cnt)
{
	int rc = SR_ERR_OK,loop_cnt=0;
	struct lyd_node *notif = NULL;
	const struct ly_ctx *ctx;	
	ctx = sr_get_context(sr_session_get_connection(session));

	rc= lyd_new_path(NULL, ctx, path, NULL, 0, &notif);
	if (rc!=SR_ERR_OK) {
		printf("Creating notification \"%s\" failed with return value %d \n", path,rc);
		goto cleanup;
	}

	for(loop_cnt=0;loop_cnt<cnt;loop_cnt++)
	{		
		/* add the input value */
		if (node_path[loop_cnt]) {
			//		printf("%s\n",node_val[loop_cnt]);
			if (lyd_new_path(notif, NULL, node_path[loop_cnt], (void *)node_val[loop_cnt], 0, 0)!=LY_SUCCESS) 
			{
				printf("Creating value \"%s\" failed.\n", node_path[loop_cnt]);
				goto cleanup;
			}
		} 	
	}
	/* send the notification */
	rc=sr_event_notif_send_tree(session, notif,0,0);
	if (rc != SR_ERR_OK) {
		printf("event notif_send_failed");
		goto cleanup;
	}

cleanup:
	lyd_free_all(notif);
	return rc;
}
	int 
ps5g_mplane_raise_alarm(sr_session_ctx_t *session,char *node_val[7])
{
	printf("In raise_alarm\n");
	int rc =0 ;

		Alarm* chk =AlarmListHead;

		while(chk!=NULL){
			/* to eliminate new alarm with severity "WARNING"  chapter 8 , 2nd para 1st line */
			if(strcmp(node_val[2],"WARNING")==0||(((strcmp(chk->fault_id,node_val[0])==0) && 
			(strcmp(chk->fault_source,node_val[1])==0) && (strcmp(chk->fault_severity,node_val[2])==0)))){
				return 0;
			}
			/*to to detect duplicate alarm*/
			else if((strcmp(chk->fault_id,node_val[0])==0) && (strcmp(chk->fault_source,node_val[1])==0) && 
				(strcmp(chk->name,node_val[2])==0) && (strcmp(chk->fault_severity,node_val[3])==0) && 
				(strcmp(chk->is_cleared,node_val[4])==0) && (strcmp(chk->fault_text,node_val[6])==0)){

				printf("duplicate alarm \n");
				return 0;	
			}			
			/*to detect change inseverity 8.3 */
			else if((strcmp(chk->fault_id,node_val[0])==0) && (strcmp(chk->fault_source,node_val[1])==0) &&
				 (strcmp(chk->fault_severity,node_val[2])!=0) && (strcmp(chk->is_cleared,"true")!=0)){
				
				chk->is_cleared = strdup("true");
			}
//			else if((strcmp(chk->fault_id,node_val[0])==0) && (strcmp(chk->fault_source,node_val[1])==0) && 
//				(strcmp(chk->fault_severity,node_val[2])!=0) && (strcmp(chk->is_cleared,"true")==0)){
//
//				chk->is_cleared = strdup("false");
//			}
			chk = chk->next;
		}
	Alarm* NewAlarm = (struct alarm*)malloc(sizeof(struct alarm)) ;

	NewAlarm->fault_id 		= strdup(node_val[0]);
	NewAlarm->fault_source		= strdup(node_val[1]);
	NewAlarm->name			= strdup(node_val[2]);
	NewAlarm->fault_severity	= strdup(node_val[3]);
	NewAlarm->is_cleared 		= strdup(node_val[4]);
	NewAlarm->event_time 		= strdup(node_val[5]);
	NewAlarm->fault_text 		= strdup(node_val[6]);
	NewAlarm->next=NULL;


	if(AlarmListHead==NULL){
		AlarmListHead = NewAlarm;
		AlarmListTail = NewAlarm;
	}	
	else{
		AlarmListTail->next = NewAlarm;
		AlarmListTail = NewAlarm;
	}
	/*To send Alarm_creation notification*/
	const char *node_path[7]={"fault-id","fault-source","affected-objects/name","fault-severity","is-cleared","event-time","fault-text"};
	rc=ps5g_mplane_send_notification(session,"/o-ran-fm:alarm-notif",node_path,(const char**)node_val,7);

	if(rc != SR_ERR_OK) {
		printf("could not send alarm notif\n");
		return SR_ERR_OPERATION_FAILED;
	}

	return rc;
}

/*Alarm removal chapter 8 2nd paragraph*/
	int 
ps5g_mplane_remove_alarm(sr_session_ctx_t *session,const char *fault_source)
{
	int rc = SR_ERR_OK;

	Alarm* chk =AlarmListHead,*temp;

	const char *node_path[7]={"fault-id","fault-source","affected-objects/name","fault-severity","is-cleared","event-time","fault-text"};
	while(chk->next!=NULL)
	{
		if(strcmp(chk->next->fault_source,fault_source)==0)
		{
			temp=chk->next;
			chk->next=chk->next->next;
	char *node_val[7];

	node_val[0]=strdup(temp->fault_id);
	node_val[1]=strdup(temp->fault_source);
	node_val[2]=strdup(temp->name);
	node_val[3]=strdup(temp->fault_severity);
	node_val[4]=strdup("true");
	node_val[5]=strdup(temp->event_time);
	node_val[6]=strdup(temp->fault_text);


	rc=ps5g_mplane_send_notification(session,"/o-ran-fm:alarm-notif",node_path,(const char**)node_val,7);

	if(rc != SR_ERR_OK) {
		printf("could not send alarm notif\n");
		return SR_ERR_OPERATION_FAILED;
	}
			free(temp);		
		}
	else
		chk=chk->next;
	}
	return rc;

}



ssh_session ps5g_mp_connect_ssh(const char *host, const char *user,int verbosity,const char *pass,char *error){
	ssh_session session;
	int auth=0;

	session=ssh_new();
	if (session == NULL) {
		return NULL;
	}

	if(user != NULL){
		if (ssh_options_set(session, SSH_OPTIONS_USER, user) < 0) {

			error=strdup("invalid user");
			ssh_free(session);
			return NULL;
		}   
	}

	if (ssh_options_set(session, SSH_OPTIONS_HOST, host) < 0) {
		ssh_free(session);
		error=strdup("invalid host");
		return NULL;
	}
	//	ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
	if(ssh_connect(session)){
		error=strdup(ssh_get_error(session));
		printf("Connection failed : %s\n",error);
		ssh_disconnect(session);
		ssh_free(session);
		return NULL;
	}
#if 0
	if(verify_knownhost(session)<0){
		ssh_disconnect(session);
		ssh_free(session);
		return NULL;
	}
#endif
	auth= ssh_userauth_password(session,NULL,pass);

	//  auth=authenticate_console(session);
	if(auth==SSH_AUTH_SUCCESS){

		return session;
	} else if(auth==SSH_AUTH_DENIED){
		error=strdup("Authentication failed");
		printf("%s\n",error);
		return NULL;
	} else {
		error=strdup(ssh_get_error(session));
		printf("Error while authenticating : %s\n",error);
	}
	ssh_disconnect(session);
	ssh_free(session);
	return NULL;
}

int ps5g_mp_file_upload_accept(ssh_session session,const char *local_file,const char *remote_file,char *error) {

	//	printf("***********%d %s\n",__LINE__,__func__);

	sftp_session sftp = sftp_new(session);

	struct stat st;
	FILE *from;
	sftp_file to;
	int len = 1;
	int filesize,ret=0;

	char remote_path[MAX_PATH_LEN];
	strcat(remote_path,remote_file);
	strcat(remote_path,strrchr(local_file,'/'));

	//	printf("remote_path %s %d\n",remote_path,(int)strlen(remote_path));

	if (!sftp) {
		printf("sftp error initialising channel: %s\n",error);

		sprintf(error,"%s",ssh_get_error(session));

		ret=1;
		goto end;
	}

	if (sftp_init(sftp)) {

		sprintf(error,"%s",ssh_get_error(session));
		printf("error initialising sftp: %s\n",error);
		ret=1;
		goto end;
	}
	//	printf("after sftp_init%d\n",__LINE__);

	from=fopen(local_file,"r");
	if(from==NULL){
		sprintf(error,"local file %s not found",local_file);
		printf("error %s\n",error);
		ret=1;
		goto end;
	}

	stat(local_file,&st);
	filesize=st.st_size;

	if((st.st_mode & S_IFMT)!=S_IFREG)
	{
		sprintf(error,"local file %s is not regular file",local_file);
		printf("error %s\n",error);
		ret=1;
		goto end;
	}

	printf("size %d\n",filesize);	

	/* open a file for writing... */
	to = sftp_open(sftp,remote_file , O_WRONLY | O_CREAT, 0700);
	if (!to) {
		sprintf(error,"error opening remote file %s for writing %s",remote_path,ssh_get_error(session));
		printf("%s\n",error);
		fclose(from);
		ret=1;
		goto end;
	}

	printf("file availability checking finished\n");
	if (len < 0) {
		fprintf(stderr, "Error reading file: %s\n", ssh_get_error(session));

		ret=1;
	}


	fclose(from);
	sftp_close(to);
end:
	/* close the sftp session */
	sftp_free(sftp);
	//	printf("sftp session terminated\n");
	return ret;
}
int ps5g_mp_upload_to_remote(ssh_session session,const char *local_file,const char *remote_file,char *error) {

	//	printf("***********%d %s\n",__LINE__,__func__);

	sftp_session sftp = sftp_new(session);

	struct stat st;
	FILE *from;
	sftp_file to;
	int len = 1;
	char *data;
	int filesize,ret=0;

	char remote_path[MAX_PATH_LEN];
	strcat(remote_path,remote_file);
	strcat(remote_path,strrchr(local_file,'/'));

	//	printf("remote_path %s %d\n",remote_path,(int)strlen(remote_path));

	if (!sftp) {
		//		fprintf(stderr, "sftp error initialising channel: %s\n",ssh_get_error(session));

		sprintf(error,"%s",ssh_get_error(session));
		ret=1;
		goto end;
	}

	if (sftp_init(sftp)) {
		//		fprintf(stderr, "error initialising sftp: %s\n",ssh_get_error(session));
		sprintf(error,"%s",ssh_get_error(session));
		ret=1;
		goto end;
	}
	//	printf("after sftp_init%d\n",__LINE__);

	from=fopen(local_file,"r");
	if(from==NULL){
		printf("local file %s  doesn't exist\n",local_file);
		sprintf(error,"%s","local file doesn't exist");
		ret=1;
		goto end;
	}

	stat(local_file,&st);
	filesize=st.st_size;

	printf("size %d\n",filesize);	

	data=(char*)malloc(filesize);

	/* open a file for writing... */
	to = sftp_open(sftp,remote_file , O_WRONLY | O_CREAT, 0700);
	if (!to) {
		//		fprintf(stderr, " Error opening remote file %s for writing: %s\n",remote_path,ssh_get_error(session));
		sprintf(error, "Error opening remote file %s for writing: %s\n",remote_path,ssh_get_error(session));
		//sprintf(error,"%s","Error opening remote file");
		fclose(from);
		ret=1;
		goto end;
	}
	printf("File upload started\n");
	while(filesize!=0)
	{
		if(filesize>SFTP_MAX_RW_SIZE)
			len=fread( data,1,SFTP_MAX_RW_SIZE,from);
		else
			len=fread(data,1,filesize,from);
		filesize-=len;
		//printf("len %d %d\n",len,filesize);
		printf(".");
		fflush(stdout);
		if (sftp_write(to, data, len) != len) {
			//			fprintf(stderr, "Error writing %d bytes: %s\n",len, ssh_get_error(session));
			sprintf(error,"error writing %d bytes %s",len,ssh_get_error(session)); 
			sftp_close(to);
			fclose(from);
			ret=1;
			goto end;
		}
	}
	printf("file-upload finished\n");
	if (len < 0) {
		//fprintf(stderr, "Error reading file: %s\n", ssh_get_error(session));
		sprintf(error, "Error reading file: %s\n", ssh_get_error(session));
		ret=1;
	}


	fclose(from);
	sftp_close(to);
end:
	/* close the sftp session */
	sftp_free(sftp);
	free(data);
	printf("sftp session terminated\n");
	return ret;
}

int ps5g_mp_download_from_remote(ssh_session session,const char *local_file,const char *remote_file,char *error) {

	//	printf("***********%d %s\n",__LINE__,__func__);

	sftp_session sftp = sftp_new(session);

	sftp_attributes st;
	FILE *to;
	sftp_file from;
	int len = 1,ret;
	char *data;
	int filesize;


	printf("local_file %s remote_file %s\n",local_file,remote_file);

	char local_path[MAX_PATH_LEN]={0};
	strcat(local_path,local_file);
	strcat(local_path,strrchr(remote_file,'/'));

	printf("local_path %s\n",local_path);

	if (!sftp) {
		//	fprintf(stderr, "sftp error initialising channel: %s\n",ssh_get_error(session));
		sprintf(error, "sftp error initialising channel: %s\n",ssh_get_error(session));

		printf("%s",error);

		ret=1;
		goto end;
	}

	if (sftp_init(sftp)) {
		sprintf(error, "error initialising sftp: %s\n",
				ssh_get_error(session));
		ret=1;
		goto end;
	}
	//	printf("after sftp_init%d\n",__LINE__);

	to=fopen(local_path,"w+");

	if(to==NULL){
		sprintf(error,"local file %s  open failed\n",local_path);
		printf("local file %s  open failed\n",local_path);
		ret=1;

		goto end;
	}

	/* open a file for writing... */
	from = sftp_open(sftp,remote_file , O_RDONLY, 0700);

	if (!from) {
		sprintf(error, " Error opening remote file %s for writing: %s\n",remote_file,ssh_get_error(session));
		printf("%s\n",error);
		fclose(to);
		ret=1;
		goto end;
	}

	st=sftp_stat(sftp,remote_file);

	if(st->type!=SSH_FILEXFER_TYPE_REGULAR)
	{
		sprintf(error,"%s", "remote_file is not a regular file");
		printf("%s",error);
		ret=1;
		goto end;
	}
	filesize=st->size;

	printf("size %d\n",filesize);	

	data=(char*)malloc(filesize);
	while(filesize!=0){

		//	printf("len %d %d\n",len,filesize);

		printf(".");
		fflush(stdout);

		if(filesize>SFTP_MAX_RW_SIZE){	
			len=sftp_read(from, data,SFTP_MAX_RW_SIZE );
			if(len!=SFTP_MAX_RW_SIZE){
				sprintf(error, "Error writing %d bytes: %s\n",
						len, ssh_get_error(session));
				printf("%s\n",error);
				sftp_close(from);
				fclose(to);
				ret=1;
				goto end;
			}
		}
		else{
			len=sftp_read(from, data,filesize);
			if(len!=filesize){
				sprintf(error, "Error writing %d bytes: %s\n",
						len, ssh_get_error(session));
				printf("%s\n",error);
				sftp_close(from);
				fclose(to);
				ret=1;
				goto end;
			}

		}
		filesize-=len;
		ret=fwrite(data,1,len,to);
		if(ret!=len)	
		{

			sprintf(error, "Error writing %d bytes\n",len);
			printf("%s\n",error);
			sftp_close(from);
			fclose(to);
			ret=1;
			goto end;

		}	

	}
	printf("finished\n");

	if (len < 0) {
		sprintf(error, "Error reading file: %s\n", ssh_get_error(session));
		printf("%s\n",error);
		ret=1;
	}

	fclose(to);
	sftp_close(from);
end:
	/* close the sftp session */
	sftp_free(sftp);
	free(data);
	printf("sftp session terminated\n");
	return ret;
}

int ps5g_mp_download_accept(ssh_session session,const char *local_file,const char *remote_file,char *error) {

	//	printf("***********%d %s\n",__LINE__,__func__);

	sftp_session sftp = sftp_new(session);

	sftp_attributes st;
	FILE *to;
	sftp_file from;
	int ret;
	//char *data;
	//int filesize;


	printf("local_file %s remote_file %s\n",local_file,remote_file);

	char local_path[MAX_PATH_LEN]={0};
	strcat(local_path,local_file);
	strcat(local_path,strrchr(remote_file,'/'));

	printf("local_path %s\n",local_path);

	if (!sftp) {
		sprintf(error,"%s",ssh_get_error(session));
		printf("sftp error initialising channel: %s\n",error);
		ret=1;
		goto end;
	}

	if (sftp_init(sftp)) {
		sprintf(error,"%s",ssh_get_error(session));
		printf("error initialising sftp: %s\n",error);
		ret=1;
		goto end;
	}
	//	printf("after sftp_init%d\n",__LINE__);

	to=fopen(local_path,"w+");

	if(to==NULL){
		printf("local file %s  open failed\n",local_path);

		sprintf(error,"local file %s open failed",local_path);

		ret=1;

		goto end;
	}

	/* open a file for writing... */
	from = sftp_open(sftp,remote_file , O_RDONLY, 0700);

	if (!from) {
		printf(" Error opening remote file %s for writing: %s\n",remote_file,ssh_get_error(session));
		sprintf(error,"error opening remote file : %s",ssh_get_error(session));
		fclose(to);
		ret=1;
		goto end;
	}
	fclose(to);
	sftp_close(from);

	st=sftp_stat(sftp,remote_file);
	if(st->type!=SSH_FILEXFER_TYPE_REGULAR)
	{
		sprintf(error,"%s", "remote_file is not a regular file");
		printf("%s",error);
		ret=1;
		goto end;
	}


end:
	/* close the sftp session */
	sftp_free(sftp);
	//free(data);
	//	printf("sftp session terminated\n");
	return ret;
}

/*copy running config to startup to retain config changes
 made by client after reboot*/

int ps5g_mp_copy_config_to_startup(sr_session_ctx_t *session,const char *module_name)
{
	
	printf("copying config from running to starup for %s\n",module_name);
	sr_datastore_t ds_org=sr_session_get_ds(session);       //get current data_store
        sr_datastore_t ds;
        ds =SR_DS_STARTUP;
        sr_session_switch_ds(session, ds);
        int c=sr_copy_config(session,module_name,1,0);
        sr_session_switch_ds(session,ds_org);
	return c;
}


int ps5g_mp_data_population(sr_session_ctx_t *session, RuntimeConfig* config)
{
		 sr_val_t *vals=NULL;
				int rc = SR_ERR_OK;
				rc=ps5g_mplane_get_operational_data(session,T2A_MIN_UP,&vals);
				if(rc==SR_ERR_OK)
					 {
					 
					    config->T2a_min_up = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
					     printf("MP: T2a_min_up  %d\n", config->T2a_min_up);}
                               
				else
					printf("operation failed for T2a_min_up %d\n",rc);
					
				rc=ps5g_mplane_get_operational_data(session,T2A_MAX_UP,&vals);
				if(rc==SR_ERR_OK)
					 {
					 
					     config->T2a_max_up = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
					     printf("MP: T2a_max_up  %d\n", config->T2a_max_up);
	                       }
                               
				else
					printf("operation failed for T2a_max_up %d\n",rc);
					
				rc=ps5g_mplane_get_operational_data(session,T2A_MAX_CP_DL,&vals);
				if(rc==SR_ERR_OK)
					 {
					 
					     config->T2a_max_cp_dl = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
					     printf("MP: T2a_max_cp_dl  %d\n", config->T2a_max_cp_dl);
	                       }
                               
				else
					printf("operation failed for T2a_max_cp_dl %d\n",rc);
					
					
				rc=ps5g_mplane_get_operational_data(session,T2A_MIN_CP_DL,&vals);
				if(rc==SR_ERR_OK)
					 {
					 
					     config->T2a_min_cp_dl = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
					     printf("MP: T2a_min_cp_dl  %d\n", config->T2a_min_cp_dl);
	                       }
                               
				else
					printf("operation failed for T2a_min_cp_dl %d\n",rc);	
					
									
				rc=ps5g_mplane_get_operational_data(session,T2A_MIN_CP_UL,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->T2a_min_cp_ul = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: T2a_min_cp_ul  %d\n", config->T2a_min_cp_ul);
                               }

                                else
                                        printf("operation failed for T2a_min_cp_ul %d\n",rc);	

				rc=ps5g_mplane_get_operational_data(session,T2A_MAX_CP_UL,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->T2a_max_cp_ul = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: T2a_max_cp_ul  %d\n", config->T2a_max_cp_ul);
                               }

                                else
                                        printf("operation failed for T2a_max_cp_ul %d\n",rc);

				rc=ps5g_mplane_get_operational_data(session,TADV_CP_DL,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->Tadv_cp_dl = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: Tadv_cp_dl  %d\n", config->Tadv_cp_dl);
                               }

                                else
                                        printf("operation failed for Tadv_cp_dl %d\n",rc);																															
				rc=ps5g_mplane_get_operational_data(session,TA3_MIN,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->Ta3_min = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: Ta3_min  %d\n", config->Ta3_min);
                               }

                                else
                                        printf("operation failed for Ta3_min %d\n",rc);

				rc=ps5g_mplane_get_operational_data(session,TA3_MAX,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->Ta3_max = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: Ta3_max  %d\n", config->Ta3_max);
                               }

                                else
                                        printf("operation failed for Ta3_max %d\n",rc);
					
				

				rc=ps5g_mplane_get_running_data(session,T1A_MAX_CP_DL,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->T1a_max_cp_dl = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: T1a_max_cp_dl  %d\n", config->T1a_max_cp_dl);
                               }

                                else
                                        printf("operation failed for T1a_max_cp_dl %d\n",rc);


                                rc=ps5g_mplane_get_running_data(session,T1A_MAX_UP,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->T1a_max_up = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: T1a_max_up  %d\n", config->T1a_max_up);
                               }

                                else
                                        printf("operation failed for T1a_max_up %d\n",rc);

				
				rc=ps5g_mplane_get_running_data(session,TA4_MAX,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->Ta4_max = ((vals)->data.uint32_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: Ta4_max  %d\n", config->Ta4_max);
                               }

                                else
                                        printf("operation failed for Ta4_max %d\n",rc);


                                rc=ps5g_mplane_get_operational_data(session,NDLABSFREPOINTA,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->nDLAbsFrePointA = ((vals)->data.uint64_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: nDLAbsFrePointA  %d\n", config->nDLAbsFrePointA);
                               }

                                else
                                        printf("operation failed for nDLAbsFrePointA %d\n",rc);



				rc=ps5g_mplane_get_operational_data(session,NULABSFREPOINTA,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->nULAbsFrePointA = ((vals)->data.uint64_val)/1000;
					if CONFIG_PRINT
                                             printf("MP: nULAbsFrePointA  %d\n", config->nULAbsFrePointA);
                               }

                                else
                                        printf("operation failed for nULAbsFrePointA %d\n",rc);


                                rc=ps5g_mplane_get_operational_data(session,NDLBANDWIDTH,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->nDLBandwidth = ((vals)->data.uint64_val)/1000000;
					if CONFIG_PRINT
                                             printf("MP: nDLBandwidth  %d\n", config->nDLBandwidth);
                               }

                                else
                                        printf("operation failed for nDLBandwidth %d\n",rc);



				rc=ps5g_mplane_get_operational_data(session,NULBANDWIDTH,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->nULBandwidth = ((vals)->data.uint64_val)/1000000;
					if CONFIG_PRINT
                                             printf("MP: nULBandwidth  %d\n", config->nULBandwidth);
                               }

                                else
                                        printf("operation failed for nULBandwidth %d\n",rc);


                                rc=ps5g_mplane_get_operational_data(session,MTUSIZE,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->mtu = (vals)->data.uint16_val;
					if CONFIG_PRINT
                                             printf("MP:mtu  %d\n", config->mtu);
                               }

                                else
                                        printf("operation failed for mtu %d\n",rc);



				rc=ps5g_mplane_get_operational_data(session,MAXFRAMEID,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->maxFrameId = (vals)->data.uint16_val;
					if CONFIG_PRINT
                                             printf("MP: maxFrameId  %d\n", config->maxFrameId);
                               }

                                else
                                        printf("operation failed for maxFrameId %d\n",rc);


                                rc=ps5g_mplane_get_operational_data(session,PRACHCONFIGINDEX,&vals);
                                if(rc==SR_ERR_OK)
                                         {

                                             config->prachConfigIndex = (vals)->data.uint8_val;
					if CONFIG_PRINT
                                             printf("MP: prachConfigIndex  %d\n", config->prachConfigIndex);
                               }

                                else
                                        printf("operation failed for prachConfigIndex %d\n",rc);



				rc=ps5g_mplane_get_operational_data(session,XRANMODE,&vals);
                                if(rc==SR_ERR_OK)
                                         {
						if(strcmp((vals)->data.enum_val,"CAT_A")==0){
							config->xranCat=0;
						}
						else if(strcmp((vals)->data.enum_val,"CAT_B")==0){
							config->xranCat=1;
						}
					if CONFIG_PRINT
                                             printf("MP: xranmode  %d\n", config->xranCat);
                               }

                                else
                                        printf("operation failed for xranmode %d\n",rc);
                                        
                              return 0;

}


