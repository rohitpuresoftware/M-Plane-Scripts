#include <dirent.h>
#include "subscribe_oran_apis.h"
#ifndef ORAN_APIS
#include "oran_apis.h"
#endif

void *handle=0;

int no_of_alarms;
Alarm *AlarmListHead;
Alarm *AlarmListTail;
	char*
add_str(const char *first, const char *second)
{
	int lenf = strlen(first);
	int lens = strlen(second);
	char *buff = NULL;

	buff = malloc(sizeof(char) * (lenf + lens + 1));
	memset(buff, 0, (lenf + lens + 1));

	strcpy(buff, first);
	strcpy(buff+lenf, second);
	return buff;
}

char *allocate_buffer(size_t size)
{
	char *buff = NULL;
	buff = malloc(size);
	memset(buff, 0, size);
	return buff;
}

char *read_oper_data_file(char *path)
{
	FILE * fptr;
	int read_len = 0;

	int buff_len = 0;

	char *buff = NULL;

	char buffer[MAX_BUFF_SIZE]={0};

	buff = malloc(1);

	fptr = fopen(path, "r");

	if(fptr == NULL)
	{
		printf("Failed to open file\n");
		return 0;
	}

	while(1)
	{
		read_len = fread(buffer, sizeof(char), MAX_BUFF_SIZE, fptr);
		if(read_len > 0)
		{
			buff = realloc(buff, buff_len + read_len + 1);
			strncpy(buff + buff_len, buffer, read_len < MAX_BUFF_SIZE ? read_len:MAX_BUFF_SIZE);
			buff_len += read_len;
		}
		if(read_len < MAX_BUFF_SIZE)
		{
			if(feof(fptr))
			{
				buff[buff_len]='\0';
				break;
			}
			else
			{
				printf("Error occured");
				free(buff);
				buff = NULL;
				break;
			}
		}
	}

	fclose(fptr);
	return buff;
}

const char *
ev_to_str(sr_event_t ev)
{
	switch (ev) {
		case SR_EV_CHANGE:
			return "change";
		case SR_EV_DONE:
			return "done";
		case SR_EV_ABORT:
		default:
			return "abort";
	}
}

int (*load_symbol(char *symbol))()
{
	char *error = NULL;
	int (*symbol_p)();
	symbol_p = dlsym(handle, symbol);
	if ((error = dlerror()) != NULL)  {
		fprintf (stderr, "%s\n", error);
		exit(1);
	}
	return symbol_p;
}

int
module_change_cb(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *module_name,
		const char *xpath,
		sr_event_t event,
		uint32_t request_id,
		void *private_data)
{
	int rc = SR_ERR_OK;
	int capacitiy = 0;
	int val_count = 0;
	char path[512];
	(void)request_id;
	(void)private_data;
	sr_change_iter_t *it = NULL;
	sr_change_oper_t oper;
	sr_val_t *old_value = NULL;
	sr_val_t *new_value = NULL;
	modified_data_t *in = NULL;

	printf("\n\n ========== EVENT %s CHANGES: ====================================\n\n", ev_to_str(event));
	if(SR_EV_DONE != event)
	{
		printf("\n\nIntermediate event, not to be handle\n\n");
		fflush(stdout);
		return SR_ERR_OK;
	}
	sprintf(path, "/%s:*//.", module_name);
	rc = sr_get_changes_iter(session, path, &it);
	if (rc != SR_ERR_OK) {
		goto cleanup;
	}

	while ((rc = sr_get_change_next(session, it, &oper, &old_value, &new_value)) == SR_ERR_OK) {
		if(capacitiy == 0)
		{
			capacitiy = CHANGE_CANDICATE_CAP;
			in = realloc(in, (val_count + capacitiy)*sizeof(modified_data_t));
		}

		in[val_count].old_val = (void*)old_value;
		in[val_count].new_val = (void*)new_value;
		in[val_count].oper = oper;
		val_count++;
		capacitiy--;
	}
	rc=ps5g_mplane_change_notification(in, val_count);




	if(rc != SR_ERR_OK)
		goto cleanup;
	else
	{

		rc= ps5g_mp_copy_config_to_startup(session,module_name);

		if(rc!=SR_ERR_OK)
			printf("copy donfig to startup failed with Error code: %d\n",rc);
	}


	printf("\n ========== END OF CHANGES =======================================");

cleanup:
	if (in != NULL)
	{
		do
		{
			val_count--;
			sr_free_val((sr_val_t*)in[val_count].old_val);
			sr_free_val((sr_val_t*)in[val_count].new_val);
		}while(val_count > 0);
		free(in);
	}
	sr_free_change_iter(it);
	return SR_ERR_OK;
}

