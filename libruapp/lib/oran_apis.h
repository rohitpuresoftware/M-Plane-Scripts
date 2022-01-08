/* @fine
*  file description (oran_apis.h): This file contains o-ran specific apis
*
*  Copyright 2021 Puresoftware Ltd
*
*  SPDX-License-Identifier: BSD-3-Clause-Patent
*
*  Based on the files under "https://github.com/sysrepo/sysrepo/blob/master/LICENSE"
*
**/

#ifndef __ORAN_APIS__
#define __ORAN_APIS__

#ifndef __CONFIG__
#include "config.h"
#endif

#include<pthread.h>
#include <inttypes.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
//#ifndef __SUBSCRIBE_ORAN_APIS__
typedef int bool;
//#include"subscribe_oran_apis.h"
//#endif
#define MAX_PATH_LEN 256

#ifndef __SYSREPO__
#include "sysrepo.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <sys/mman.h>


#define SFTP_MAX_RW_SIZE  65536	/*261120 255KB*/

#define DEBUG 1

struct oran_serv {
    sr_conn_ctx_t *sr_conn;         /**< sysrepo connection */
    sr_session_ctx_t *sr_sess;      /**< sysrepo server session */
    sr_subscription_ctx_t *sr_rpc_sub;  /**< sysrepo RPC subscription context */
    sr_subscription_ctx_t *sr_data_sub; /**< sysrepo data subscription context */
    sr_subscription_ctx_t *sr_notif_sub;    /**< sysrepo notification subscription context */
};
extern struct oran_serv oran_srv;
#define SW_MGMT_PATH "/home/user/PureSoftware/MP_3.0/software_management"

///xpath defination for reading data from sysrepo 

#define T2A_MAX_UP "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-max-up"

#define T2A_MIN_UP "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-min-up"

#define T2A_MAX_CP_DL "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-max-cp-dl"

#define T2A_MIN_CP_DL "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-min-cp-dl"

#define TADV_CP_DL "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/tcp-adv-dl"

#define T2A_MIN_CP_UL "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-min-cp-ul"

#define T2A_MAX_CP_UL "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/t2a-max-cp-ul"

#define TA3_MIN "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/ta3-min"

#define TA3_MAX "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/ru-delay-profile/ta3-max"

#define T1A_MAX_UP "/o-ran-delay-management:delay-management/adaptive-delay-configuration/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/o-du-delay-profile/t1a-max-up"

#define T1A_MAX_CP_DL "/o-ran-delay-management:delay-management/adaptive-delay-configuration/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/o-du-delay-profile/t1a-max-cp-dl"

#define TA4_MAX "/o-ran-delay-management:delay-management/adaptive-delay-configuration/bandwidth-scs-delay-state[bandwidth=400000][subcarrier-spacing=120000]/o-du-delay-profile/ta4-max"

#define NDLABSFREPOINTA "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers/center-of-channel-bandwidth"

#define NULABSFREPOINTA "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers/center-of-channel-bandwidth"

#define NDLBANDWIDTH "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers/channel-bandwidth"

#define NULBANDWIDTH "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers/channel-bandwidth"

#define MTUSIZE "/ietf-interfaces:interfaces/interface[name='ps_eth']/l2-mtu"

#define MAXFRAMEID "/o-ran-uplane-conf:user-plane-configuration/static-prach-configurations/prach-patterns/frame-number"

#define PRACHCONFIGINDEX "/o-ran-uplane-conf:user-plane-configuration/static-prach-configurations/static-prach-config-id"

#define XRANMODE "/o-ran-module-cap:module-capability/ru-capabilities/ru-supported-category"



/**
 * @brief Possible types of a data element stored in the sysrepo datastore.
 */
typedef enum notification_type_e {
    /* special types that does not contain any data */
    UNKNOWN_T,

    LIST_T,
    CONTAINER_T,
    CONTAINER_PRESENCE_T,
    LEAF_EMPTY_T,
    NOTIFICATION_T,

    /* types containing some data */
    BINARY_T,
    BITS_T,
    BOOL_T,
    DECIMAL64_T,
    ENUM_T,
    IDENTITYREF_T,
    INSTANCEID_T,
    INT8_T,
    INT16_T,
    INT32_T,
    INT64_T,
    STRING_T,
    UINT8_T,
    UINT16_T,
    UINT32_T,
    UINT64_T,
    ANYXML_T,
    ANYDATA_T
} notification_type_t;

