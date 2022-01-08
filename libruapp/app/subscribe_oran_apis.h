
#ifndef __SUBSCRIBE_ORAN_APIS__
#define __SUBSCRIBE_ORAN_APIS__

#include <string.h>
#include <dlfcn.h>

#include <sysrepo.h>

#include"oran_apis.h"

extern void *handle ;

#define ALRM_ID_SIZE 10
#define OBJ_INST_SIZE 15
#define TEXT_SIZE 50
#define DATE_TIME_SIZE 30

#define MAX_PATH_LEN 256
#define MAX_MOD_NAME_LEN 128
#define MAX_BUFF_SIZE 1024
#define CHANGE_CANDICATE_CAP 5
#define MAX_SIZE_XPATH 256
//#define STATE_DATA_XML_BASE_PATH "/tmp/mplane/state_data_xml/"

#define STATE_DATA_XML_BASE_PATH "/home/user/PureSoftware/MP_3.0/state_data_xml/"

#define O_RAN_RPC_SUBSCR(xpath, cb) \
    rc = sr_rpc_subscribe(oran_srv.sr_sess, xpath, cb, NULL, 0, SR_SUBSCR_CTX_REUSE, &oran_srv.sr_rpc_sub); \
    if (rc != SR_ERR_OK) { \
        printf("Subscribing for \"%s\" RPC failed (%s).", xpath, sr_strerror(rc)); \
        goto error; \
    }

typedef struct wrap_sr_val {
    uint8_t count;
    sr_val_t *values;
} wrap_sr_val_t;

typedef enum
{
   CRITICAL = 3,
   MAJOR = 4,
   MINOR = 5,
   WARNING = 6,
   INDETERMINATE = 7,
   CLEARED = 8
}SeverityLevel; 



#define SR_STRING_T_DATA              string_val
#define SR_BOOL_T_DATA                bool_val
#define SR_DECIMAL64_T_DATA           decimal64_val
#define SR_INT8_T_DATA                int8_val
#define SR_INT16_T_DATA               int16_val
#define SR_INT32_T_DATA               int32_val
#define SR_INT64_T_DATA               int64_val
#define SR_UINT8_T_DATA               uint8_val
#define SR_UINT16_T_DATA              uint16_val
#define SR_UINT32_T_DATA              uint32_val
#define SR_UINT64_T_DATA              uint64_val
#define SR_IDENTITYREF_T_DATA         identityref_val
#define SR_INSTANCEID_T_DATA          instanceid_val
#define SR_BITS_T_DATA                bits_val
#define SR_BINARY_T_DATA              binary_val
#define SR_ENUM_T_DATA                enum_val
#define SR_LIST_T_DATA                string_val

#define GET_FIELD(TYPE) TYPE##_DATA

#define ADD_OUTPUT(WRAP_OUT, XPATH, TYPE, DFLT, DATA,ORIGIN) {                       \
            WRAP_OUT.count++;                                                 \
            if(WRAP_OUT.values == NULL){                                      \
                WRAP_OUT.values = malloc(sizeof(sr_val_t));                   \
            } else {                                                          \
                WRAP_OUT.values = realloc(WRAP_OUT.values,                    \
                                          sizeof(sr_val_t) * WRAP_OUT.count); \
            }                                                                 \
            if(WRAP_OUT.values == NULL) {                                     \
                printf("malloc() failed, itr: %d", WRAP_OUT.count);           \
            }                                                                 \
            WRAP_OUT.values[WRAP_OUT.count-1].xpath = XPATH;                  \
            WRAP_OUT.values[WRAP_OUT.count-1].type = TYPE;                    \
            WRAP_OUT.values[WRAP_OUT.count-1].dflt = DFLT;                    \
            WRAP_OUT.values[WRAP_OUT.count-1].data.GET_FIELD(TYPE) = DATA;	\
            WRAP_OUT.values[WRAP_OUT.count-1].origin = ORIGIN;}


extern void
print_change(sr_change_oper_t op,
              sr_val_t *old_val,
              sr_val_t *new_va);

extern const
char * ev_to_str(sr_event_t ev);

extern void
process_module_change(sr_change_oper_t oper,
                      sr_val_t *value);

extern int
module_change_cb(sr_session_ctx_t *session,
		uint32_t sub_id,
                 const char *module_name,
                 const char *xpath,
                 sr_event_t event,
                 uint32_t request_id,
                 void *private_data);