int subscribe_state_data_cb(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *module_name,
		const char *xpath,
		const char *request_xpath,
		uint32_t request_id,
		struct lyd_node **parent,
		void *private_data)
{
	char *buff = private_data;

	int rc=LY_SUCCESS;
	/*
	 * @Populate operational data
	 */

	rc=lyd_parse_data_mem((struct ly_ctx *)sr_get_context(oran_srv.sr_conn),
			buff,
			LYD_XML,
			0,
			LYD_VALIDATE_PRESENT,
			parent);

	return rc;
}
int subscribe_state_data_json_cb(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *module_name,
		const char *xpath,
		const char *request_xpath,
		uint32_t request_id,
		struct lyd_node **parent,
		void *private_data)
{
	char *buff = private_data;


	int rc=LY_SUCCESS;
	/*
	 * @Populate operational data
	 */

	rc=lyd_parse_data_mem((struct ly_ctx *)sr_get_context(oran_srv.sr_conn),
			buff,
			LYD_JSON,
			0,
			LYD_VALIDATE_PRESENT,
			parent);

	return rc;
}
#if 0
int subscribe_customer_rpc()
{
	printf("Need to add by customer");
	subscribe_o_ran_rpcs("/o-ran-software-management:software-install", mplane_oran_software_install);
	return 0;
}
#endif

int mplane_oran_reset()
{
	printf("Do reset related stuff here\n");
	return 0;
}

	int
mplane_oran_ald_communication(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	printf("Ald communication here\n");
	int rc = SR_ERR_OK;
	ruapp_switch_in_t in;
	ruapp_switch_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	in.type = START;
	*output_cnt = 1;

	rc=ps5g_mplane_start_ruapp(&in, &out);

	if(rc != SR_ERR_OK)
		goto error;
	fflush(stdout);
	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/port-id"),
			SR_INT32_T,
			0,
			4,
			strdup("unknown"));
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->ruapp_start_out.status==SUCCESS ? strdup("ACCEPTED"):strdup("REJECTED"),
			strdup("unknown"));

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}
	int
mplane_start_ruapp(sr_session_ctx_t *session,
		uint32_t sub_id, 
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ruapp_switch_in_t in;
	ruapp_switch_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};
	in.type = START;
	*output_cnt = 1;

		rc = ps5g_mplane_start_ruapp(&in, &out);

	if(rc != SR_ERR_OK)
		goto error;
	fflush(stdout);
	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->ruapp_start_out.status==SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));

	if(out->ruapp_start_out.status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/error-message"),
				SR_STRING_T,
				0,
				strdup(out->ruapp_start_out.error_message),
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}

	int
mplane_oran_software_download(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ru_sw_pkg_in_t in;
	ru_sw_pkg_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	/* Fill the input buffer */
	in.type = SW_DOWNLOAD;
	in.sw_download_in.remote_file_path = input->data.string_val;
	in.session=session;

	if(strstr(input[2].xpath,"password")){

		if(DEBUG)	
			printf("password %s\n",input[2].data.string_val);

		in.sw_download_in.credentials.type=PASSWORD;
		in.sw_download_in.credentials.cred_password.password._password=input[2].data.string_val;
	}

	rc=ps5g_mplane_software_download(&in, &out);
	if(rc != SR_ERR_OK)
	{
		goto error;
	}

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->sw_download_out.status==STARTED ? strdup("STARTED"):strdup("FAILED"),
			strdup("unknown"));

	//	ADD_OUTPUT( wrap_sr_val,
	//			add_str(path, "/notification-timeout"),
	//			SR_INT32_T,
	//			0,
	//			10,
	//			strdup("unknown"));

	if(out->sw_download_out.status != STARTED)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/error-message"),
				SR_STRING_T,
				0,
				strdup(out->sw_download_out.error_message),
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}

	int