/**
 * @brief Data of an element (if applicable), properly set according to the type.
 */
typedef union notification_data_u {
    char *binary_val;
    char *bits_val;
    bool bool_val;
    double decimal64_val;
    char *enum_val;
    char *identityref_val;
    char *instanceid_val;
    int8_t int8_val;
    int16_t int16_val;
    int32_t int32_val;
    int64_t int64_val;
    char *string_val;
    uint8_t uint8_val;
    uint16_t uint16_val;
    uint32_t uint32_val;
    uint64_t uint64_val;
    char *anyxml_val;
    char *anydata_val;
} notification_data_t;

/**
 * @brief Structure that contains value of an data element stored in the sysrepo datastore.
 */
typedef struct notification_val_s {
    /** [XPath](@ref paths) (or rather path) identifier of the data element. */
    char *xpath;

    /** Type of an element. */
    notification_type_t type;

    /**
     * Flag for node with default value (applicable only for leaves).
     * It is set to TRUE only if the value was *implicitly* set by the datastore as per
     * module schema. Explicitly set/modified data element (through the sysrepo API) always
     * has this flag unset regardless of the entered value.
     */
    bool dflt;

    /** [Origin](@ref oper_ds) of the value. */
    char *origin;

    /** Data of an element (if applicable), properly set according to the type. */
    notification_data_t data;

} notification_val_t;

/**
 * @brief Type of the operation made on an item, used by changeset retrieval in ::notification_get_change_next.
 */
typedef enum notification_change_oper_e {
    OP_CREATED,   /**< The item has been created by the change. */
    OP_MODIFIED,  /**< The value of the item has been modified by the change. */
    OP_DELETED,   /**< The item has been deleted by the change. */
    OP_MOVED      /**< The item has been moved in the subtree by the change (applicable for leaf-lists and user-ordered lists). */
} notification_change_oper_t;


/**
 * @structure to hold the modified data captured by notification subscription
 */
typedef struct modified_data
{
    notification_val_t *old_val;
    notification_val_t *new_val;
    notification_change_oper_t oper;
} modified_data_t;


//structure for alarm 

typedef struct alarm{
	char* fault_id;
	char* fault_source;
	char* fault_severity;
	char* name;
	char* is_cleared;
	char* event_time;
	char* fault_text;
	struct alarm* next; 
}Alarm;


extern Alarm *AlarmListHead;
extern Alarm *AlarmListTail;
typedef enum {
    STARTED = 0,
    SUCCESS = 0,
    COMPLETED=0,
    FAILED = -1,
    FAILURE = -1,
    AUTHENTICATION_ERROR=1,
    PROTOCOL_ERROR,
    FILE_NOT_FOUND,
    APPLICATION_ERROR,
    TIMEOUT,
} status_t;

/**
 * @in and out type can be same for an RPC 
 */

/*
 * @command code to start/stop ruapp
 */
typedef enum {
    START = 0,
    STOP
} system_status_opcode;


/*******for trace START/STOP shared-memory*********/
char *MemForTrace;
#ifndef trace_start
int trace_start;
#endif


/*
 * @command code for software operations
 */
typedef enum {
    SW_DOWNLOAD = 2,
    SW_INSTALL,
    SW_ACTIVATE,
} sw_pkg_opcode;

/*
 * @command code for file operations
 */
typedef enum {
    FILE_UPLOAD = 5,
    FILE_LIST,
    FILE_DOWNLOAD
} file_oper_opcode;

/*
 * @define credential type
 */
typedef enum
{
    PASSWORD = 0,
    CERTIFICATE
} credential_type;


/* 
 * @encryption elogrithms 
 */
typedef enum {
    NONE,       /* "Asymetric key algorithm is NULL."                   */
    rsa1024,    /* "The RSA algorithm using a 1024-bit key."            */
    rsa2048,    /* "The RSA algorithm using a 2048-bit key.";           */
    rsa3072,    /* "The RSA algorithm using a 3072-bit key.";           */
    rsa4096,    /* "The RSA algorithm using a 4096-bit key.";           */
    rsa7680,    /* "The RSA algorithm using a 7680-bit key.";           */
    rsa15360,   /* "The RSA algorithm using a 15360-bit key.";          */
    secp192r1,  /* "The asymmetric algorithm using a NIST P192 Curve."; */
    secp224r1,  /* "The asymmetric algorithm using a NIST P224 Curve."; */
    secp256r1,  /* "The asymmetric algorithm using a NIST P256 Curve."; */
    secp384r1,  /* "The asymmetric algorithm using a NIST P384 Curve."; */
    secp521r1,  /* "The asymmetric algorithm using a NIST P521 Curve."; */
    x25519,     /* "The asymmetric algorithm using a x.25519 Curve.";   */
    x448        /* "The asymmetric algorithm using a x.448 Curve.";     */
} asymmetric_key_algorithm_t;