extern int
mplane_oran_software_install(sr_session_ctx_t *session,
				uint32_t sub_id,
                             const char *path,
                             const sr_val_t *input,
                             const size_t input_cnt,
                             sr_event_t event,
                             uint32_t request_id,
                             sr_val_t **output,
                             size_t *output_cnt,
                             void *private_data);

extern int
subscribe_o_ran_rpcs(char *xpath,
                       int(*cb)());

extern int
module_change_subscribe();

extern int
init_sysrepo();

extern char
*read_oper_data_file();

extern int
sw_inventory_get_items_cb(sr_session_ctx_t *session,
                          const char *module_name,
                          const char *xpath,
                          const char *request_xpath,
                          uint32_t request_id,
                          struct lyd_node **parent,
                          void *private_data);

extern void o_ran_app_test();
extern int 
subscribe_notifications(const char *path,const char *node_path,const char *node_val);



extern int
subscribe_operation_data();

extern int
o_ran_lib_init();

extern int
o_ran_lib_deinit();

extern int
mplane_oran_software_activate(sr_session_ctx_t *session,
				uint32_t sub_id,
                              const char *path,
                              const sr_val_t *input,
                              const size_t input_cnt,
                              sr_event_t event,
                              uint32_t request_id,
                              sr_val_t **output,
                              size_t *output_cnt,
                              void *private_data);

extern int
mplane_oran_file_upload(sr_session_ctx_t *session,
			uint32_t sub_id,
                        const char *path,
                        const sr_val_t *input,
                        const size_t input_cnt,
                        sr_event_t event,
                        uint32_t request_id,
                        sr_val_t **output,
                        size_t *output_cnt,
                        void *private_data);

extern int
mplane_oran_retrieve_file_list(sr_session_ctx_t *session,
				uint32_t sub_id,
                               const char *path,
                               const sr_val_t *input,
                               const size_t input_cnt,
                               sr_event_t event,
                               uint32_t request_id,
                               sr_val_t **output,
                               size_t *output_cnt,
                               void *private_data);

extern int
mplane_oran_file_download(sr_session_ctx_t *session,
			uint32_t sub_id,
                          const char *path,
                          const sr_val_t *input,
                          const size_t input_cnt,
                          sr_event_t event,
                          uint32_t request_id,
                          sr_val_t **output,
                          size_t *output_cnt,
                          void *private_data);

extern int
mplane_oran_reset();

extern int
mplane_oran_software_download(sr_session_ctx_t *session,
				uint32_t sub_id,
                              const char *path,
                              const sr_val_t *input,
                              const size_t input_cnt,
                              sr_event_t event,
                              uint32_t request_id,
                              sr_val_t **output,
                              size_t *output_cnt,
                              void *private_data);

extern int
o_ran_lib_sub();

extern int
o_ran_lib_init_ctx();

extern int
o_ran_lib_deinit_ctx();
#if 0
extern int
mplane_oran_software_download(sr_session_ctx_t *session,
                              const char *path,
                              const sr_val_t *input,
                              const size_t input_cnt,
                              sr_event_t event,
                              uint32_t request_id,
                              sr_val_t **output,
                              size_t *output_cnt,
                              void *private_data);
#endif
extern int
mplane_start_ruapp(sr_session_ctx_t *session,
		uint32_t sub_id,	
                   const char *path,
                   const sr_val_t *input,
                   const size_t input_cnt,
                   sr_event_t event,
                   uint32_t request_id,
                   sr_val_t **output,
                   size_t *output_cnt,
                   void *private_data);
extern int
mplane_oran_ald_communication(sr_session_ctx_t *session,
				uint32_t sub_id,
                              const char *path,
                              const sr_val_t *input,
                              const size_t input_cnt,
                              sr_event_t event,
                              uint32_t request_id,
                              sr_val_t **output,
                              size_t *output_cnt,
                              void *private_data);

extern int
module_change_subscribe();

extern int
init_o_ran_lib();

extern int
deinit_o_ran_lib();

extern int
module_change_unsubscribe();


typedef struct {
char object_name[20][MAX_MOD_NAME_LEN];
int count;

}Affected_Objects;

struct oran_alarm{
char fault_id[ALRM_ID_SIZE];
char fault_source[MAX_MOD_NAME_LEN];
Affected_Objects affected_objects;
int cleared;
char alarmRaiseTime[DATE_TIME_SIZE];
SeverityLevel Severity;

};

struct oran_serv oran_srv;

extern struct oran_alarm oran_alarm;
#endif