mplane_oran_software_install(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ru_sw_pkg_in_t in;
	ru_sw_pkg_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	// More values can be filled and passed
	in.type = SW_INSTALL;
	in.sw_install_in.slot_name = input->data.string_val;

	rc=ps5g_mplane_software_install(&in, &out);

	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->sw_install_out.status == STARTED ? strdup("STARTED"):strdup("FAILED"),
			strdup("unknown"));
	if (out->sw_install_out.status == FAILED)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/error-message"),
				SR_STRING_T,
				0,
				out->sw_install_out.error_message,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}

	int
mplane_oran_software_activate(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ru_sw_pkg_in_t in;
	ru_sw_pkg_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	in.type = SW_ACTIVATE;
	in.sw_activate_in.slot_name = input->data.string_val;

	ps5g_mplane_software_activate(&in, &out);
	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->sw_activate_out.status == STARTED ? strdup("STARTED"):strdup("FAILED"),
			strdup("unknown"));

	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/notification-timeout"),
			SR_INT32_T,
			0,
			out->sw_activate_out.notification_timeout,
			strdup("unknown"));
	if (out->sw_activate_out.status != STARTED)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/error-message"),
				SR_STRING_T,
				0,
				out->sw_activate_out.error_message,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}

	int
mplane_oran_file_upload(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ru_file_mgmt_in_t in;
	ru_file_mgmt_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	in.session=session;
	in.type = FILE_UPLOAD;
	in.file_upload_in.path.local_logical_file_path = input[0].data.string_val;
	in.file_upload_in.path.remote_file_path = input[1].data.string_val;

	if(strstr(input[3].xpath,"password")){
		if(DEBUG)	
			printf("password %s\n",input[3].data.string_val);
		in.file_upload_in.credentials.type=PASSWORD;
		in.file_upload_in.credentials.cred_password.password._password=input[3].data.string_val;
	}

	else if(strstr(input[3].xpath,"certificate")){
		in.file_upload_in.credentials.type=CERTIFICATE;
	}

	rc=ps5g_mplane_file_upload(&in, &out);
	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->file_upload_out.status == SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));
	if (out->file_upload_out.status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/reject-reason"),
				SR_STRING_T,
				0,
				out->file_upload_out.reject_reason,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;
}

	int
mplane_oran_retrieve_file_list(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int file_i = 0;
	int rc = SR_ERR_OK;
	ru_file_mgmt_in_t in;
	ru_file_mgmt_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};

	in.type = FILE_LIST;
	in.file_retrieve_in.logical_path = input[0].data.string_val;
	in.file_retrieve_in.file_name_filter = input[1].data.string_val;

	rc=ps5g_mplane_retrieve_file_list(&in, &out);
	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->file_retrieve_out.status == SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));
	if (out->file_retrieve_out.status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/reject-reason"),
				SR_STRING_T,
				0,
				out->file_retrieve_out.reject_reason,
				strdup("unknown"));
	}
	else
	{
		/* Adding the files list in output buffer */
		while (out->file_retrieve_out.file_list[file_i])
		{
			ADD_OUTPUT( wrap_sr_val,
					add_str(path, "/file-list"),
					SR_STRING_T,
					0,
					out->file_retrieve_out.file_list[file_i++],
					strdup("unknown"));
		}
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	printf("retrive file list operation finished\n");	
	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);	
	}
	printf("retrive file operation failed\n");	
	return -1;
}

	int