/*
 * @structure to hold the credential informations as per o-ran yang models
 */
typedef struct keys{
    asymmetric_key_algorithm_t algorithm;
    char *public_key;
} keys_t;

typedef struct server {
    keys_t *keys;
} server_t;

typedef struct password {
    char *_password;
} password_t;

typedef struct cred_password {
    password_t password;
    server_t server;
} cred_password_t;

typedef struct credentials {
    credential_type type;
    cred_password_t cred_password;
} credentials_t;


/*
 * @input & output structure for software download
 */
typedef struct sw_download_input {
    char *remote_file_path;
    credentials_t credentials;
} sw_download_input_t;

typedef struct sw_download_output {
    int status;
    char *error_message;
    uint32_t notification_timeout;
} sw_download_output_t;


/*
 * @input & output structure for software install
 */
typedef struct sw_install_input {
    char *slot_name;
    char *file_names[];
} sw_install_input_t;

typedef struct sw_install_output {
    int status;
    char *error_message;
} sw_install_output_t;


/*
 * @input & output structure for software activate
 */
typedef struct sw_activate_input {
    char *slot_name;
} sw_activate_input_t;

typedef struct sw_activate_output {
    int status;
    char *error_message;
    uint32_t notification_timeout;
} sw_activate_output_t;


/*
 * @input & output structure for all software operations
 */
typedef struct ru_sw_pkg_in {
sr_session_ctx_t *session;   
    sw_pkg_opcode type;
    union {
         sw_download_input_t sw_download_in;
         sw_install_input_t sw_install_in;
         sw_activate_input_t sw_activate_in;
    };
} ru_sw_pkg_in_t;

typedef struct ru_sw_pkg_out {
    sw_pkg_opcode type;
    union {
         sw_download_output_t sw_download_out;
         sw_install_output_t sw_install_out;
	 sw_activate_output_t sw_activate_out;
    };
} ru_sw_pkg_out_t;


/*
 * @structue to specify local and remote path
 */
typedef struct path {
    char *local_logical_file_path;
    char *remote_file_path; // Format:sftp://<username>@<host>[:port]/path
} path_t;


/*
 * @input & output structure for file upload
 */
typedef struct file_upload_input {
    path_t path;
    credentials_t credentials;
} file_upload_input_t;

typedef struct file_upload_output {
   int status;
   char *reject_reason;
} file_upload_output_t;


/*
 * @input & output structure for listing files
 */
typedef struct file_retrieve_input {
   char *logical_path; // ex :  O-RAN/log, o-RAN/PM, O-RAN/transceiver
   char *file_name_filter;
} file_retrieve_input_t;

typedef struct file_retrieve_output {
   int status;
   char *reject_reason;
   char **file_list;
} file_retrieve_output_t;


/*
 * @input & output structure for file download
 */
typedef struct file_download_input {
    path_t path;
    credentials_t credentials;
} file_download_input_t;

typedef struct file_download_output {
   int status;
   char *reject_reason;
} file_download_output_t;


/*
 * @input & output structure for all file operations
 */
typedef struct ru_file_mgmt_in{
sr_session_ctx_t *session;   
 file_oper_opcode type;
    union {
         file_upload_input_t file_upload_in;
	 file_retrieve_input_t file_retrieve_in;
	 file_download_input_t file_download_in;
    };
} ru_file_mgmt_in_t;

typedef struct ru_file_mgmt_out{
    file_oper_opcode type;
    union {
         file_upload_output_t file_upload_out;
	 file_retrieve_output_t file_retrieve_out;
	 file_download_output_t file_download_out;
    };
} ru_file_mgmt_out_t;

/******trace structure ********/

typedef struct ru_trace_out{
	int status;
	char *reject_reason;	

}ru_trace_out_t;


/*
 * @requires if start take input
 *

typedef struct ruapp_start_input {
    char *status;
} ruapp_start_input_t;

*
*/

/*
 * @output structure for ruapp start
 */
