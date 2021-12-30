/**
 * @file sysrepocfg.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief sysrepocfg tool
 *
 * @copyright
 * Copyright (c) 2018 - 2021 Deutsche Telekom AG.
 * Copyright (c) 2018 - 2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unistd.h>

#include <libyang/libyang.h>

#include "bin_common.h"
#include "compat.h"
#include "sysrepo.h"

sr_log_level_t log_level = SR_LL_ERR;

static void
version_print(void)
{
    printf(
            "sysrepocfg - sysrepo configuration manipulation tool, compiled with libsysrepo v%s (SO v%s)\n"
            "\n",
            SR_VERSION, SR_SOVERSION);
}

static void
help_print(void)
{
    printf(
            "Usage:\n"
            "  sysrepocfg <operation> [options]\n"
            "\n"
            "Available operations:\n"
            "  -h, --help                   Print usage help.\n"
            "  -V, --version                Print only information about sysrepo version.\n"
            "  -I, --import[=<path>]        Import the configuration from a file or STDIN.\n"
            "  -X, --export[=<path>]        Export configuration to a file or STDOUT.\n"
            "  -E, --edit[=<path>/<editor>]\n"
            "                               Edit configuration data by merging (applying) a configuration (edit) file or\n"
            "                               by editing the current datastore content using a text editor.\n"
            "  -R, --rpc[=<path>/<editor>]\n"
            "                               Send a RPC/action in a file or using a text editor. Output is printed to STDOUT.\n"
            "  -N, --notification[=<path>/<editor>]\n"
            "                               Send a notification in a file or using a text editor.\n"
            "  -C, --copy-from <path>/<source-datastore>\n"
            "                               Perform a copy-config from a file or a datastore.\n"
            "  -W, --new-data <path>        Set the configuration from a file as the initial one for a new module only scheduled\n"
            "                               to be installed. Is useful for modules with mandatory top-level nodes.\n"
            "\n"
            "       When both a <path> and <editor>/<source-datastore> can be specified, it is always first checked\n"
            "       that the file exists. If not, then it is interpreted as the other parameter.\n"
            "       If no <path> and no <editor> is set, use text editor in $VISUAL or $EDITOR environment variables.\n"
            "\n"
            "Available options:\n"
            "  -d, --datastore <datastore>  Datastore to be operated on, \"running\" by default (\"running\", \"startup\",\n"
            "                               \"candidate\", or \"operational\"). Accepted by import, export, edit,\n"
            "                               copy-from op.\n"
            "  -m, --module <module-name>   Module to be operated on, otherwise it is operated on full datastore.\n"
            "                               Accepted by import, export, edit, copy-from, mandatory for new-data op.\n"
            "  -x, --xpath <xpath>          XPath to select. Accepted by export op.\n"
            "  -f, --format <format>        Data format to be used, by default based on file extension or \"xml\" if not applicable\n"
            "                               (\"xml\", \"json\", or \"lyb\"). Accepted by import, export, edit, rpc,\n"
            "                               notification, copy-from, new-data op.\n"
            "  -l, --lock                   Lock the specified datastore for the whole operation. Accepted by edit op.\n"
            "  -n, --not-strict             Silently ignore any unknown data. Accepted by import, edit, rpc, notification,\n"
            "                               copy-from op.\n"
            "  -o, --opaque                 Parse invalid nodes in the edit into opaque nodes. Accepted by edit op.\n"
            "  -p, --depth <depth>          Limit the depth of returned subtrees, 0 (unlimited) by default. Accepted by\n"
            "                               export op.\n"
            "  -t, --timeout <seconds>      Set the timeout for the operation, otherwise the default one is used.\n"
            "                               Accepted by all op.\n"
            "  -e, --defaults <wd-mode>     Print the default values, which are trimmed by default (\"report-all\",\n"
            "                               \"report-all-tagged\", \"trim\", \"explicit\", \"implicit-tagged\").\n"
            "                               Accepted by export, edit, rpc op.\n"
            "  -v, --verbosity <level>      Change verbosity to a level (none, error, warning, info, debug) or\n"
            "                               number (0, 1, 2, 3, 4).\n"
            "\n");
}

static void
error_print(int sr_error, const char *format, ...)
{
    va_list ap;
    char msg[2048];

    if (!sr_error) {
        sprintf(msg, "sysrepocfg error: %s\n", format);
    } else {
        sprintf(msg, "sysrepocfg error: %s (%s)\n", format, sr_strerror(sr_error));
    }

    va_start(ap, format);
    vfprintf(stderr, msg, ap);
    va_end(ap);

    if (log_level < SR_LL_INF) {
        fprintf(stderr, "For more details you may try to increase the verbosity up to \"-v3\".\n");
    }
}

static void
error_ly_print(const struct ly_ctx *ctx)
{
    struct ly_err_item *e;

    for (e = ly_err_first(ctx); e; e = e->next) {
        fprintf(stderr, "libyang error: %s\n", e->msg);
    }

    ly_err_clean((struct ly_ctx *)ctx, NULL);
}

static int
step_edit_input(const char *editor, const char *path)
{
    int ret;
    pid_t pid, wait_pid;

    /* learn what editor to use */
    if (!editor) {
        editor = getenv("VISUAL");
    }
    if (!editor) {
        editor = getenv("EDITOR");
    }
    if (!editor) {
        error_print(0, "Editor not specified nor read from the environment");
        return EXIT_FAILURE;
    }

    if ((pid = fork()) == -1) {
        error_print(0, "Fork failed (%s)", strerror(errno));
        return EXIT_FAILURE;
    } else if (pid == 0) {
        /* child */
        execlp(editor, editor, path, (char *)NULL);

        error_print(0, "Exec failed (%s)", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        /* parent */
        wait_pid = wait(&ret);
        if (wait_pid != pid) {
            error_print(0, "Child process other than the editor exited, weird");
            return EXIT_FAILURE;
        }
        if (!WIFEXITED(ret)) {
            error_print(0, "Editor exited in a non-standard way");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

static int
step_read_file(FILE *file, char **mem)
{
    size_t mem_size, mem_used;

    mem_size = 512;
    mem_used = 0;
    *mem = malloc(mem_size);

    do {
        if (mem_used == mem_size) {
            mem_size >>= 1;
            *mem = realloc(*mem, mem_size);
        }

        mem_used += fread(*mem + mem_used, 1, mem_size - mem_used, file);
    } while (mem_used == mem_size);

    (*mem)[mem_used] = '\0';

    if (ferror(file)) {
        free(*mem);
        error_print(0, "Error reading from file (%s)", strerror(errno));
        return EXIT_FAILURE;
    } else if (!feof(file)) {
        free(*mem);
        error_print(0, "Unknown file problem");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

enum data_type {
    DATA_CONFIG,
    DATA_EDIT,
    DATA_RPC,
    DATA_NOTIF
};

static int
step_load_data(sr_session_ctx_t *sess, const char *file_path, LYD_FORMAT format, enum data_type data_type,
        int not_strict, int opaq, struct lyd_node **data)
{
    const struct ly_ctx *ly_ctx;
    struct ly_in *in;
    char *ptr;
    int parse_flags;
    LY_ERR lyrc = 0;

    ly_ctx = sr_get_context(sr_session_get_connection(sess));

    /* learn format */
    if (format == LYD_UNKNOWN) {
        if (!file_path) {
            error_print(0, "When reading data from STDIN, format must be specified");
            return EXIT_FAILURE;
        }

        ptr = strrchr(file_path, '.');
        if (ptr && !strcmp(ptr, ".xml")) {
            format = LYD_XML;
        } else if (ptr && !strcmp(ptr, ".json")) {
            format = LYD_JSON;
        } else if (ptr && !strcmp(ptr, ".lyb")) {
            format = LYD_LYB;
        } else {
            error_print(0, "Failed to detect format of \"%s\"", file_path);
            return EXIT_FAILURE;
        }
    }

    /* get input */
    if (file_path) {
        if (ly_in_new_filepath(file_path, 0, &in)) {
            /* empty file */
            ptr = strdup("");
            ly_in_new_memory(ptr, &in);
        }
    } else {
        /* we need to load the data into memory first */
        if (step_read_file(stdin, &ptr)) {
            return EXIT_FAILURE;
        }
        ly_in_new_memory(ptr, &in);
    }

    /* parse data */
    switch (data_type) {
    case DATA_CONFIG:
        parse_flags = LYD_PARSE_NO_STATE | LYD_PARSE_ONLY | (not_strict ? 0 : LYD_PARSE_STRICT);
        lyrc = lyd_parse_data(ly_ctx, NULL, in, format, parse_flags, 0, data);
        break;
    case DATA_EDIT:
        parse_flags = LYD_PARSE_NO_STATE | LYD_PARSE_ONLY | (opaq ? LYD_PARSE_OPAQ : (not_strict ? 0 : LYD_PARSE_STRICT));
        lyrc = lyd_parse_data(ly_ctx, NULL, in, format, parse_flags, 0, data);
        break;
    case DATA_RPC:
        lyrc = lyd_parse_op(ly_ctx, NULL, in, format, LYD_TYPE_RPC_YANG, data, NULL);
        break;
    case DATA_NOTIF:
        lyrc = lyd_parse_op(ly_ctx, NULL, in, format, LYD_TYPE_NOTIF_YANG, data, NULL);
        break;
    }
    ly_in_free(in, 1);

    if (lyrc) {
        error_ly_print(ly_ctx);
        error_print(0, "Data parsing failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
step_create_input_file(LYD_FORMAT format, char *tmp_file)
{
    int fd;

    if (format == LYD_LYB) {
        error_print(0, "LYB binary format cannot be opened in a text editor");
        return EXIT_FAILURE;
    } else if (format == LYD_UNKNOWN) {
        format = LYD_XML;
    }

#ifdef SR_HAVE_MKSTEMPS
    int suffix;

    /* create temporary file, suffix is used only so that the text editor
     * can automatically use syntax highlighting */
    if (format == LYD_JSON) {
        sprintf(tmp_file, "/tmp/srtmpXXXXXX.json");
        suffix = 5;
    } else {
        sprintf(tmp_file, "/tmp/srtmpXXXXXX.xml");
        suffix = 4;
    }
    fd = mkstemps(tmp_file, suffix);
#else
    sprintf(tmp_file, "/tmp/srtmpXXXXXX");
    fd = mkstemp(tmp_file);
#endif
    if (fd == -1) {
        error_print(0, "Failed to open temporary file (%s)", strerror(errno));
        return EXIT_FAILURE;
    }
    close(fd);

    return EXIT_SUCCESS;
}

static int
op_import(sr_session_ctx_t *sess, const char *file_path, const char *module_name, LYD_FORMAT format, int not_strict,
        int timeout_s)
{
    struct lyd_node *data;
    int r;

    if (step_load_data(sess, file_path, format, DATA_CONFIG, not_strict, 0, &data)) {
        return EXIT_FAILURE;
    }

    /* replace config (always spends data) */
    r = sr_replace_config(sess, module_name, data, timeout_s * 1000);
    if (r) {
        error_print(r, "Replace config failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
op_export(sr_session_ctx_t *sess, const char *file_path, const char *module_name, const char *xpath, LYD_FORMAT format,
        uint32_t max_depth, int wd_opt, int timeout_s)
{
    struct lyd_node *data;
    FILE *file = NULL;
    char *str;
    int r;

    if (format == LYD_UNKNOWN) {
        format = LYD_XML;
    }

    if (file_path) {
        file = fopen(file_path, "w");
        if (!file) {
            error_print(0, "Failed to open \"%s\" for writing (%s)", file_path, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* get subtrees */
    if (module_name) {
        if (asprintf(&str, "/%s:*", module_name) == -1) {
            r = SR_ERR_NO_MEMORY;
        } else {
            r = sr_get_data(sess, str, max_depth, timeout_s * 1000, 0, &data);
            free(str);
        }
    } else if (xpath) {
        r = sr_get_data(sess, xpath, max_depth, timeout_s * 1000, 0, &data);
    } else {
        r = sr_get_data(sess, "/*", max_depth, timeout_s * 1000, 0, &data);
    }
    if (r != SR_ERR_OK) {
        error_print(r, "Getting data failed");
        if (file) {
            fclose(file);
        }
        return EXIT_FAILURE;
    }

    /* print exported data */
    lyd_print_file(file ? file : stdout, data, format, LYD_PRINT_WITHSIBLINGS | wd_opt);
    lyd_free_all(data);

    /* cleanup */
    if (file) {
        fclose(file);
    }
    return EXIT_SUCCESS;
}

static int
op_edit(sr_session_ctx_t *sess, const char *file_path, const char *editor, const char *module_name, LYD_FORMAT format,
        int lock, int not_strict, int opaq, int wd_opt, int timeout_s)
{
    char tmp_file[22];
    int r, rc = EXIT_FAILURE;
    struct lyd_node *data;

    if (file_path) {
        /* just apply an edit from a file */
        if (step_load_data(sess, file_path, format, DATA_EDIT, not_strict, opaq, &data)) {
            return EXIT_FAILURE;
        }

        if (!data) {
            error_print(0, "No parsed data");
            return EXIT_FAILURE;
        }

        r = sr_edit_batch(sess, data, "merge");
        lyd_free_all(data);
        if (r != SR_ERR_OK) {
            error_print(r, "Failed to prepare edit");
            return EXIT_FAILURE;
        }

        r = sr_apply_changes(sess, timeout_s * 1000);
        if (r != SR_ERR_OK) {
            error_print(r, "Failed to merge edit data");
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    /* create temporary file */
    if (step_create_input_file(format, tmp_file)) {
        return EXIT_FAILURE;
    }

    /* lock if requested */
    if (lock && ((r = sr_lock(sess, module_name)) != SR_ERR_OK)) {
        error_print(r, "Lock failed");
        return EXIT_FAILURE;
    }

    /* use export operation to get data to edit */
    if (op_export(sess, tmp_file, module_name, NULL, format, 0, wd_opt, timeout_s)) {
        goto cleanup_unlock;
    }

    /* edit */
    if (step_edit_input(editor, tmp_file)) {
        goto cleanup_unlock;
    }

    /* use import operation to store edited data */
    if (op_import(sess, tmp_file, module_name, format, not_strict, timeout_s)) {
        goto cleanup_unlock;
    }

    /* success */
    rc = EXIT_SUCCESS;

cleanup_unlock:
    if (lock && ((r = sr_unlock(sess, module_name)) != SR_ERR_OK)) {
        error_print(r, "Unlock failed");
    }
    return rc;
}

static int
op_rpc(sr_session_ctx_t *sess, const char *file_path, const char *editor, LYD_FORMAT format, int wd_opt, int timeout_s)
{
    char tmp_file[22];
    int r;
    struct lyd_node *input, *output, *node;

    if (!file_path) {
        /* create temp file */
        if (step_create_input_file(format, tmp_file)) {
            return EXIT_FAILURE;
        }

        /* load rpc/action into the file */
        if (step_edit_input(editor, tmp_file)) {
            return EXIT_FAILURE;
        }

        file_path = tmp_file;
    }

    /* load the file */
    if (step_load_data(sess, file_path, format, DATA_RPC, 0, 0, &input)) {
        return EXIT_FAILURE;
    }

    /* send rpc/action */
    r = sr_rpc_send_tree(sess, input, timeout_s * 1000, &output);
    lyd_free_all(input);
    if (r) {
        error_print(r, "Sending RPC/action failed");
        return EXIT_FAILURE;
    }

    /* print output if any */
    LY_LIST_FOR(lyd_child(output), node) {
        if (!(node->flags & LYD_DEFAULT)) {
            break;
        }
    }
    if (node) {
        lyd_print_file(stdout, output, format, wd_opt);
    }
    lyd_free_all(output);

    return EXIT_SUCCESS;
}

static int
op_notif(sr_session_ctx_t *sess, const char *file_path, const char *editor, LYD_FORMAT format)
{
    char tmp_file[22];
    int r;
    struct lyd_node *notif;

    if (!file_path) {
        /* create temp file */
        if (step_create_input_file(format, tmp_file)) {
            return EXIT_FAILURE;
        }

        /* load notif into the file */
        if (step_edit_input(editor, tmp_file)) {
            return EXIT_FAILURE;
        }

        file_path = tmp_file;
    }

    /* load the file */
    if (step_load_data(sess, file_path, format, DATA_NOTIF, 0, 0, &notif)) {
        return EXIT_FAILURE;
    }

    /* send notification */
    r = sr_event_notif_send_tree(sess, notif, 0, 1);
    lyd_free_all(notif);
    if (r) {
        error_print(r, "Sending notification failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
op_copy(sr_session_ctx_t *sess, const char *file_path, sr_datastore_t source_ds, const char *module_name,
        LYD_FORMAT format, int not_strict, int timeout_s)
{
    int r;
    struct lyd_node *data;

    if (file_path) {
        /* load the file */
        if (step_load_data(sess, file_path, format, DATA_CONFIG, not_strict, 0, &data)) {
            return EXIT_FAILURE;
        }

        /* replace data */
        r = sr_replace_config(sess, module_name, data, timeout_s * 1000);
        if (r) {
            error_print(r, "Replace config failed");
            return EXIT_FAILURE;
        }
    } else {
        /* copy config */
        r = sr_copy_config(sess, module_name, source_ds, timeout_s * 1000);
        if (r) {
            error_print(r, "Copy config failed");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

static int
op_new_data(sr_conn_ctx_t *conn, const char *file_path, const char *module_name, LYD_FORMAT format)
{
    int r;

    /* set the initial data */
    r = sr_install_module_data(conn, module_name, NULL, file_path, format);
    if (r) {
        error_print(r, "Install module data failed");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
arg_is_file(const char *optarg)
{
    return !access(optarg, F_OK);
}

static int
arg_get_ds(const char *optarg, sr_datastore_t *ds)
{
    if (!strcmp(optarg, "running")) {
        *ds = SR_DS_RUNNING;
    } else if (!strcmp(optarg, "startup")) {
        *ds = SR_DS_STARTUP;
    } else if (!strcmp(optarg, "candidate")) {
        *ds = SR_DS_CANDIDATE;
    } else if (!strcmp(optarg, "operational")) {
        *ds = SR_DS_OPERATIONAL;
    } else {
        error_print(0, "Unknown datastore \"%s\"", optarg);
        return -1;
    }

    return 0;
}

int
main(int argc, char **argv)
{
    sr_conn_ctx_t *conn = NULL;
    sr_session_ctx_t *sess = NULL;
    sr_datastore_t ds = SR_DS_RUNNING, source_ds;
    LYD_FORMAT format = LYD_UNKNOWN;
    const char *module_name = NULL, *editor = NULL, *file_path = NULL, *xpath = NULL, *op_str;
    char *ptr;
    int r, rc = EXIT_FAILURE, opt, operation = 0, lock = 0, not_strict = 0, opaq = 0, timeout = 0, wd_opt = 0;
    uint32_t max_depth = 0;
    struct option options[] = {
        {"help",            no_argument,       NULL, 'h'},
        {"version",         no_argument,       NULL, 'V'},
        {"import",          optional_argument, NULL, 'I'},
        {"export",          optional_argument, NULL, 'X'},
        {"edit",            optional_argument, NULL, 'E'},
        {"rpc",             optional_argument, NULL, 'R'},
        {"notification",    optional_argument, NULL, 'N'},
        {"copy-from",       required_argument, NULL, 'C'},
        {"new-data",        required_argument, NULL, 'W'},
        {"datastore",       required_argument, NULL, 'd'},
        {"module",          required_argument, NULL, 'm'},
        {"xpath",           required_argument, NULL, 'x'},
        {"format",          required_argument, NULL, 'f'},
        {"lock",            no_argument,       NULL, 'l'},
        {"not-strict",      no_argument,       NULL, 'n'},
        {"opaque",          no_argument,       NULL, 'o'},
        {"depth",           required_argument, NULL, 'p'},
        {"timeout",         required_argument, NULL, 't'},
        {"defaults",        required_argument, NULL, 'e'},
        {"verbosity",       required_argument, NULL, 'v'},
        {NULL,              0,                 NULL, 0},
    };

    if (argc == 1) {
        help_print();
        goto cleanup;
    }

    /* process options */
    opterr = 0;
    while ((opt = getopt_long(argc, argv, "hVI::X::E::R::N::C:W:d:m:x:f:lnop:t:e:v:", options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            version_print();
            help_print();
            rc = EXIT_SUCCESS;
            goto cleanup;
        case 'V':
            version_print();
            rc = EXIT_SUCCESS;
            goto cleanup;
        case 'I':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (optarg) {
                file_path = optarg;
            }
            operation = opt;
            break;
        case 'X':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (optarg) {
                file_path = optarg;
            }
            operation = opt;
            break;
        case 'E':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (optarg) {
                if (arg_is_file(optarg)) {
                    file_path = optarg;
                } else {
                    editor = optarg;
                }
            }
            operation = opt;
            break;
        case 'R':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (optarg) {
                if (arg_is_file(optarg)) {
                    file_path = optarg;
                } else {
                    editor = optarg;
                }
            }
            operation = opt;
            break;
        case 'N':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (optarg) {
                if (arg_is_file(optarg)) {
                    file_path = optarg;
                } else {
                    editor = optarg;
                }
            }
            operation = opt;
            break;
        case 'C':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            if (arg_is_file(optarg)) {
                file_path = optarg;
            } else {
                if (arg_get_ds(optarg, &source_ds)) {
                    goto cleanup;
                }
            }
            operation = opt;
            break;
        case 'W':
            if (operation) {
                error_print(0, "Operation already specified");
                goto cleanup;
            }
            file_path = optarg;
            operation = opt;
            break;
        case 'd':
            if (arg_get_ds(optarg, &ds)) {
                goto cleanup;
            }
            break;
        case 'm':
            if (module_name) {
                error_print(0, "Module already specified");
                goto cleanup;
            } else if (xpath) {
                error_print(0, "Only one of options --module and --xpath can be set");
                goto cleanup;
            }
            module_name = optarg;
            break;
        case 'x':
            if (xpath) {
                error_print(0, "XPath already specified");
                goto cleanup;
            } else if (module_name) {
                error_print(0, "Only one of options --module and --xpath can be set");
                goto cleanup;
            }
            xpath = optarg;
            break;
        case 'f':
            if (!strcmp(optarg, "xml")) {
                format = LYD_XML;
            } else if (!strcmp(optarg, "json")) {
                format = LYD_JSON;
            } else if (!strcmp(optarg, "lyb")) {
                format = LYD_LYB;
            } else {
                error_print(0, "Unknown format \"%s\"", optarg);
                goto cleanup;
            }
            break;
        case 'l':
            lock = 1;
            break;
        case 'n':
            not_strict = 1;
            break;
        case 'o':
            opaq = 1;
            break;
        case 'p':
            max_depth = strtoul(optarg, &ptr, 10);
            if (ptr[0]) {
                error_print(0, "Invalid depth \"%s\"", optarg);
                goto cleanup;
            }
            break;
        case 't':
            timeout = strtoul(optarg, &ptr, 10);
            if (ptr[0]) {
                error_print(0, "Invalid timeout \"%s\"", optarg);
                goto cleanup;
            }
            break;
        case 'e':
            if (!strcmp(optarg, "report-all")) {
                wd_opt = LYD_PRINT_WD_ALL;
            } else if (!strcmp(optarg, "report-all-tagged")) {
                wd_opt = LYD_PRINT_WD_ALL_TAG;
            } else if (!strcmp(optarg, "trim")) {
                wd_opt = LYD_PRINT_WD_TRIM;
            } else if (!strcmp(optarg, "explicit")) {
                wd_opt = LYD_PRINT_WD_EXPLICIT;
            } else if (!strcmp(optarg, "implicit-tagged")) {
                wd_opt = LYD_PRINT_WD_IMPL_TAG;
            } else {
                error_print(0, "Invalid defaults mode \"%s\"", optarg);
                goto cleanup;
            }
            break;
        case 'v':
            if (!strcmp(optarg, "none")) {
                log_level = SR_LL_NONE;
            } else if (!strcmp(optarg, "error")) {
                log_level = SR_LL_ERR;
            } else if (!strcmp(optarg, "warning")) {
                log_level = SR_LL_WRN;
            } else if (!strcmp(optarg, "info")) {
                log_level = SR_LL_INF;
            } else if (!strcmp(optarg, "debug")) {
                log_level = SR_LL_DBG;
            } else if ((strlen(optarg) == 1) && (optarg[0] >= '0') && (optarg[0] <= '4')) {
                log_level = atoi(optarg);
            } else {
                error_print(0, "Invalid verbosity \"%s\"", optarg);
                goto cleanup;
            }
            break;
        default:
            error_print(0, "Invalid option or missing argument: -%c", optopt);
            goto cleanup;
        }
    }

    /* check for additional argument */
    if (optind < argc) {
        error_print(0, "Redundant parameters (%s)", argv[optind]);
        goto cleanup;
    }

    /* check if operation on the datastore is supported */
    if (ds == SR_DS_OPERATIONAL) {
        switch (operation) {
        case 'I':
            op_str = "Import";
            break;
        case 'E':
            op_str = "Edit";
            break;
        case 'C':
            op_str = "Copy-config";
            break;
        default:
            op_str = NULL;
            break;
        }

        if (op_str) {
            error_print(0, "%s operation on operational DS not supported, changes would be lost after session is terminated", op_str);
            goto cleanup;
        }
    }

    /* set logging */
    sr_log_stderr(log_level);

    /* create connection */
    if ((r = sr_connect(0, &conn)) != SR_ERR_OK) {
        error_print(r, "Failed to connect");
        goto cleanup;
    }

    /* create session */
    if ((r = sr_session_start(conn, ds, &sess)) != SR_ERR_OK) {
        error_print(r, "Failed to start a session");
        goto cleanup;
    }

    /* perform the operation */
    switch (operation) {
    case 'I':
        rc = op_import(sess, file_path, module_name, format, not_strict, timeout);
        break;
    case 'X':
        rc = op_export(sess, file_path, module_name, xpath, format, max_depth, wd_opt, timeout);
        break;
    case 'E':
        rc = op_edit(sess, file_path, editor, module_name, format, lock, not_strict, opaq, wd_opt, timeout);
        break;
    case 'R':
        rc = op_rpc(sess, file_path, editor, format, wd_opt, timeout);
        break;
    case 'N':
        rc = op_notif(sess, file_path, editor, format);
        break;
    case 'C':
        rc = op_copy(sess, file_path, source_ds, module_name, format, not_strict, timeout);
        break;
    case 'W':
        if (!module_name) {
            error_print(0, "Module must be specified when setting its initial data");
            break;
        } else if (!format) {
            error_print(0, "Format of the file must be specified when setting initial data");
            break;
        }
        rc = op_new_data(conn, file_path, module_name, format);
        break;
    case 0:
        error_print(0, "No operation specified");
        break;
    default:
        error_print(0, "Internal");
        break;
    }

cleanup:
    sr_session_stop(sess);
    sr_disconnect(conn);
    return rc;
}