mplane_oran_file_download(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	int rc = SR_ERR_OK;
	ru_file_mgmt_in_t in;
	ru_file_mgmt_out_t *out = NULL;
	wrap_sr_val_t wrap_sr_val = {0};
	in.session=session;
	in.type = FILE_DOWNLOAD;
	in.file_download_in.path.local_logical_file_path = input[0].data.string_val;
	in.file_download_in.path.remote_file_path = input[1].data.string_val;

	if(strstr(input[3].xpath,"password")){
		if(DEBUG)	
			printf("password %s\n",input[3].data.string_val);
		in.file_upload_in.credentials.type=PASSWORD;
		in.file_upload_in.credentials.cred_password.password._password=input[3].data.string_val;
	}

	else if(strstr(input[3].xpath,"certificate")){
		in.file_upload_in.credentials.type=CERTIFICATE;
	}

	rc=ps5g_mplane_file_download(&in, &out);

	if(rc != SR_ERR_OK)
		goto error;
	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->file_download_out.status==SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));

	if (out->file_download_out.status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/reject-reason"),
				SR_STRING_T,
				0,
				out->file_download_out.reject_reason,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;

error:
	if(wrap_sr_val.values)
	{
		sr_free_values(wrap_sr_val.values, wrap_sr_val.count);
	}
	return -1;

}

	int
mplane_oran_trace_start(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{
	printf("mplane_oran_trace_start rpc call \n");

	int rc = SR_ERR_OK;
	ru_trace_out_t *out;	
	ru_trace_switch_in_t in;	

	wrap_sr_val_t wrap_sr_val = {0};
	in.type = START;
	in.session=session;

	rc=ps5g_mp_trace_start(&in, &out);

	*output_cnt = 1;


	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->status==SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));


	if (out->status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/failure-reason"),
				SR_STRING_T,
				0,
				out->reject_reason,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;	

error:
	return -1;			

}

int mplane_oran_trace_stop(sr_session_ctx_t *session,
		uint32_t sub_id,
		const char *path,
		const sr_val_t *input,
		const size_t input_cnt,
		sr_event_t event,
		uint32_t request_id,
		sr_val_t **output,
		size_t *output_cnt,
		void *private_data)
{


	printf("mplane_oran_trace_stop rpc call \n");

	int rc = SR_ERR_OK;
	ru_trace_out_t *out;	
	ru_trace_switch_in_t in;	

	wrap_sr_val_t wrap_sr_val = {0};
	in.type = START;

	in.session=session;

	rc=ps5g_mp_trace_stop(&in, &out);

	*output_cnt = 1;

	if(rc != SR_ERR_OK)
		goto error;

	/* Preparing the output buffer to send to NMS(CLI) */
	ADD_OUTPUT( wrap_sr_val,
			add_str(path, "/status"),
			SR_ENUM_T,
			0,
			out->status==SUCCESS ? strdup("SUCCESS"):strdup("FAILURE"),
			strdup("unknown"));

	if (out->status != SUCCESS)
	{
		ADD_OUTPUT( wrap_sr_val,
				add_str(path, "/failure-reason"),
				SR_STRING_T,
				0,
				out->reject_reason,
				strdup("unknown"));
	}

	*output_cnt = wrap_sr_val.count;
	*output = wrap_sr_val.values;

	return SR_ERR_OK;	


error:
	return -1;
}

int init_sysrepo()
{
	int rc = SR_ERR_OK;
	/* connect to sysrepo */
	rc = sr_connect(SR_CONN_CACHE_RUNNING, &oran_srv.sr_conn);
	if (rc != SR_ERR_OK) {
		printf("Faild to connect to sysrepo\n");
		goto error;
	}

	/* start session */
	rc = sr_session_start(oran_srv.sr_conn, SR_DS_RUNNING, &oran_srv.sr_sess);
	if (rc != SR_ERR_OK) {
		printf("Faild to create sysrepo session\n");
		goto error;
	}
error:
	return rc;
}

int get_oper_data_container_ns(char *filename, char **buff, char **container, char **ns)
{
	char *i     = NULL;
	char *j     = NULL;
	char *start = NULL;
	char *end   = NULL;
	static char *file_buff = NULL;
	static char *temp_buff = NULL;
	static char *file_name = NULL;

	if(temp_buff == NULL)
	{
		if(filename)
		{
			file_name = filename;
			file_buff = temp_buff = read_oper_data_file(filename);
		}
		else
			return -1;
	}

	start = i = strchr(temp_buff, '<');
	j = strchr(start, ' ');

	/*
	 * @Ignore start and end character
	 * @It required <end-start-1>
	 */
	*container = allocate_buffer(j-start);
	strncpy(*container, start+1, j-start-1);
	i = strstr(j, "xmlns");
	i = strchr(i, '=');
	i++;
	while(*i == ' ')
		i++;
	j = strchr(i+1, *i);

	/*
	 * @Ignore start and end character
	 * @It required <end-start-1>
	 */
	*ns = allocate_buffer(j-i);
	strncpy(*ns, i+1, j-i-1);


	i = strstr(j, *container);
	end = strchr(i, '>');

	/*
	 * @Consider start and end character as well
	 */
	*buff = allocate_buffer(end - start + 2);
	strncpy(*buff, temp_buff, end - start + 1);

	temp_buff = strchr(end, '<');
	if(temp_buff == NULL)
	{
		free(file_buff);
		printf("Processed state data file: %s\n", file_name);
		file_buff = NULL;
		file_name = NULL;
	}
	else
	{
		printf("Still have some content to process\n");
	}
	return 0;
}