typedef struct ruapp_start_output {
   int status;
   char *error_message;
} ruapp_start_output_t;


/*
 * @input/output structure for ruapp start/stop
 */
typedef struct ruapp_switch_in{
    system_status_opcode type;
/*
 * @requires if start take input
 *
    union {
         ruapp_start_input_t ruapp_start_in;
    };
*
*/
} ruapp_switch_in_t;

/*****for trace input/to choose start-trace and stop trace********/
typedef struct ru_trace_switch_in{

sr_session_ctx_t *session;   
system_status_opcode type;
}ru_trace_switch_in_t;


typedef struct ruapp_switch_out{
    system_status_opcode type;
    union {
         ruapp_start_output_t ruapp_start_out;
    };
} ruapp_switch_out_t;


/*
 * @prototype for o-ran specific apis
 */
extern void *file_download_thread(void *);
extern void *file_upload_thread(void *);

extern void *software_download_thread(void *);

extern int ps5g_mplane_change_notification(modified_data_t *in, int count);
extern int ps5g_mplane_software_download(ru_sw_pkg_in_t *in, ru_sw_pkg_out_t **out);
extern int ps5g_mplane_software_install(ru_sw_pkg_in_t *in, ru_sw_pkg_out_t **out);
extern int ps5g_mplane_software_activate(ru_sw_pkg_in_t *in, ru_sw_pkg_out_t **out);
extern int ps5g_mplane_reset();
extern int ps5g_mplane_file_upload(ru_file_mgmt_in_t *in, ru_file_mgmt_out_t **out);
extern int ps5g_mplane_retrieve_file_list(ru_file_mgmt_in_t *in, ru_file_mgmt_out_t **out);
extern int ps5g_mplane_file_download(ru_file_mgmt_in_t *in, ru_file_mgmt_out_t **out);
extern int ps5g_mplane_start_ruapp(ruapp_switch_in_t *in, ruapp_switch_out_t **out);
extern int ps5g_mplane_raise_alarm(sr_session_ctx_t *session,char *node_val[6]);
extern int ps5g_mplane_send_notification(sr_session_ctx_t *session,const char *path,const char *node_path[],const char* node_val[],int cnt);
extern int ps5g_mplane_set_operational_data(sr_session_ctx_t *session,const char *xpath,const char *val);
extern int ps5g_mplane_get_operational_data(sr_session_ctx_t *session,const char *xpath,  sr_val_t **vals);
extern int ps5g_mplane_set_running_data(sr_session_ctx_t *session,const char *xpath,const char *val);
extern int ps5g_mplane_get_running_data(sr_session_ctx_t *session,const char *xpath,  sr_val_t **vals);
extern int ps5g_mplane_send_notifications(sr_session_ctx_t *session,const char *path,const char *node_path,const char *node_val);
extern int ps5g_mplane_remove_alarm(sr_session_ctx_t *session,const char *fault_source);

extern int ps5g_mp_upload_to_remote(ssh_session session,const char *local_file,const char *remote_file,char *error);

extern int ps5g_mp_file_upload_accept(ssh_session session,const char *local_file,const char *remote_file,char *error);

extern int ps5g_mp_download_from_remote(ssh_session session,const char *local_file,const char *remote_file,char *error);
extern int ps5g_mp_download_accept(ssh_session session,const char *local_file,const char *remote_file,char *error);


extern ssh_session ps5g_mp_connect_ssh(const char *host, const char *user,int verbosity,const char *pass,char *error);

extern int 
ps5g_mp_trace_start(ru_trace_switch_in_t *in,ru_trace_out_t **out);

extern int 
ps5g_mp_trace_stop(ru_trace_switch_in_t *in,ru_trace_out_t **out);

extern int ps5g_mp_copy_config_to_startup(sr_session_ctx_t *session,const char *module_name); //copy config from running to startup

extern int ps5g_mp_data_population(sr_session_ctx_t *session, RuntimeConfig* config);
#endif
#if 0
/* to send notification */
int send_notif(const char *path,const char *node_path[],const char* node_val[],int cnt);
/*to raise alarm*/
int raise_alarm(char *node_val[]);

/*to get running data*/
int get_data(const char *xpath,  sr_val_t **vals);//xpath to get value ;vals -> o/p data


/*to set running data*/
int set_data(const char *xpath,const char *val); //xpath ->leaf address to set the value//val -> value in string format							
#endif