int get_oper_data_container(char *filename, char **buff, char **container)
{
	char *i     = NULL;
	char *j     = NULL;
	char *start = NULL;
	char *end   = NULL;
	static char *file_buff = NULL;
	static char *temp_buff = NULL;
	static char *file_name = NULL;

	if(temp_buff == NULL)
	{
		if(filename)
		{
			file_name = filename;
			file_buff = temp_buff = read_oper_data_file(filename);
		}
		else
			return -1;
	}

	start = i = strchr(temp_buff, '"')+1;
	j = strchr(start, '"');

	*container = allocate_buffer(j-start);
	strncpy(*container, start, j-start);

	i = strstr(j, *container);
	end = strrchr(start, '}');

	start=strchr(temp_buff,'{');

	*buff = allocate_buffer(end - start + 2);
	strncpy(*buff, temp_buff, end - start + 1);

	temp_buff = strchr(end, '{');
	if(temp_buff == NULL)
	{
		free(file_buff);
		printf("Processed state data file: %s\n", file_name);
		file_buff = NULL;
		file_name = NULL;
	}
	else
	{
		printf("Still have some content to process\n");
	}
	return 0;
}
int subscribe_operation_data()
{
	int rc = SR_ERR_OK;
	int flag = 0;
	DIR *d;
	struct dirent *dir;
	char *buff = NULL;
	char *container = NULL;
	char *ns = NULL;
	char abs_path[MAX_PATH_LEN]={0};
	char path[MAX_PATH_LEN]={0};
	char *base_path = STATE_DATA_XML_BASE_PATH;
	int base_path_len = strlen(base_path);
	struct lys_module *module = NULL;
	strcpy(abs_path, base_path);
	d = opendir(base_path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (dir->d_type == DT_REG)
			{
				/*
				 * @ Create absolute file path to read it
				 */
				memset(abs_path + base_path_len, 0, MAX_PATH_LEN - base_path_len);
				strcpy(abs_path + base_path_len, dir->d_name);

				/*
				 * @Read the xml
				 */
				if(strstr(abs_path,".xml")!=NULL)
				{
					get_oper_data_container_ns(abs_path, &buff, &container, &ns);

					do
					{
						module = (struct lys_module *)
							ly_ctx_get_module_latest_ns((struct ly_ctx *)sr_get_context(oran_srv.sr_conn),
									ns);
						if (module == NULL)
						{
							flag = 1;
							printf("No yang model installed with namespace: %s\n", ns);
						}
						else
						{
							memset(path, 0, MAX_PATH_LEN);
							sprintf(path, "/%s:%s", module->name, container);

							rc = sr_oper_get_items_subscribe(oran_srv.sr_sess,
									module->name,
									path,
									subscribe_state_data_cb,
									buff,
									SR_SUBSCR_CTX_REUSE,
									&oran_srv.sr_rpc_sub);
						}
					} while(get_oper_data_container_ns(NULL, &buff, &container, &ns) != -1);
				}

				/*
				 * @Read json
				 */

				else{
					printf("json file found\n");

					memset(path, 0, MAX_PATH_LEN);

					get_oper_data_container(abs_path, &buff, &container);


					strncpy(path,container,strchr(container,':')-container);


					module=(struct lys_module *)
						ly_ctx_get_module_latest((struct ly_ctx *)sr_get_context(oran_srv.sr_conn),
								path);

					if (module == NULL){
						flag = 1;
						printf("No yang model installed with namespace: %s\n", ns);
					}
					else{

						memset(path, 0, MAX_PATH_LEN);
						sprintf(path,"%c",'/');
						strcat(path,container);
						rc=sr_oper_get_items_subscribe(oran_srv.sr_sess,
								module->name,
								path,
								subscribe_state_data_json_cb,
								buff,
								SR_SUBSCR_CTX_REUSE,
								&oran_srv.sr_rpc_sub);

					}


				}
			}
		}
		closedir(d);
		if (flag == 1)
		{
			printf("Some of the yang model is not installed and for those state"
					"data is not populated. Some feature may not work\n");
		}
	}
	return rc;
}

	int
module_change_subscribe()
{
	int rc = SR_ERR_OK;
	FILE *fp = NULL;

	printf("==== Listening for changes in all modules ====\n");

	/* turn logging on */
	sr_log_stderr(SR_LL_WRN);

	/* Open the command for reading. */
	fp = popen("sysrepoctl -l | grep '| I' | cut -d ' ' -f 1", "r");
	if (fp == NULL) {
		printf("Failed to get the list of installed modules\n" );
		rc = -1;
		goto cleanup;
	}

	char module_name[MAX_MOD_NAME_LEN] = {0};

	/* Read the output a line at a time and subscribe for module change */
	while (fgets(module_name, MAX_MOD_NAME_LEN, fp) != NULL) {

		module_name[strcspn(module_name, "\n")] = '\0';

		/* subscribe for changes in running config */
		rc = sr_module_change_subscribe(oran_srv.sr_sess, module_name, NULL,
				module_change_cb, NULL, 0, 0, &oran_srv.sr_data_sub);
		if (rc != SR_ERR_OK) {
			printf("Failed to subscribe module(%s) for change", module_name);
			goto cleanup;
		}
	}

	/* close */
	pclose(fp);

cleanup:
	return rc;
}

int start_file_download_thread()
{
	printf("file_download_thread started\n");
	pthread_t file_d_thread;
	int rc=pthread_create(&file_d_thread,NULL,file_download_thread,NULL);
	if(rc!=SR_ERR_OK)
		goto error;
error:
	return rc;
}

int start_file_upload_thread()
{
	printf("file_upload_thread started\n");
	pthread_t file_ul_thread;
	int rc=pthread_create(&file_ul_thread,NULL,file_upload_thread,NULL);
	if(rc!=SR_ERR_OK)
		goto error;
error:
	return rc;

}
int start_software_download_thread()
{
	printf("software download thread started\n");
	pthread_t sw_dl_thread;
	int rc=pthread_create(&sw_dl_thread,NULL,software_download_thread,NULL);
	if(rc!=SR_ERR_OK)
		goto error;
error:
	return rc;		
}

int o_ran_lib_init()
{
	int rc = SR_ERR_OK;

	rc = init_sysrepo();
	if(rc != SR_ERR_OK)
	{
		goto error;
	}

	//printf("%d %s %s\n",__LINE__,__func__,__FILE__);
	rc = module_change_subscribe();
	if(rc != SR_ERR_OK)
	{
		goto error;
	}
	//printf("%d %s %s\n",__LINE__,__func__,__FILE__);
#if 0
	rc = subscribe_customer_rpc();
	if(rc != SR_ERR_OK)
	{
		goto error;
	}
#endif
	rc = subscribe_operation_data();
	if(rc != SR_ERR_OK)
	{
		goto error;
	}

error:
	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}

	int
subscribe_o_ran_rpcs(char *xpath,
		int(*cb)())
{
	int rc = SR_ERR_OK;

	/* turn logging on */
	sr_log_stderr(SR_LL_WRN);
	/* Register oran specific rpc */
	O_RAN_RPC_SUBSCR(xpath, cb);

error:
	return rc;
}
# if 1


#endif

static int
dp_get_items_cb(sr_session_ctx_t *session, uint32_t sub_id, const char *module_name, const char *xpath,
        const char *request_xpath, uint32_t request_id, struct lyd_node **parent, void *private_data)
{
    (void)session;
    (void)sub_id;
    (void)request_xpath;
    (void)request_id;
    (void)private_data;


      Alarm* alarm_tmp = AlarmListHead;

//	printf("\n\n ******========== DATA FOR \"%s\" \"%s\" REQUESTED =======================\n\n", module_name, xpath);
	int i=1;
	char path[100];				
	while(alarm_tmp!=NULL)
	{
	//	printf("***************before*********\n");
		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/fault-id",i);
		lyd_new_path(*parent, sr_get_context(sr_session_get_connection(session)),path,(alarm_tmp->fault_id), 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/fault-source",i);
	lyd_new_path(*parent, NULL, path, alarm_tmp->fault_source, 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/fault-severity",i);
		lyd_new_path(*parent, NULL, path, alarm_tmp->fault_severity, 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/is-cleared",i);
		lyd_new_path(*parent, NULL, path, alarm_tmp->is_cleared, 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/affected-objects/name",i);
		lyd_new_path(*parent, NULL, path, alarm_tmp->name, 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/fault-text",i);
		lyd_new_path(*parent, NULL, path, alarm_tmp->fault_text, 0, NULL);

		sprintf(path,"/o-ran-fm:active-alarm-list/active-alarms[%d]/event-time",i);
		lyd_new_path(*parent, NULL, path, alarm_tmp->event_time, 0, NULL);
		i++;
		alarm_tmp = alarm_tmp->next;
	}

	return SR_ERR_OK;
}


	int
o_ran_lib_sub()
{
	int i = 0;
	char rpcs[][MAX_SIZE_XPATH] = {
		"/o-ran-operations:reset",
		"/mplane-rpcs:start-ruapp",
		"/o-ran-software-management:software-download",
		"/o-ran-software-management:software-install",
		"/o-ran-software-management:software-activate",
		"/o-ran-file-management:file-upload",
		"/o-ran-file-management:retrieve-file-list",
		"/o-ran-file-management:file-download",
		"/o-ran-ald:ald-communication",
		"/o-ran-trace:start-trace-logs",
		"/o-ran-trace:stop-trace-logs"
	};
	int (*cbs[])() = {
		mplane_oran_reset,
		mplane_start_ruapp,
		mplane_oran_software_download,
		mplane_oran_software_install,
		mplane_oran_software_activate,
		mplane_oran_file_upload,
		mplane_oran_retrieve_file_list,
		mplane_oran_file_download,
		mplane_oran_ald_communication,
		mplane_oran_trace_start,
		mplane_oran_trace_stop,
		NULL
	};

	do {
		printf("subscribing callback(%d): %s\n", i, rpcs[i]);
		subscribe_o_ran_rpcs(rpcs[i], cbs[i]);
	} while(cbs[++i]);


	/*******for trace start/stop********/	

	/* subscribe for providing the operational data */

	int  rc = sr_oper_get_items_subscribe(oran_srv.sr_sess,"o-ran-fm","/o-ran-fm:active-alarm-list/*" , dp_get_items_cb, NULL, 0, &oran_srv.sr_rpc_sub);
	if (rc != SR_ERR_OK) {
		printf("alarm data subscription error\n");
	}

	rc=start_file_download_thread();
	if(rc != SR_ERR_OK)
	{
		printf("file_download_thread_fail\n");
	}
	rc=start_file_upload_thread();
	if(rc != SR_ERR_OK)
	{
		printf("file_upload_thread_fail\n");
	}
	rc=start_software_download_thread();
	if(rc != SR_ERR_OK)
	{
		printf("software_download_thread_fail\n");
	}
	return 0;
}

int o_ran_lib_deinit()
{
	sr_unsubscribe(oran_srv.sr_rpc_sub);
	sr_unsubscribe(oran_srv.sr_data_sub);
	/*
	 * sr_unsubscribe(oran_srv.sr_notif_sub);
	 */
	sr_disconnect(oran_srv.sr_conn);
	return SR_ERR_OK;
}
	int
o_ran_lib_init_ctx()
{
	int rc = SR_ERR_OK;
	dlerror();    /* Clear any existing error */
	rc = o_ran_lib_init();
	rc = o_ran_lib_sub();
	return rc;
}

	int
o_ran_lib_deinit_ctx()
{
	o_ran_lib_deinit();
	return 0;
}
