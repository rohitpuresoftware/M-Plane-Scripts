/**
 * @file edit_diff.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief routines for sysrepo edit and diff data tree handling
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

#include "edit_diff.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <libyang/libyang.h>

#include "common.h"
#include "compat.h"
#include "log.h"

enum insert_val {
    INSERT_DEFAULT = 0,
    INSERT_FIRST,
    INSERT_LAST,
    INSERT_BEFORE,
    INSERT_AFTER
};

/**
 * @brief Return operation from a string.
 *
 * @param[in] str Operation in string.
 * @return Operation.
 */
static enum edit_op
sr_edit_str2op(const char *str)
{
    assert(str);

    switch (str[0]) {
    case 'e':
        assert(!strcmp(str, "ether"));
        return EDIT_ETHER;
    case 'n':
        assert(!strcmp(str, "none"));
        return EDIT_NONE;
    case 'm':
        assert(!strcmp(str, "merge"));
        return EDIT_MERGE;
    case 'r':
        if (str[2] == 'p') {
            assert(!strcmp(str, "replace"));
            return EDIT_REPLACE;
        }
        assert(!strcmp(str, "remove"));
        return EDIT_REMOVE;
    case 'c':
        assert(!strcmp(str, "create"));
        return EDIT_CREATE;
    case 'd':
        assert(!strcmp(str, "delete"));
        return EDIT_DELETE;
    case 'p':
        assert(!strcmp(str, "purge"));
        return EDIT_PURGE;
    default:
        break;
    }

    assert(0);
    return 0;
}

/**
 * @brief Learn the operation of an edit node.
 *
 * @param[in] edit_node Edit node to inspect.
 * @param[in] parent_op Parent operation.
 * @param[out] op Edit node operation.
 * @param[out] insert Optional insert place of the operation.
 * @param[out] userord_anchor Optional user-ordered anchor of relative (leaf-)list instance for the operation.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_op(const struct lyd_node *edit_node, enum edit_op parent_op, enum edit_op *op, enum insert_val *insert,
        const char **userord_anchor)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_meta *meta;
    struct lyd_attr *attr;
    enum insert_val ins = INSERT_DEFAULT;
    const char *meta_name = NULL, *meta_anchor = NULL, *val_str;
    int user_order_list = 0;

    *op = parent_op;
    if (lysc_is_userordered(edit_node->schema)) {
        user_order_list = 1;
    }

    if (edit_node->schema) {
        if (user_order_list) {
            if (lysc_is_dup_inst_list(edit_node->schema)) {
                meta_name = "position";
            } else if (edit_node->schema->nodetype == LYS_LIST) {
                meta_name = "key";
            } else {
                meta_name = "value";
            }
        }

        LY_LIST_FOR(edit_node->meta, meta) {
            val_str = lyd_get_meta_value(meta);
            if (!strcmp(meta->name, "operation") && (!strcmp(meta->annotation->module->name, "sysrepo") ||
                    !strcmp(meta->annotation->module->name, "ietf-netconf"))) {
                *op = sr_edit_str2op(val_str);
            } else if (user_order_list && !strcmp(meta->name, "insert") && !strcmp(meta->annotation->module->name, "yang")) {
                if (!strcmp(val_str, "first")) {
                    ins = INSERT_FIRST;
                } else if (!strcmp(val_str, "last")) {
                    ins = INSERT_LAST;
                } else if (!strcmp(val_str, "before")) {
                    ins = INSERT_BEFORE;
                } else if (!strcmp(val_str, "after")) {
                    ins = INSERT_AFTER;
                } else {
                    SR_ERRINFO_INT(&err_info);
                    return err_info;
                }
            } else if (user_order_list && !strcmp(meta->name, meta_name) && !strcmp(meta->annotation->module->name, "yang")) {
                meta_anchor = val_str;
            }
        }
    } else {
        LY_LIST_FOR(((struct lyd_node_opaq *)edit_node)->attr, attr) {
            if (!strcmp(attr->name.name, "operation")) {
                /* try to create a metadata instance and use that */
                uint32_t prev_lo = ly_log_options(0);
                if (!lyd_new_meta2(LYD_CTX(edit_node), NULL, 0, attr, &meta)) {
                    if (!strcmp(meta->annotation->module->name, "sysrepo") ||
                            !strcmp(meta->annotation->module->name, "ietf-netconf")) {
                        *op = sr_edit_str2op(lyd_get_meta_value(meta));
                    }
                    lyd_free_meta_single(meta);
                }
                ly_log_options(prev_lo);
            }
        }
    }

    if (user_order_list && ((ins == INSERT_BEFORE) || (ins == INSERT_AFTER)) && !(meta_anchor)) {
        sr_errinfo_new(&err_info, SR_ERR_VALIDATION_FAILED, "Missing attribute \"%s\" required by the \"insert\" attribute.",
                meta_name);
        return err_info;
    }

    if (insert) {
        *insert = ins;
    }
    if (userord_anchor) {
        *userord_anchor = meta_anchor;
    }
    return NULL;
}

/**
 * @brief Find CID meta of an edit node or its parents.
 *
 * @param[in] edit Edit node.
 * @param[out] cid Found stored CID, 0 if none found.
 * @param[out] meta_own Whether @p pid and @p conn_ptr are own or inherited.
 * @return err_info, NULL on success.
 */
static void
sr_edit_find_cid(struct lyd_node *edit, sr_cid_t *cid, int *meta_own)
{
    struct lyd_node *parent;
    struct lyd_meta *cid_meta = NULL;
    struct lyd_attr *attr;

    if (cid) {
        *cid = 0;
    }
    if (meta_own) {
        *meta_own = 0;
    }

    if (!edit) {
        return;
    }

    for (parent = edit; parent; parent = lyd_parent(parent)) {
        if (parent->schema) {
            /* data node with metadata */
            cid_meta = lyd_find_meta(parent->meta, NULL, "sysrepo:cid");
            if (cid_meta) {
                /* found */
                if (cid) {
                    *cid = cid_meta->value.uint32;
                }
                if (meta_own && (parent == edit) && cid_meta) {
                    *meta_own = 1;
                }
                break;
            }
        } else {
            /* opaque node with attributes */
            LY_LIST_FOR(((struct lyd_node_opaq *)parent)->attr, attr) {
                if (strcmp(attr->name.name, "cid")) {
                    continue;
                }
                if ((attr->format == LY_VALUE_XML) && strcmp(attr->name.module_ns, "urn:ietf:params:xml:ns:yang:1")) {
                    continue;
                }
                if ((attr->format == LY_VALUE_JSON) && strcmp(attr->name.module_name, "yang")) {
                    continue;
                }

                /* found */
                if (cid) {
                    *cid = atoi(attr->value);
                }
                if (meta_own && (parent == edit)) {
                    *meta_own = 1;
                }
                break;
            }
        }

        if (!cid) {
            /* no recursive check */
            break;
        }
    }
}

/**
 * @brief Update (inherited) CID meta of an edit node.
 *
 * @param[in] edit_node Edit node to examine.
 * @param[in] cid CID of the edit merge source (new owner of these oper edit nodes).
 * @param[in] keep_cur_child Whether to keep current meta for direct children.
 * @param[in] changed Optional flag that the CID was changed.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_update_cid(struct lyd_node *edit_node, sr_cid_t cid, int keep_cur_child, int *changed)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *child;
    int meta_own;
    char cid_str[21];
    sr_cid_t cur_cid, child_cid;

    assert(cid);
    if (changed) {
        *changed = 0;
    }

    /* learn current CID */
    sr_edit_find_cid(edit_node, &cur_cid, &meta_own);

    /* it may need to be set for children */
    child_cid = cur_cid;

    if (!cur_cid || (cur_cid != cid)) {
        if (meta_own) {
            /* remove meta from the node */
            sr_edit_del_meta_attr(edit_node, "cid");

            /* effective CID changed */
            sr_edit_find_cid(edit_node, &cur_cid, NULL);
        }

        if (cur_cid != cid) {
            /* add meta of the new connection */
            sprintf(cid_str, "%" PRIu32, cid);
            if (lyd_new_meta(LYD_CTX(edit_node), edit_node, NULL, "sysrepo:cid", cid_str, 0, NULL)) {
                sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
                return err_info;
            }

            if (changed) {
                *changed = 1;
            }
        }

        if (!keep_cur_child || !child_cid) {
            /* there was no CID before so ignore keep_cur_child */
            return NULL;
        }

        /* keep meta of the current connection for children */
        sprintf(cid_str, "%" PRIu32, child_cid);

        LY_LIST_FOR(lyd_child_no_keys(edit_node), child) {
            sr_edit_find_cid(child, NULL, &meta_own);
            if (!meta_own) {
                if (lyd_new_meta(LYD_CTX(edit_node), child, NULL, "sysrepo:cid", cid_str, 0, NULL)) {
                    sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
                    return err_info;
                }
            }
        }
    }

    return NULL;
}

/**
 * @brief Remove any previous metadata in target and copy them from source instead.
 * Handled metadata: operation, insert, position/key/value
 *
 * @param[in] src_node Source edit node.
 * @param[in,out] trg_node Target edit node.
 * @param[in] trg_op_own Whether @p trg_node has its own operation metadata.
 * @param[in] src_op Operation of @p src_node.
 * @param[out] meta_changed Whether any of the metadata (except operation) values differ.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_copy_meta(const struct lyd_node *src_node, struct lyd_node *trg_node, int trg_op_own, enum edit_op src_op,
        int *meta_changed)
{
    sr_error_info_t *err_info = NULL;
    const struct lys_module *yang_mod;
    enum insert_val insert;
    const char *userord_anchor, *anchor_meta_name = NULL, *insert_str = NULL, *orig_insert = NULL, *orig_anchor = NULL;
    struct lyd_meta *meta;

    *meta_changed = 0;

    yang_mod = ly_ctx_get_module_implemented(LYD_CTX(trg_node), "yang");
    SR_CHECK_INT_GOTO(!yang_mod, err_info, cleanup);

    /* remove current operation */
    if (trg_op_own) {
        sr_edit_del_meta_attr(trg_node, "operation");
    }

    /* remove current insert */
    meta = lyd_find_meta(trg_node->meta, yang_mod, "insert");
    orig_insert = lyd_get_meta_value(meta);
    lyd_free_meta_single(meta);

    /* remove current anchor */
    if (lysc_is_userordered(trg_node->schema)) {
        if (lysc_is_dup_inst_list(trg_node->schema)) {
            anchor_meta_name = "position";
        } else if (trg_node->schema->nodetype == LYS_LIST) {
            anchor_meta_name = "key";
        } else {
            anchor_meta_name = "value";
        }
        meta = lyd_find_meta(trg_node->meta, yang_mod, anchor_meta_name);
        orig_anchor = lyd_get_meta_value(meta);
        lyd_free_meta_single(meta);
    }

    /* get src_node metadata */
    if ((err_info = sr_edit_op(src_node, src_op, &src_op, &insert, &userord_anchor))) {
        goto cleanup;
    }

    /* copy operation */
    if (src_op == EDIT_ETHER) {
        if (lyd_new_meta(LYD_CTX(trg_node), trg_node, NULL, "sysrepo:operation", sr_edit_op2str(src_op), 0, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(trg_node));
            goto cleanup;
        }
    } else {
        if (lyd_new_meta(LYD_CTX(trg_node), trg_node, NULL, "ietf-netconf:operation", sr_edit_op2str(src_op), 0, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(trg_node));
            goto cleanup;
        }
    }

    /* copy any insert and anchor meta */
    switch (insert) {
    case INSERT_DEFAULT:
        insert_str = NULL;
        break;
    case INSERT_FIRST:
        insert_str = "first";
        break;
    case INSERT_LAST:
        insert_str = "last";
        break;
    case INSERT_BEFORE:
        insert_str = "before";
        break;
    case INSERT_AFTER:
        insert_str = "after";
        break;
    }
    if (insert_str && lyd_new_meta(LYD_CTX(trg_node), trg_node, yang_mod, "insert", insert_str, 0, NULL)) {
        sr_errinfo_new_ly(&err_info, LYD_CTX(trg_node));
        goto cleanup;
    }
    if ((insert_str && !orig_insert) || (!insert_str && orig_insert) ||
            (insert_str && orig_insert && strcmp(insert_str, orig_insert))) {
        *meta_changed = 1;
    }

    assert(!userord_anchor || anchor_meta_name);
    if (userord_anchor && lyd_new_meta(LYD_CTX(trg_node), trg_node, yang_mod, anchor_meta_name, userord_anchor, 0, NULL)) {
        sr_errinfo_new_ly(&err_info, LYD_CTX(trg_node));
        goto cleanup;
    }
    if ((userord_anchor && !orig_anchor) || (!userord_anchor && orig_anchor) ||
            (userord_anchor && orig_anchor && strcmp(userord_anchor, orig_anchor))) {
        *meta_changed = 1;
    }

cleanup:
    return err_info;
}

/**
 * @brief Transform edit operation into diff operation.
 *
 * @param[in] op Edit operation.
 * @return Diff operation.
 */
static enum edit_op
sr_op_edit2diff(enum edit_op op)
{
    switch (op) {
    case EDIT_ETHER:
    case EDIT_NONE:
        return EDIT_NONE;
    case EDIT_MERGE:
    case EDIT_CREATE:
        return EDIT_CREATE;
    case EDIT_REPLACE:
        return EDIT_REPLACE;
    case EDIT_PURGE:
    case EDIT_DELETE:
    case EDIT_REMOVE:
        return EDIT_DELETE;
    default:
        break;
    }

    assert(0);
    return 0;
}

/**
 * @brief Add diff metadata.
 *
 * @param[in] diff_node Diff node to change.
 * @param[in] meta_val Metadata value (meaning depends on the nodetype).
 * @param[in] prev_meta_val Previous metadata value (meaning depends on the nodetype).
 * @param[in] op Diff operation.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_diff_add_meta(struct lyd_node *diff_node, const char *meta_val, const char *prev_meta_val, enum edit_op op)
{
    sr_error_info_t *err_info = NULL;
    enum edit_op cur_op;

    assert((op == EDIT_CREATE) || (op == EDIT_DELETE) || (op == EDIT_REPLACE) || (op == EDIT_NONE));

    /* add operation if needed */
    cur_op = sr_edit_diff_find_oper(diff_node, 1, NULL);
    if ((cur_op != op) && (err_info = sr_diff_set_oper(diff_node, sr_edit_op2str(op)))) {
        return err_info;
    }

    switch (op) {
    case EDIT_NONE:
        /* add attributes for the special dflt-only change */
        if (diff_node->schema->nodetype & (LYS_LEAF | LYS_LEAFLIST)) {
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-default", prev_meta_val ? "true" : "false",
                    0, NULL)) {
                goto ly_error;
            }
        }
        break;
    case EDIT_REPLACE:
        if (diff_node->schema->nodetype & (LYS_LEAF | LYS_ANYXML | LYS_ANYDATA)) {
            assert(meta_val);
            assert(!prev_meta_val || (diff_node->schema->nodetype == LYS_LEAF));

            /* add info about previous value and default state as an attribute */
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-value", meta_val, 0, NULL)) {
                goto ly_error;
            }
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-default", prev_meta_val ? "true" : "false",
                    0, NULL)) {
                goto ly_error;
            }
            break;
        }

        assert(lysc_is_userordered(diff_node->schema));

        /* add info about current place for abort */
        if (lysc_is_dup_inst_list(diff_node->schema)) {
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-position", prev_meta_val, 0, NULL)) {
                goto ly_error;
            }
        } else if (diff_node->schema->nodetype == LYS_LIST) {
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-key", prev_meta_val, 0, NULL)) {
                goto ly_error;
            }
        } else {
            if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-value", prev_meta_val, 0, NULL)) {
                goto ly_error;
            }
        }
    /* fallthrough */
    case EDIT_CREATE:
        if (lysc_is_userordered(diff_node->schema)) {
            /* add info about inserted place as a metadata (meta_val can be NULL, inserted on the first place) */
            if (lysc_is_dup_inst_list(diff_node->schema)) {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:position", meta_val, 0, NULL)) {
                    goto ly_error;
                }
            } else if (diff_node->schema->nodetype == LYS_LIST) {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:key", meta_val, 0, NULL)) {
                    goto ly_error;
                }
            } else {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:value", meta_val, 0, NULL)) {
                    goto ly_error;
                }
            }
        }
        break;
    case EDIT_DELETE:
        if (lysc_is_userordered(diff_node->schema)) {
            /* add info about current place for abort */
            if (lysc_is_dup_inst_list(diff_node->schema)) {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-position", prev_meta_val, 0, NULL)) {
                    goto ly_error;
                }
            } else if (diff_node->schema->nodetype == LYS_LIST) {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-key", prev_meta_val, 0, NULL)) {
                    goto ly_error;
                }
            } else {
                if (lyd_new_meta(LYD_CTX(diff_node), diff_node, NULL, "yang:orig-value", prev_meta_val, 0, NULL)) {
                    goto ly_error;
                }
            }
        }
        break;
    default:
        SR_ERRINFO_INT(&err_info);
        return err_info;
    }

    return NULL;

ly_error:
    sr_errinfo_new_ly(&err_info, LYD_CTX(diff_node));
    return err_info;
}

/**
 * @brief Find a previous (leaf-)list instance.
 *
 * @param[in] llist (Leaf-)list instance.
 * @return Previous instance, NULL if first.
 */
static const struct lyd_node *
sr_edit_find_previous_instance(const struct lyd_node *llist)
{
    if (!llist->prev->next) {
        /* the only/first node */
        return NULL;
    }

    if (llist->prev->schema != llist->schema) {
        /* first instance */
        return NULL;
    }

    return llist->prev;
}

/**
 * @brief Create a predicate for a user-ordered (leaf-)list. For dpulicate-instance list, it is its position.
 * In case of list, it is an array of predicates for each key. For leaf-list, it is simply its value.
 *
 * @param[in] llist (Leaf-)list to process.
 * @return Predicate, NULL on error.
 */
static char *
sr_edit_create_userord_predicate(const struct lyd_node *llist)
{
    char *pred;
    uint32_t pred_len, key_len;
    struct lyd_node *key;

    assert(lysc_is_userordered(llist->schema));

    if (lysc_is_dup_inst_list(llist->schema)) {
        /* duplicate-instance lists use their position */
        if (asprintf(&pred, "%" PRIu32, lyd_list_pos(llist)) == -1) {
            return NULL;
        }
        return pred;
    }

    if (llist->schema->nodetype == LYS_LEAFLIST) {
        /* leaf-list uses the value directly */
        pred = strdup(lyd_get_value(llist));
        return pred;
    }

    /* create list predicate consisting of all the keys */
    pred_len = 0;
    pred = NULL;
    for (key = lyd_child(llist); key && (key->schema->flags & LYS_KEY); key = key->next) {
        key_len = 1 + strlen(key->schema->name) + 2 + strlen(lyd_get_value(key)) + 2;
        pred = sr_realloc(pred, pred_len + key_len + 1);
        if (!pred) {
            return NULL;
        }

        sprintf(pred + pred_len, "[%s='%s']", key->schema->name, lyd_get_value(key));
        pred_len += key_len;
    }

    return pred;
}

/**
 * @brief Transform edit metadata into best-effort diff metadata.
 *
 * @param[in] node Node to transform.
 * @param[in] set_op Set this diff op, if 0 inherit or transform edit op to diff op if there is an explicit operation.
 * @param[in] orig_node Original @p node linked into the full edit.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_meta_edit2diff(struct lyd_node *node, enum edit_op set_op, const struct lyd_node *orig_node)
{
    sr_error_info_t *err_info = NULL;
    const struct lys_module *yang_mod;
    enum edit_op eop;
    struct lyd_meta *meta;
    const struct lyd_node *sibling_before;
    char *sibling_before_val = NULL;
    int op_own;

    yang_mod = ly_ctx_get_module_implemented(LYD_CTX(node), "yang");
    SR_CHECK_INT_GOTO(!yang_mod, err_info, cleanup);

    /* check if there is an operation */
    eop = sr_edit_diff_find_oper(node, 1, &op_own);
    if (op_own) {
        /* delete the current edit operation */
        sr_edit_del_meta_attr(node, "operation");
    }

    if (!set_op) {
        /* transform the operation to diff */
        set_op = sr_op_edit2diff(eop);
    }

    /* remove insert */
    meta = lyd_find_meta(node->meta, yang_mod, "insert");
    lyd_free_meta_single(meta);

    if (lysc_is_userordered(node->schema)) {
        /* find previous instance */
        sibling_before = sr_edit_find_previous_instance(orig_node);
        if (sibling_before) {
            sibling_before_val = sr_edit_create_userord_predicate(sibling_before);
        }
    }

    /* add all diff metadata */
    assert((set_op == EDIT_CREATE) || (set_op == EDIT_DELETE) || (set_op == EDIT_NONE));
    if ((err_info = sr_diff_add_meta(node, sibling_before_val, NULL, set_op))) {
        goto cleanup;
    }

cleanup:
    free(sibling_before_val);
    return err_info;
}

/**
 * @brief Create/find missing parents when appending edit/diff subtree into existing edit/diff tree.
 *
 * @param[in] node Node (subtree) to append.
 * @param[in,out] tree Existing edit/diff tree, is updated.
 * @param[out] top_parent First created parent, NULL if no parents were created.
 * @param[out] node_parent Parent of @p node, may exist or be created.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_diff_create_parents(const struct lyd_node *node, struct lyd_node **tree, struct lyd_node **top_parent,
        struct lyd_node **node_parent)
{
    sr_error_info_t *err_info = NULL;
    char *path_str = NULL;
    LY_ERR lyrc;
    struct lyd_node *tree_parent;

    if (!lyd_parent(node)) {
        /* top-level node, there is no parent */
        *top_parent = NULL;
        *node_parent = NULL;
    } else {
        /* generate parent path */
        path_str = lyd_path(lyd_parent(node), LYD_PATH_STD, NULL, 0);
        SR_CHECK_MEM_GOTO(!path_str, err_info, cleanup);

        /* find first existing parent */
        if (!*tree) {
            tree_parent = NULL;
            lyrc = LY_ENOTFOUND;
        } else {
            lyrc = lyd_find_path(*tree, path_str, 0, &tree_parent);
        }
        if ((lyrc == LY_EINCOMPLETE) || (lyrc == LY_ENOTFOUND)) {
            /* create the missing parents */
            if (lyd_dup_single(lyd_parent(node), (struct lyd_node_inner *)tree_parent,
                    LYD_DUP_NO_META | LYD_DUP_WITH_PARENTS, node_parent)) {
                sr_errinfo_new_ly(&err_info, LYD_CTX(node));
                goto cleanup;
            }

            /* find the first created parent */
            for (*top_parent = *node_parent; lyd_parent(*top_parent) != tree_parent; *top_parent = lyd_parent(*top_parent)) {}

            /* append to tree if no parent existed */
            if (!tree_parent) {
                lyd_insert_sibling(*tree, *top_parent, tree);
            }
        } else if (!lyrc) {
            /* parent already exists */
            *top_parent = NULL;
            *node_parent = tree_parent;
        } else {
            /* error */
            sr_errinfo_new_ly(&err_info, LYD_CTX(*tree));
            goto cleanup;
        }
    }

cleanup:
    free(path_str);
    return err_info;
}

/**
 * @brief Append new diff data based on an edit.
 *
 * @param[in] edit Edit node to transform into diff.
 * @param[in] op Diff operation to set.
 * @param[in] recursive Whether to append @p edit with descendants or not.
 * @param[in,out] diff Diff to append to, do nothing if NULL.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_diff_append(const struct lyd_node *edit, enum edit_op op, int recursive, struct lyd_node **diff)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *diff_parent, *new_diff_parent, *diff_subtree, *elem;

    if (!diff) {
        /* nothing to do, diff is not generated */
        goto cleanup;
    }

    /* find/create edit node parents */
    if ((err_info = sr_edit_diff_create_parents(edit, diff, &new_diff_parent, &diff_parent))) {
        goto cleanup;
    }

    /* set new parent operation, if any */
    if (new_diff_parent && (err_info = sr_diff_set_oper(new_diff_parent, "none"))) {
        goto cleanup;
    }

    /* make a copy of the edit node/subtree */
    if (lyd_dup_single(edit, NULL, recursive ? LYD_DUP_RECURSIVE : 0, &diff_subtree)) {
        sr_errinfo_new_ly(&err_info, LYD_CTX(edit));
        goto cleanup;
    }

    /* switch all edit metadata for best-effort diff metadata */
    LYD_TREE_DFS_BEGIN(diff_subtree, elem) {
        if (!lysc_is_key(elem->schema)) {
            if (elem == diff_subtree) {
                /* set specific operation for the root, use original node from the edit to learn positions */
                err_info = sr_meta_edit2diff(elem, op, edit);
            } else {
                /* inherit and transform op, descendats were fully duplicated so we can use the node normally */
                err_info = sr_meta_edit2diff(elem, 0, elem);
            }
            if (err_info) {
                goto cleanup;
            }
        }

        LYD_TREE_DFS_END(diff_subtree, elem);
    }

    /* finally, insert subtree into diff parent */
    if (diff_parent) {
        lyd_insert_child(diff_parent, diff_subtree);
    } else {
        lyd_insert_sibling(*diff, diff_subtree, diff);
    }

cleanup:
    if (err_info) {
        lyd_free_all(*diff);
        *diff = NULL;
    }
    return err_info;
}

sr_error_info_t *
sr_edit2diff(const struct lyd_node *edit, struct lyd_node **diff)
{
    sr_error_info_t *err_info = NULL;
    const struct lyd_node *root;
    enum edit_op op;

    LY_LIST_FOR(edit, root) {
        op = sr_edit_diff_find_oper(root, 0, NULL);
        assert(op);

        if ((err_info = sr_edit_diff_append(root, sr_op_edit2diff(op), 1, diff))) {
            return err_info;
        }
    }

    return NULL;
}

LY_ERR
sr_lyd_merge_cb(struct lyd_node *trg_node, const struct lyd_node *src_node, void *cb_data)
{
    sr_error_info_t *err_info = NULL;
    struct sr_lyd_merge_cb_data *arg = cb_data;
    char *origin = NULL, *cur_origin = NULL;
    struct lyd_node *next, *child;
    enum edit_op src_op, trg_op;
    int trg_op_own, meta_changed = 0, cid_changed;

    /* update CID of the new node */
    if ((err_info = sr_edit_update_cid(trg_node, arg->cid, src_node ? 1 : 0, &cid_changed))) {
        goto cleanup;
    }
    if (cid_changed) {
        arg->changed = 1;
    }

    /* learn target operation */
    trg_op = sr_edit_diff_find_oper(trg_node, 1, &trg_op_own);
    if ((trg_op != EDIT_MERGE) && (trg_op != EDIT_REMOVE) && (trg_op != EDIT_ETHER)) {
        SR_ERRINFO_INT(&err_info);
        goto cleanup;
    }

    if (!src_node) {
        /* change, append to diff */
        arg->changed = 1;
        if ((err_info = sr_edit_diff_append(trg_node, sr_op_edit2diff(trg_op), 1, arg->diff))) {
            goto cleanup;
        }

        /* origin must be correct, it is either inherited (from some of our parent that was properly merged
         * with its origin) or its is explicitly set (when it was copied into the edit with the nodes) */
        return LY_SUCCESS;
    }

    /* learn source operation */
    src_op = sr_edit_diff_find_oper(src_node, 1, NULL);
    if ((src_op != EDIT_MERGE) && (src_op != EDIT_REMOVE) && (src_op != EDIT_ETHER)) {
        SR_ERRINFO_INT(&err_info);
        goto cleanup;
    }

    /* src_op trg_op */
    /* MERGE MERGE - (copy oper), insert meta src -> trg */
    /* MERGE ETHER - copy oper, insert meta src -> trg */
    /* MERGE REMOVE - copy oper, insert meta src -> trg; free trg children */
    /* REMOVE MERGE - copy oper, (insert meta) src -> trg; free trg children */
    /* REMOVE ETHER - copy oper, (insert meta) src -> trg; free trg children */
    /* ETHER REMOVE - copy oper, (insert meta) src -> trg; (free trg children) */
    /* REMOVE REMOVE - nothing */
    /* ETHER MERGE - nothing */
    /* ETHER ETHER - nothing */

    /* fix operation */
    if (((src_op != EDIT_REMOVE) || (trg_op != EDIT_REMOVE)) && ((src_op != EDIT_ETHER) || (trg_op != EDIT_MERGE)) &&
            ((src_op != EDIT_ETHER) || (trg_op != EDIT_ETHER))) {
        /* copy all metadata */
        if ((err_info = sr_edit_copy_meta(src_node, trg_node, trg_op_own, src_op, &meta_changed))) {
            goto cleanup;
        }

        if ((src_op != EDIT_MERGE) || (trg_op == EDIT_REMOVE)) {
            /* free target children */
            LY_LIST_FOR_SAFE(lyd_child_no_keys(trg_node), next, child) {
                lyd_free_tree(child);
            }
        }
    }

    if ((src_op != trg_op) || meta_changed || lyd_compare_single(src_node, trg_node, LYD_COMPARE_DEFAULTS)) {
        /* change, append to diff */
        arg->changed = 1;
        if ((err_info = sr_edit_diff_append(src_node, sr_op_edit2diff(src_op), 0, arg->diff))) {
            goto cleanup;
        }
    }

    /* fix origin of the new node, keep origin of descendants for now */
    sr_edit_diff_get_origin(trg_node, &cur_origin, NULL);
    sr_edit_diff_get_origin(src_node, &origin, NULL);
    if ((err_info = sr_edit_diff_set_origin(trg_node, origin, 1))) {
        goto cleanup;
    }
    LY_LIST_FOR(lyd_child_no_keys(trg_node), child) {
        if ((err_info = sr_edit_diff_set_origin(child, cur_origin, 0))) {
            goto cleanup;
        }
    }

cleanup:
    free(origin);
    free(cur_origin);
    if (err_info) {
        arg->err_info = err_info;
        return LY_EINT;
    }
    return LY_SUCCESS;
}

LY_ERR
sr_lyd_diff_apply_cb(const struct lyd_node *diff_node, struct lyd_node *data_node, void *user_data)
{
    sr_error_info_t *err_info = NULL;
    char *origin;

    (void)user_data;

    /* copy origin */
    sr_edit_diff_get_origin(diff_node, &origin, NULL);
    err_info = sr_edit_diff_set_origin(data_node, origin, 1);
    free(origin);
    if (err_info) {
        sr_errinfo_free(&err_info);
        return LY_EINT;
    }

    return LY_SUCCESS;
}

/**
 * @brief Check whether a (leaf-)list instance was moved.
 *
 * @param[in] match_node Node instance in the data tree.
 * @param[in] insert Insert place.
 * @param[in] anchor_node Optional relative instance in the data tree.
 * @return 0 if not, non-zero if it was.
 */
static int
sr_edit_userord_is_moved(const struct lyd_node *match_node, enum insert_val insert, const struct lyd_node *anchor_node)
{
    const struct lyd_node *sibling;

    assert(match_node && (((insert != INSERT_BEFORE) && (insert != INSERT_AFTER)) || anchor_node));
    assert(lysc_is_userordered(match_node->schema));

    switch (insert) {
    case INSERT_DEFAULT:
        /* with no insert attribute it can never be moved */
        return 0;

    case INSERT_FIRST:
    case INSERT_AFTER:
        sibling = sr_edit_find_previous_instance(match_node);
        if (sibling == anchor_node) {
            /* match_node is after the anchor node (or is the first) */
            return 0;
        }

        /* node is moved */
        return 1;

    case INSERT_LAST:
    case INSERT_BEFORE:
        if (!match_node->next) {
            /* last node */
            sibling = NULL;
        } else {
            for (sibling = match_node->next; sibling->schema != match_node->schema; sibling = sibling->next) {
                if (!sibling->next) {
                    /* no instance after, it is the last */
                    sibling = NULL;
                    break;
                }
            }
        }
        if (sibling == anchor_node) {
            /* match_node is before the anchor node (or is the last) */
            return 0;
        }

        /* node is moved */
        return 1;
    }

    /* unreachable */
    assert(0);
    return 0;
}

/**
 * @brief Find a matching node in data tree for a specific (leaf-)list instance.
 *
 * @param[in] sibling First data tree sibling.
 * @param[in] llist Arbitrary instance of the (leaf-)list.
 * @param[in] userord_anchor Preceding user-ordered anchor of the searched instance.
 * @param[out] match Matching instance in the data tree.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_find_userord_predicate(const struct lyd_node *sibling, const struct lyd_node *llist, const char *userord_anchor,
        struct lyd_node **match)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *iter;
    uint32_t cur_pos, pos;
    int found = 0;
    LY_ERR lyrc;

    if (lysc_is_dup_inst_list(llist->schema)) {
        pos = atoi(userord_anchor);
        cur_pos = 1;
        LYD_LIST_FOR_INST(sibling, llist->schema, iter) {
            if (cur_pos == pos) {
                found = 1;
                break;
            }
            ++cur_pos;
        }
        if (!found) {
            sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" instance to insert next to not found.",
                    llist->schema->name);
            return err_info;
        }
        *match = iter;
    } else {
        lyrc = lyd_find_sibling_val(sibling, llist->schema, userord_anchor, strlen(userord_anchor), match);
        if (lyrc == LY_ENOTFOUND) {
            sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" instance to insert next to not found.",
                    llist->schema->name);
            return err_info;
        } else if (lyrc) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(llist));
            return err_info;
        }
    }

    return NULL;
}

/**
 * @brief Find a possibly matching node instance in data tree for an edit node.
 *
 * @param[in] first_node First sibling in the data tree.
 * @param[in] edit_node Edit node to match.
 * @param[out] match_p Matching node.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_find_match(const struct lyd_node *first_node, const struct lyd_node *edit_node, struct lyd_node **match_p)
{
    sr_error_info_t *err_info = NULL;
    const struct lysc_node *schema = NULL;
    const struct lys_module *mod = NULL;
    LY_ERR lyrc;

    if (!edit_node->schema) {
        /* opaque node, find target module first */
        mod = lyd_owner_module(edit_node);
        if (mod) {
            /* find target schema node */
            schema = lys_find_child(edit_node->parent ? edit_node->parent->schema : NULL, mod,
                    ((struct lyd_node_opaq *)edit_node)->name.name, 0, 0, 0);
        }
        if (schema) {
            lyrc = lyd_find_sibling_val(first_node, schema, NULL, 0, match_p);
        } else {
            *match_p = NULL;
            lyrc = LY_ENOTFOUND;
        }
    } else if (lysc_is_dup_inst_list(edit_node->schema)) {
        /* never matches */
        *match_p = NULL;
        lyrc = LY_ENOTFOUND;
    } else if (edit_node->schema->nodetype & (LYS_LIST | LYS_LEAFLIST)) {
        /* exact (leaf-)list instance */
        lyrc = lyd_find_sibling_first(first_node, edit_node, match_p);
    } else {
        /* any existing instance */
        lyrc = lyd_find_sibling_val(first_node, edit_node->schema, NULL, 0, match_p);
    }

    /* check for errors */
    if (lyrc && (lyrc != LY_ENOTFOUND)) {
        sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
        return err_info;
    }

    return NULL;
}

/**
 * @brief Find a matching node in data tree for an edit node.
 *
 * @param[in] first_node First sibling in the data tree.
 * @param[in] edit_node Edit node to match.
 * @param[in] op Operation of the edit node.
 * @param[in] insert Optional insert place of the operation.
 * @param[in] userord_anchor Optional user-ordered list anchor of relative (leaf-)list instance of the operation.
 * @param[in] dflt_ll_skip Whether to skip found default leaf-list instance.
 * @param[in] del_val_check Whether to check even values when deleting.
 * @param[out] match_p Matching node.
 * @param[out] val_equal_p Whether even the value matches.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_find(const struct lyd_node *first_node, const struct lyd_node *edit_node, enum edit_op op, enum insert_val insert,
        const char *userord_anchor, int dflt_ll_skip, int del_val_check, struct lyd_node **match_p, int *val_equal_p)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *anchor_node;
    const struct lyd_node *match = NULL;
    int val_equal = 0;
    LY_ERR lyrc;

    if ((op == EDIT_PURGE) && edit_node->schema && (edit_node->schema->nodetype & (LYS_LIST | LYS_LEAFLIST))) {
        /* find first instance */
        lyrc = lyd_find_sibling_val(first_node, edit_node->schema, NULL, 0, (struct lyd_node **)&match);
        if (lyrc && (lyrc != LY_ENOTFOUND)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
            return err_info;
        }
        if (match) {
            val_equal = 1;
        }
    } else {
        /* find the edit node instance efficiently in data (if possible) */
        if ((err_info = sr_edit_find_match(first_node, edit_node, (struct lyd_node **)&match))) {
            return err_info;
        }

        if (match) {
            switch (match->schema->nodetype) {
            case LYS_CONTAINER:
                val_equal = 1;
                break;
            case LYS_LEAF:
            case LYS_ANYXML:
            case LYS_ANYDATA:
                if ((!del_val_check && ((op == EDIT_REMOVE) || (op == EDIT_DELETE))) || (op == EDIT_PURGE)) {
                    /* we do not care about the value in this case */
                    val_equal = 1;
                } else if (lyd_compare_single(match, edit_node, 0) == LY_ENOT) {
                    /* check whether the value is different (dflt flag may or may not differ) */
                    val_equal = 0;
                } else {
                    /* canonical values are the same */
                    val_equal = 1;
                }
                break;
            case LYS_LIST:
            case LYS_LEAFLIST:
                if (dflt_ll_skip && (match->flags & LYD_DEFAULT)) {
                    /* default leaf-list is not really considered to exist in data */
                    assert(match->schema->nodetype == LYS_LEAFLIST);
                    match = NULL;
                } else if (lysc_is_userordered(match->schema)) {
                    /* check if even the order matches for user-ordered (leaf-)lists */
                    anchor_node = NULL;
                    if (userord_anchor) {
                        /* find the anchor node if set */
                        if ((err_info = sr_edit_find_userord_predicate(first_node, match, userord_anchor, &anchor_node))) {
                            return err_info;
                        }
                    }
                    /* check for move */
                    if (sr_edit_userord_is_moved(match, insert, anchor_node)) {
                        val_equal = 0;
                    } else {
                        val_equal = 1;
                    }
                } else {
                    val_equal = 1;
                }
                break;
            default:
                SR_ERRINFO_INT(&err_info);
                return err_info;
            }
        }
    }

    *match_p = (struct lyd_node *)match;
    if (val_equal_p) {
        *val_equal_p = val_equal;
    }
    return NULL;
}

const char *
sr_edit_op2str(enum edit_op op)
{
    switch (op) {
    case EDIT_ETHER:
        return "ether";
    case EDIT_PURGE:
        return "purge";
    case EDIT_NONE:
        return "none";
    case EDIT_MERGE:
        return "merge";
    case EDIT_REPLACE:
        return "replace";
    case EDIT_CREATE:
        return "create";
    case EDIT_DELETE:
        return "delete";
    case EDIT_REMOVE:
        return "remove";
    default:
        break;
    }

    assert(0);
    return NULL;
}

/**
 * @brief Insert an edit node into a data tree.
 *
 * @param[in,out] first_node First sibling of the data tree.
 * @param[in] parent_node Data tree sibling parent node.
 * @param[in] new_node Edit node to insert.
 * @param[in] insert Place where to insert the node.
 * @param[in] userord_anchor Optional user-ordered anchor of relative (leaf-)list instance.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_insert(struct lyd_node **first_node, struct lyd_node *parent_node, struct lyd_node *new_node,
        enum insert_val insert, const char *userord_anchor)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *anchor;

    assert(new_node);

    if (!*first_node) {
        if (!parent_node) {
            /* no parent or siblings */
            *first_node = new_node;
            return NULL;
        }

        /* simply insert into parent, no other children */
        if (userord_anchor) {
            sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" instance to insert next to not found.",
                    new_node->schema->name);
            return err_info;
        }
        if (lyd_insert_child(parent_node, new_node)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(parent_node));
            return err_info;
        }
        return NULL;
    }

    assert(!(*first_node)->parent || ((*first_node)->parent == (struct lyd_node_inner *)parent_node));

    /* unlink properly first to avoid unwanted behavior (first node equals new_node or new_node is the first sibling) */
    if (new_node == *first_node) {
        *first_node = (*first_node)->next;
    }
    lyd_unlink_tree(new_node);

    /* insert last or first */
    if ((insert == INSERT_DEFAULT) || (insert == INSERT_LAST)) {
        /* default insert is at the last position */
        lyd_insert_sibling(*first_node, new_node, parent_node ? NULL : first_node);
        return NULL;
    } else if (insert == INSERT_FIRST) {
        /* find first instance */
        lyd_find_sibling_val(*first_node, new_node->schema, NULL, 0, &anchor);
        if (anchor) {
            /* insert before the first instance */
            lyd_insert_before(anchor, new_node);
            if (anchor == *first_node) {
                assert((*first_node)->prev == new_node);
                *first_node = new_node;
            }
        } else {
            /* insert anywhere, there are no instances */
            lyd_insert_sibling(*first_node, new_node, parent_node ? NULL : first_node);
        }
        return NULL;
    }

    assert(lysc_is_userordered(new_node->schema) && userord_anchor);

    /* find the anchor sibling */
    if ((err_info = sr_edit_find_userord_predicate(*first_node, new_node, userord_anchor, &anchor))) {
        return err_info;
    }

    /* insert before or after */
    if (insert == INSERT_BEFORE) {
        lyd_insert_before(anchor, new_node);
        assert(anchor->prev == new_node);
        if (*first_node == anchor) {
            *first_node = new_node;
        }
    } else if (insert == INSERT_AFTER) {
        lyd_insert_after(anchor, new_node);
        assert(new_node->prev == anchor);
        if (*first_node == new_node) {
            *first_node = anchor;
        }
    }

    return NULL;
}

sr_error_info_t *
sr_diff_set_oper(struct lyd_node *diff, const char *op)
{
    sr_error_info_t *err_info = NULL;

    if (diff->schema) {
        if (lyd_new_meta(LYD_CTX(diff), diff, NULL, "yang:operation", op, 0, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(diff));
            return err_info;
        }
    } else {
        if (lyd_new_attr2(diff, "urn:ietf:params:xml:ns:yang:1", "operation", op, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(diff));
            return err_info;
        }
    }

    return NULL;
}

enum edit_op
sr_edit_diff_find_oper(const struct lyd_node *edit, int recursive, int *own_oper)
{
    struct lyd_meta *meta;
    struct lyd_attr *attr;
    enum edit_op op;

    if (!edit) {
        return 0;
    }

    if (own_oper) {
        *own_oper = 1;
    }
    do {
        if (edit->schema) {
            LY_LIST_FOR(edit->meta, meta) {
                if (!strcmp(meta->name, "operation")) {
                    if (!strcmp(meta->annotation->module->name, "sysrepo") ||
                            !strcmp(meta->annotation->module->name, "ietf-netconf") ||
                            !strcmp(meta->annotation->module->name, "yang")) {
                        return sr_edit_str2op(lyd_get_meta_value(meta));
                    }
                }
            }
        } else {
            op = 0;
            LY_LIST_FOR(((struct lyd_node_opaq *)edit)->attr, attr) {
                if (!strcmp(attr->name.name, "operation")) {
                    /* try to create a metadata instance and use that */
                    uint32_t prev_lo = ly_log_options(0);
                    if (!lyd_new_meta2(LYD_CTX(edit), NULL, 0, attr, &meta)) {
                        if (!strcmp(meta->annotation->module->name, "sysrepo") ||
                                !strcmp(meta->annotation->module->name, "ietf-netconf")) {
                            op = sr_edit_str2op(lyd_get_meta_value(meta));
                        }
                        lyd_free_meta_single(meta);
                    }
                    ly_log_options(prev_lo);

                    if (op) {
                        return op;
                    }
                }
            }
        }

        if (!recursive) {
            return 0;
        }

        edit = lyd_parent(edit);
        if (own_oper) {
            *own_oper = 0;
        }
    } while (edit);

    return 0;
}

void
sr_edit_del_meta_attr(struct lyd_node *edit, const char *name)
{
    struct lyd_meta *meta;
    struct lyd_attr *attr;

    if (edit->schema) {
        LY_LIST_FOR(edit->meta, meta) {
            if (!strcmp(meta->name, name)) {
                if (!strcmp(meta->annotation->module->name, "sysrepo") ||
                        !strcmp(meta->annotation->module->name, "ietf-netconf") ||
                        !strcmp(meta->annotation->module->name, "yang") ||
                        !strcmp(meta->annotation->module->name, "ietf-origin")) {
                    lyd_free_meta_single(meta);
                    return;
                }
            }
        }
    } else {
        LY_LIST_FOR(((struct lyd_node_opaq *)edit)->attr, attr) {
            if (!strcmp(attr->name.name, name)) {
                switch (attr->format) {
                case LY_VALUE_JSON:
                    if (!strcmp(attr->name.module_name, "sysrepo") ||
                            !strcmp(attr->name.module_name, "ietf-netconf") ||
                            !strcmp(attr->name.module_name, "yang") ||
                            !strcmp(attr->name.module_name, "ietf-origin")) {
                        lyd_free_attr_single(LYD_CTX(edit), attr);
                        return;
                    }
                    break;
                case LY_VALUE_XML:
                    if (!strcmp(attr->name.module_ns, "http://www.sysrepo.org/yang/sysrepo") ||
                            !strcmp(attr->name.module_ns, "urn:ietf:params:xml:ns:netconf:base:1.0") ||
                            !strcmp(attr->name.module_ns, "urn:ietf:params:xml:ns:yang:1") ||
                            !strcmp(attr->name.module_ns, "urn:ietf:params:xml:ns:yang:ietf-origin")) {
                        lyd_free_attr_single(LYD_CTX(edit), attr);
                        return;
                    }
                    break;
                default:
                    assert(0);
                    return;
                }
            }
        }
    }

    assert(0);
}

void
sr_edit_diff_get_origin(const struct lyd_node *node, char **origin, int *origin_own)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_meta *meta = NULL, *attr_meta = NULL;
    struct lyd_attr *a;
    const struct lyd_node *parent;
    LY_ERR lyrc;

    *origin = NULL;
    if (origin_own) {
        *origin_own = 0;
    }

    for (parent = node; parent; parent = lyd_parent(parent)) {
        if (parent->schema) {
            meta = lyd_find_meta(parent->meta, NULL, "ietf-origin:origin");
            if (meta) {
                break;
            }
        } else {
            LY_LIST_FOR(((struct lyd_node_opaq *)parent)->attr, a) {
                /* try to parse into metadata */
                if (!strcmp(a->name.name, "origin")) {
                    lyrc = lyd_new_meta2(LYD_CTX(node), NULL, 0, a, &attr_meta);
                    if (lyrc && (lyrc != LY_ENOT)) {
                        sr_errinfo_new_ly(&err_info, LYD_CTX(node));
                        sr_errinfo_free(&err_info);
                        return;
                    }
                    if (!lyrc) {
                        if (!strcmp(attr_meta->annotation->module->name, "ietf-origin")) {
                            meta = attr_meta;
                            break;
                        } else {
                            lyd_free_meta_single(attr_meta);
                            attr_meta = NULL;
                        }
                    }
                }
            }
        }
    }

    if (meta) {
        *origin = strdup(lyd_get_meta_value(meta));
        if (origin_own && (parent == node)) {
            *origin_own = 1;
        }
    }
    lyd_free_meta_single(attr_meta);
}

sr_error_info_t *
sr_edit_diff_set_origin(struct lyd_node *node, const char *origin, int overwrite)
{
    sr_error_info_t *err_info = NULL;
    char *cur_origin;
    int cur_origin_own;

    if (!origin) {
        origin = SR_OPER_ORIGIN;
    }

    sr_edit_diff_get_origin(node, &cur_origin, &cur_origin_own);

    if (cur_origin && (!strcmp(origin, cur_origin) || (!overwrite && cur_origin_own))) {
        /* already set */
        free(cur_origin);
        return NULL;
    }
    free(cur_origin);

    /* our origin is wrong, remove it */
    if (cur_origin_own) {
        sr_edit_del_meta_attr(node, "origin");
    }

    /* set correct origin */
    if (node->schema) {
        if (lyd_new_meta(LYD_CTX(node), node, NULL, "ietf-origin:origin", origin, 0, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(node));
            return err_info;
        }
    } else {
        if (lyd_new_attr(node, NULL, "ietf-origin:origin", origin, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(node));
            return err_info;
        }
    }

    return NULL;
}

/**
 * @brief Add a node from data tree/edit into sysrepo diff.
 *
 * @param[in,out] node Changed node, may be freed.
 * @param[in] meta_val Metadata value (meaning depends on the nodetype).
 * @param[in] prev_meta_value Previous metadata value (meaning depends on the nodetype).
 * @param[in] op Diff operation.
 * @param[in] no_dup Do not duplicate the tree (handy when deleting subtree while having datastore).
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Current sysrepo diff root node.
 * @param[out] diff_node Optional created diff node.
 * @return err_info, NULL on error.
 */
static sr_error_info_t *
sr_edit_diff_add(struct lyd_node **node, const char *meta_val, const char *prev_meta_val, enum edit_op op, int no_dup,
        struct lyd_node *diff_parent, struct lyd_node **diff_root, struct lyd_node **diff_node)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *node_dup = NULL, *elem;
    const struct lyd_node *sibling_before;
    char *sibling_before_val = NULL;

    assert((op == EDIT_NONE) || (op == EDIT_CREATE) || (op == EDIT_DELETE) || (op == EDIT_REPLACE));
    assert(!diff_node || !*diff_node);

    if (!diff_parent && !diff_root) {
        /* we are actually not generating a diff, so just perform what we are supposed to to change the datastore */
        if (no_dup) {
            lyd_free_tree(*node);
            *node = NULL;
        }
        return NULL;
    }

    if (no_dup) {
        assert(op == EDIT_DELETE);

        /* unlink subtree */
        lyd_unlink_tree(*node);
        node_dup = *node;

        /* add attributes for all nodes in the subtree */
        LYD_TREE_DFS_BEGIN(node_dup, elem) {
            if (elem == node_dup) {
                /* meta values are relevant for this node only */
                if ((err_info = sr_diff_add_meta(elem, meta_val, prev_meta_val, op))) {
                    goto error;
                }
            } else if (lysc_is_userordered(elem->schema)) {
                /* only add information about previous instance for userord lists, nothing else is needed */
                sibling_before = sr_edit_find_previous_instance(elem);
                if (sibling_before) {
                    sibling_before_val = sr_edit_create_userord_predicate(sibling_before);
                }

                /* add metadata */
                if ((err_info = sr_diff_add_meta(elem, NULL, sibling_before_val, op))) {
                    goto error;
                }
                free(sibling_before_val);
                sibling_before_val = NULL;
            }

            LYD_TREE_DFS_END(node_dup, elem);
        }
    } else {
        /* duplicate node */
        if (lyd_dup_single(*node, NULL, LYD_DUP_NO_META, &node_dup)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(*node));
            goto error;
        }

        /* add specific attributes for the node */
        if ((err_info = sr_diff_add_meta(node_dup, meta_val, prev_meta_val, op))) {
            goto error;
        }
    }

    if ((node_dup->schema->nodetype == LYS_LEAFLIST) && ((struct lysc_node_leaflist *)node_dup->schema)->dflts &&
            (op == EDIT_CREATE)) {
        /* default leaf-list with the same value may have been removed, so we need to merge these 2 diffs */
        if (lyd_diff_merge_tree(diff_root, diff_parent, node_dup, NULL, NULL, 0)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(*node));
            goto error;
        }

        /* it was duplicated, so free it and do not return any diff node (since it has no children, it is okay) */
        lyd_free_tree(node_dup);
        node_dup = NULL;
    } else {
        /* insert node into diff */
        if (diff_parent) {
            lyd_insert_child(diff_parent, node_dup);
        } else {
            lyd_insert_sibling(*diff_root, node_dup, diff_root);
        }
    }

    if (diff_node) {
        *diff_node = node_dup;
    }
    return NULL;

error:
    if (!no_dup) {
        lyd_free_tree(node_dup);
    }
    free(sibling_before_val);
    return err_info;
}

/**
 * @brief Set container default flag for all empty containers as a result of delete operation.
 *
 * @param[in] parent First possibly affected parent.
 */
static void
sr_edit_delete_set_cont_dflt(struct lyd_node *parent)
{
    struct lyd_node *iter;

    if (!parent || (parent->schema->nodetype != LYS_CONTAINER)) {
        return;
    }

    LY_LIST_FOR(lyd_child(parent), iter) {
        if (!(iter->flags & LYD_DEFAULT)) {
            return;
        }
    }

    if (parent->schema->flags & LYS_PRESENCE) {
        parent->flags |= LYD_DEFAULT;
    }
}

#define EDIT_APPLY_REPLACE_R 0x01       /**< There was a replace operation in a parent, change behavior accordingly. */
#define EDIT_APPLY_CHECK_OP_R 0x02      /**< Do not apply edit, just check whether the operations are valid. */

/**
 * @brief Check whether this diff node is redundant (does not change data).
 *
 * @param[in] diff Diff node.
 * @return 0 if not, non-zero if it is.
 */
static int
sr_diff_is_redundant(struct lyd_node *diff)
{
    sr_error_info_t *err_info = NULL;
    enum edit_op op;
    struct lyd_meta *orig_meta = NULL, *meta = NULL;
    struct lyd_node *child;
    const struct lys_module *yang_mod;

    assert(diff);

    child = lyd_child_no_keys(diff);
    yang_mod = ly_ctx_get_module_latest(LYD_CTX(diff), "yang");
    assert(yang_mod);

    /* get node operation */
    op = sr_edit_diff_find_oper(diff, 1, NULL);

    if ((op == EDIT_REPLACE) && lysc_is_userordered(diff->schema)) {
        /* check for redundant move */
        if (lysc_is_dup_inst_list(diff->schema)) {
            orig_meta = lyd_find_meta(diff->meta, yang_mod, "orig-position");
            meta = lyd_find_meta(diff->meta, yang_mod, "position");
        } else if (diff->schema->nodetype == LYS_LIST) {
            orig_meta = lyd_find_meta(diff->meta, yang_mod, "orig-key");
            meta = lyd_find_meta(diff->meta, yang_mod, "key");
        } else {
            orig_meta = lyd_find_meta(diff->meta, yang_mod, "orig-value");
            meta = lyd_find_meta(diff->meta, yang_mod, "value");
        }
        assert(orig_meta && meta);
        /* in the dictionary */
        if (!lyd_compare_meta(orig_meta, meta)) {
            /* there is actually no move */
            lyd_free_meta_single(orig_meta);
            lyd_free_meta_single(meta);
            if (child) {
                /* change operation to NONE, we have siblings */
                sr_edit_del_meta_attr(diff, "operation");
                if ((err_info = sr_diff_set_oper(diff, "none"))) {
                    /* it was printed at least */
                    sr_errinfo_free(&err_info);
                }
                return 0;
            }

            /* redundant node, BUT !!
             * In diff the move operation is always converted to be INSERT_AFTER, which is fine
             * because the data that this is applied on do not change for the diff lifetime.
             * However, when we are merging 2 diffs, this conversion is actually lossy because
             * if the data change, the move operation can also change its meaning. In this specific
             * case the move operation will be lost. But it can be considered a feature, it is not supported.
             */
            return 1;
        }
    } else if ((op == EDIT_NONE) && (diff->schema->nodetype & (LYS_LEAF | LYS_LEAFLIST))) {
        meta = lyd_find_meta(diff->meta, yang_mod, "orig-default");
        assert(meta);

        /* if previous and current dflt flags are the same, this node is redundant */
        if ((meta->value.boolean && (diff->flags & LYD_DEFAULT)) || (!meta->value.boolean && !(diff->flags & LYD_DEFAULT))) {
            return 1;
        }
        return 0;
    }

    if (!child && (op == EDIT_NONE)) {
        return 1;
    }

    return 0;
}

/**
 * @brief Apply edit ether operation.
 *
 * @param[in] match_node Matching data tree node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[in,out] flags_r Modified flags for the rest of recursive applpying of this operation.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_ether(struct lyd_node *match_node, enum edit_op *next_op, int *flags_r)
{
    if (!match_node) {
        *flags_r |= EDIT_APPLY_CHECK_OP_R;
        *next_op = EDIT_CONTINUE;
    } else {
        *next_op = EDIT_NONE;
    }

    return NULL;
}

/**
 * @brief Apply edit none operation.
 *
 * @param[in] match_node Matching data tree node.
 * @param[in] edit_node Current edit node.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[out] diff_node Created diff node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[out] change Whether some data change occurred.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_none(struct lyd_node *match_node, const struct lyd_node *edit_node, struct lyd_node *diff_parent,
        struct lyd_node **diff_root, struct lyd_node **diff_node, enum edit_op *next_op, int *change)
{
    sr_error_info_t *err_info = NULL;
    uintptr_t prev_dflt;

    assert(edit_node || match_node);

    if (!match_node) {
        sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" does not exist.", LYD_NAME(edit_node));
        return err_info;
    }

    if (match_node->schema->nodetype & (LYS_LIST | LYS_CONTAINER)) {
        /* update diff, we may need this node */
        if ((err_info = sr_edit_diff_add(&match_node, NULL, NULL, EDIT_NONE, 0, diff_parent, diff_root, diff_node))) {
            return err_info;
        }
    } else if ((match_node->schema->nodetype & (LYS_LEAF | LYS_LEAFLIST)) &&
            ((match_node->flags & LYD_DEFAULT) != (edit_node->flags & LYD_DEFAULT))) {
        prev_dflt = match_node->flags & LYD_DEFAULT;

        /* update dflt flag itself */
        match_node->flags &= ~LYD_DEFAULT;
        match_node->flags |= edit_node->flags & LYD_DEFAULT;

        /* default flag changed, we need the node in the diff */
        if ((err_info = sr_edit_diff_add(&match_node, NULL, (char *)prev_dflt, EDIT_NONE, 0, diff_parent, diff_root,
                diff_node))) {
            return err_info;
        }

        if (change) {
            *change = 1;
        }
    }

    *next_op = EDIT_CONTINUE;
    return NULL;
}

/**
 * @brief Apply edit remove operation.
 *
 * @param[in,out] first_node First sibling of the data tree.
 * @param[in] parent_node Parent of the first sibling.
 * @param[in,out] match_node Matching data tree node, may be updated for auto-remove.
 * @param[in] val_equal Whether even values of the nodes match.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[out] diff_node Created diff node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[in,out] flags_r Modified flags for the rest of recursive applpying of this operation.
 * @param[out] change Whether some data change occurred.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_remove(struct lyd_node **first_node, struct lyd_node *parent_node, struct lyd_node **match_node,
        int val_equal, struct lyd_node *diff_parent, struct lyd_node **diff_root, struct lyd_node **diff_node,
        enum edit_op *next_op, int *flags_r, int *change)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *parent;
    const struct lyd_node *sibling_before;
    char *sibling_before_val = NULL;

    /* just use the value because it is only in an assert */
    (void)parent_node;

    if (*match_node && val_equal) {
        if (lysc_is_userordered((*match_node)->schema)) {
            /* get original (current) previous instance to be stored in diff */
            sibling_before = sr_edit_find_previous_instance(*match_node);
            if (sibling_before) {
                sibling_before_val = sr_edit_create_userord_predicate(sibling_before);
            }
        }

        if ((*match_node == *first_node) && !(*match_node)->parent) {
            assert(!parent_node);

            /* we will unlink a top-level node */
            *first_node = (*first_node)->next;
        }
        parent = lyd_parent(*match_node);

        /* update diff, remove the whole subtree by relinking it to the diff */
        if ((err_info = sr_edit_diff_add(match_node, NULL, sibling_before_val, EDIT_DELETE, 1, diff_parent, diff_root,
                diff_node))) {
            goto cleanup;
        }

        /* set empty non-presence container dflt flag */
        sr_edit_delete_set_cont_dflt(parent);

        if (*flags_r & EDIT_APPLY_REPLACE_R) {
            /* we are definitely finished with this subtree now and there is no edit to continue with */
            *next_op = EDIT_FINISH;
        } else {
            /* continue normally with the edit */
            *next_op = EDIT_CONTINUE;
        }

        if (change) {
            *change = 1;
        }
    } else {
        /* match is not valid, value does not match */
        *match_node = NULL;

        /* there is nothing to remove, just check operations in the rest of this edit subtree */
        *flags_r |= EDIT_APPLY_CHECK_OP_R;
        *next_op = EDIT_CONTINUE;
    }

cleanup:
    free(sibling_before_val);
    return err_info;
}

/**
 * @brief Apply edit move operation.
 *
 * @param[in,out] first_node First sibling of the data tree.
 * @param[in] parent_node Parent of the first sibling.
 * @param[in] edit_node Current edit node.
 * @param[in] match_node Matching data tree node, may be created.
 * @param[in] insert Insert attribute value.
 * @param[in] key_or_value Optional relative list instance keys predicate or leaf-list value.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[out] diff_node Created diff node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[out] change Whether some data change occurred.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_move(struct lyd_node **first_node, struct lyd_node *parent_node, const struct lyd_node *edit_node,
        struct lyd_node **match_node, enum insert_val insert, const char *key_or_value, struct lyd_node *diff_parent,
        struct lyd_node **diff_root, struct lyd_node **diff_node, enum edit_op *next_op, int *change)
{
    sr_error_info_t *err_info = NULL;
    const struct lyd_node *old_sibling_before, *sibling_before;
    char *old_sibling_before_val = NULL, *sibling_before_val = NULL;
    enum edit_op diff_op;

    assert(lysc_is_userordered(edit_node->schema));

    if (!*match_node) {
        /* new instance */
        if (lyd_dup_single(edit_node, NULL, LYD_DUP_NO_META, match_node)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
            return err_info;
        }
        diff_op = EDIT_CREATE;
    } else {
        /* in the data tree, being replaced */
        diff_op = EDIT_REPLACE;
    }

    /* get current previous sibling instance */
    old_sibling_before = sr_edit_find_previous_instance(*match_node);

    /* move the node */
    if ((err_info = sr_edit_insert(first_node, parent_node, *match_node, insert, key_or_value))) {
        return err_info;
    }

    /* get previous instance after move */
    sibling_before = sr_edit_find_previous_instance(*match_node);

    /* update diff with correct move information */
    if (old_sibling_before) {
        old_sibling_before_val = sr_edit_create_userord_predicate(old_sibling_before);
    }
    if (sibling_before) {
        sibling_before_val = sr_edit_create_userord_predicate(sibling_before);
    }
    err_info = sr_edit_diff_add(match_node, sibling_before_val, old_sibling_before_val, diff_op, 0, diff_parent,
            diff_root, diff_node);

    free(old_sibling_before_val);
    free(sibling_before_val);
    if (err_info) {
        return err_info;
    }

    *next_op = EDIT_CONTINUE;
    if (change) {
        *change = 1;
    }
    return NULL;
}

sr_error_info_t *
sr_edit_created_subtree_apply_move(struct lyd_node *match_subtree)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *elem;
    const struct lyd_node *sibling_before;
    char *sibling_before_val;

    LYD_TREE_DFS_BEGIN(match_subtree, elem) {
        if (lysc_is_userordered(elem->schema)) {
            sibling_before_val = NULL;
            sibling_before = sr_edit_find_previous_instance(elem);
            if (sibling_before) {
                sibling_before_val = sr_edit_create_userord_predicate(sibling_before);
            }

            if (elem->schema->nodetype == LYS_LIST) {
                if (lyd_new_meta(LYD_CTX(elem), elem, NULL, "yang:key", sibling_before_val, 0, NULL)) {
                    sr_errinfo_new_ly(&err_info, LYD_CTX(elem));
                }
            } else {
                if (lyd_new_meta(LYD_CTX(elem), elem, NULL, "yang:value", sibling_before_val, 0, NULL)) {
                    sr_errinfo_new_ly(&err_info, LYD_CTX(elem));
                }
            }
            free(sibling_before_val);
            if (err_info) {
                break;
            }
        }

        LYD_TREE_DFS_END(match_subtree, elem);
    }

    return err_info;
}

/**
 * @brief Apply edit replace operation.
 *
 * @param[in] match_node Matching data tree node.
 * @param[in] val_equal Whether even values of the nodes match.
 * @param[in] edit_node Current edit node.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[out] diff_node Created diff node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[in,out] flags_r Modified flags for the rest of recursive applpying of this operation.
 * @param[out] change Whether some data change occured.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_replace(struct lyd_node *match_node, int val_equal, const struct lyd_node *edit_node, struct lyd_node *diff_parent,
        struct lyd_node **diff_root, struct lyd_node **diff_node, enum edit_op *next_op, int *flags_r, int *change)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node_any *any;
    LY_ERR lyrc;
    char *prev_val;
    uintptr_t prev_dflt;

    if (!match_node) {
        *next_op = EDIT_CREATE;
        return NULL;
    }

    if (val_equal) {
        *next_op = EDIT_NONE;
    } else {
        switch (match_node->schema->nodetype) {
        case LYS_LIST:
        case LYS_LEAFLIST:
            *next_op = EDIT_MOVE;
            break;
        case LYS_LEAF:
            /* remember previous value */
            prev_val = strdup(lyd_get_value(match_node));
            SR_CHECK_MEM_RET(!prev_val, err_info);
            prev_dflt = match_node->flags & LYD_DEFAULT;

            /* modify the node */
            lyrc = lyd_change_term(match_node, lyd_get_value(edit_node));
            if (lyrc && (lyrc != LY_EEXIST)) {
                free(prev_val);
                sr_errinfo_new_ly(&err_info, LYD_CTX(match_node));
                return err_info;
            }

            /* add the updated node into diff */
            err_info = sr_edit_diff_add(&match_node, prev_val, (char *)prev_dflt, EDIT_REPLACE, 0, diff_parent,
                    diff_root, diff_node);
            free(prev_val);
            if (err_info) {
                return err_info;
            }

            *next_op = EDIT_CONTINUE;
            if (change) {
                *change = 1;
            }
            break;
        case LYS_ANYXML:
        case LYS_ANYDATA:
            /* remember previous value */
            if (lyd_any_value_str(match_node, &prev_val)) {
                sr_errinfo_new_ly(&err_info, LYD_CTX(match_node));
                return err_info;
            }

            /* modify the node */
            any = (struct lyd_node_any *)edit_node;
            if (lyd_any_copy_value(match_node, &any->value, any->value_type)) {
                free(prev_val);
                sr_errinfo_new_ly(&err_info, LYD_CTX(match_node));
                return err_info;
            }

            /* add the updated node into diff */
            err_info = sr_edit_diff_add(&match_node, prev_val, NULL, EDIT_REPLACE, 0, diff_parent, diff_root, diff_node);
            free(prev_val);
            if (err_info) {
                return err_info;
            }

            *next_op = EDIT_CONTINUE;
            if (change) {
                *change = 1;
            }
            break;
        default:
            SR_ERRINFO_INT(&err_info);
            return err_info;
        }
    }

    /* remove all children that are in the datastore and not in the edit (the rest will be handled in a standard way) */
    *flags_r |= EDIT_APPLY_REPLACE_R;
    return NULL;
}

/**
 * @brief Apply edit create operation.
 *
 * @param[in,out] first_node First sibling of the data tree.
 * @param[in] parent_node Parent of the first sibling.
 * @param[in,out] match_node Matching data tree node, may be created.
 * @param[in] val_equal Whether even values of the nodes match.
 * @param[in] edit_node Current edit node.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[out] diff_node Created diff node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @param[out] change Whether some data change occured.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_create(struct lyd_node **first_node, struct lyd_node *parent_node, struct lyd_node **match_node,
        int val_equal, const struct lyd_node *edit_node, struct lyd_node *diff_parent, struct lyd_node **diff_root,
        struct lyd_node **diff_node, enum edit_op *next_op, int *change)
{
    sr_error_info_t *err_info = NULL;

    if (!edit_node->schema) {
        sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED, "Opaque (invalid) node \"%s\" cannot be created.", LYD_NAME(edit_node));
        return err_info;
    }

    if (*match_node) {
        if (lysc_is_np_cont(edit_node->schema)) {
            /* ignore creating NP containers */
            *next_op = EDIT_NONE;
            return NULL;
        }

        if ((edit_node->schema->nodetype == LYS_LEAF) && ((*match_node)->flags & LYD_DEFAULT)) {
            /* allow creating existing default leaves */
            if (val_equal) {
                *next_op = EDIT_NONE;
            } else {
                *next_op = EDIT_REPLACE;
            }
            return NULL;
        }

        sr_errinfo_new(&err_info, SR_ERR_EXISTS, "Node \"%s\" to be created already exists.",
                edit_node->schema->name);
        return err_info;
    }

    if (lysc_is_userordered(edit_node->schema)) {
        /* handle creating user-ordered lists separately */
        *next_op = EDIT_MOVE;
        return NULL;
    }

    /* create and insert the node at the correct place */
    if (lyd_dup_single(edit_node, NULL, LYD_DUP_NO_META, match_node)) {
        sr_errinfo_new_ly(&err_info, LYD_CTX(edit_node));
        return err_info;
    }

    if ((err_info = sr_edit_insert(first_node, parent_node, *match_node, 0, NULL))) {
        return err_info;
    }

    if ((err_info = sr_edit_diff_add(match_node, NULL, NULL, EDIT_CREATE, 0, diff_parent, diff_root, diff_node))) {
        return err_info;
    }

    *next_op = EDIT_CONTINUE;
    if (change) {
        *change = 1;
    }
    return NULL;
}

/**
 * @brief Apply edit merge operation.
 *
 * @param[in] match_node Matching data tree node.
 * @param[in] val_equal Whether even values of the nodes match.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_merge(struct lyd_node *match_node, int val_equal, enum edit_op *next_op)
{
    sr_error_info_t *err_info = NULL;

    if (!match_node) {
        *next_op = EDIT_CREATE;
    } else if (!val_equal) {
        switch (match_node->schema->nodetype) {
        case LYS_LIST:
        case LYS_LEAFLIST:
            assert(lysc_is_userordered(match_node->schema));
            *next_op = EDIT_MOVE;
            break;
        case LYS_LEAF:
        case LYS_ANYXML:
        case LYS_ANYDATA:
            *next_op = EDIT_REPLACE;
            break;
        default:
            SR_ERRINFO_INT(&err_info);
            return err_info;
        }
    } else {
        *next_op = EDIT_NONE;
    }

    return NULL;
}

/**
 * @brief Apply edit delete operation.
 *
 * @param[in] match_node Matching data tree node.
 * @param[in] val_equal Whether even values of the nodes match.
 * @param[in] edit_node Current edit node.
 * @param[out] next_op Next operation to be performed with these nodes.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_delete(struct lyd_node *match_node, int val_equal, const struct lyd_node *edit_node, enum edit_op *next_op)
{
    sr_error_info_t *err_info = NULL;

    if (!match_node) {
        sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" to be deleted does not exist.", LYD_NAME(edit_node));
        return err_info;
    } else if (!val_equal) {
        sr_errinfo_new(&err_info, SR_ERR_NOT_FOUND, "Node \"%s\" with the specific value to be deleted does not exist.",
                LYD_NAME(edit_node));
        return err_info;
    }

    *next_op = EDIT_REMOVE;
    return NULL;
}

/**
 * @brief Apply sysrepo edit subtree on data tree nodes, recursively. Optionally,
 * sysrepo diff is being also created/updated.
 *
 * @param[in,out] first_node First sibling of the data tree. If not set, data tree is not modified.
 * @param[in] parent_node Parent of the first sibling.
 * @param[in] edit_node Sysrepo edit node.
 * @param[in] parent_op Parent operation.
 * @param[in] diff_parent Current sysrepo diff parent.
 * @param[in,out] diff_root Sysrepo diff root node.
 * @param[in] oper_edit Whether we are applying stored operational edit.
 * @param[in] flags Flags modifying the behavior.
 * @param[out] change Set if there are some data changes.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_apply_r(struct lyd_node **first_node, struct lyd_node *parent_node, const struct lyd_node *edit_node,
        enum edit_op parent_op, struct lyd_node *diff_parent, struct lyd_node **diff_root, int oper_edit, int flags,
        int *change)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *match = NULL, *child, *next, *edit_match, *diff_node = NULL;
    enum edit_op op, next_op, prev_op = 0;
    enum insert_val insert;
    const char *key_or_value;
    char *origin;
    int val_equal;

    assert(first_node || (flags & EDIT_APPLY_CHECK_OP_R));
    /* if data node is set, it must be the first sibling */
    assert(!first_node || !*first_node || !(*first_node)->prev->next);

    /* get this node operation */
    if ((err_info = sr_edit_op(edit_node, parent_op, &op, &insert, &key_or_value))) {
        return err_info;
    }

reapply:
    /* find an equal node in the current data */
    if (flags & EDIT_APPLY_CHECK_OP_R) {
        /* we have no data */
        match = NULL;
    } else {
        if ((err_info = sr_edit_find(*first_node, edit_node, op, insert, key_or_value, 1, oper_edit, &match, &val_equal))) {
            return err_info;
        }
    }

    /* apply */
    next_op = op;
    do {
        switch (next_op) {
        case EDIT_REPLACE:
            if (flags & EDIT_APPLY_CHECK_OP_R) {
                sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED,
                        "Node \"%s\" cannot be created because its parent does not exist.", LYD_NAME(edit_node));
                goto op_error;
            }
            if ((err_info = sr_edit_apply_replace(match, val_equal, edit_node, diff_parent, diff_root, &diff_node,
                    &next_op, &flags, change))) {
                goto op_error;
            }
            break;
        case EDIT_CREATE:
            if (flags & EDIT_APPLY_CHECK_OP_R) {
                sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED,
                        "Node \"%s\" cannot be created because its parent does not exist.", LYD_NAME(edit_node));
                goto op_error;
            }
            if ((err_info = sr_edit_apply_create(first_node, parent_node, &match, val_equal, edit_node, diff_parent,
                    diff_root, &diff_node, &next_op, change))) {
                goto op_error;
            }
            break;
        case EDIT_MERGE:
            if (flags & EDIT_APPLY_CHECK_OP_R) {
                sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED,
                        "Node \"%s\" cannot be created because its parent does not exist.", LYD_NAME(edit_node));
                goto op_error;
            }
            if ((err_info = sr_edit_apply_merge(match, val_equal, &next_op))) {
                goto op_error;
            }
            break;
        case EDIT_DELETE:
            if ((err_info = sr_edit_apply_delete(match, val_equal, edit_node, &next_op))) {
                goto op_error;
            }
            break;
        case EDIT_AUTO_REMOVE:
        case EDIT_PURGE:
            prev_op = next_op;
        /* fallthrough */
        case EDIT_REMOVE:
            if ((err_info = sr_edit_apply_remove(first_node, parent_node, &match, val_equal, diff_parent, diff_root,
                    &diff_node, &next_op, &flags, change))) {
                goto op_error;
            }
            break;
        case EDIT_MOVE:
            if ((err_info = sr_edit_apply_move(first_node, parent_node, edit_node, &match, insert, key_or_value,
                    diff_parent, diff_root, &diff_node, &next_op, change))) {
                goto op_error;
            }
            break;
        case EDIT_NONE:
            if ((err_info = sr_edit_apply_none(match, edit_node, diff_parent, diff_root, &diff_node, &next_op, change))) {
                goto op_error;
            }
            break;
        case EDIT_ETHER:
            if ((err_info = sr_edit_apply_ether(match, &next_op, &flags))) {
                goto op_error;
            }
            break;
        case EDIT_CONTINUE:
        case EDIT_FINISH:
            SR_ERRINFO_INT(&err_info);
            return err_info;
        }
    } while ((next_op != EDIT_CONTINUE) && (next_op != EDIT_FINISH));

    /* fix origin in data */
    sr_edit_diff_get_origin(edit_node, &origin, NULL);
    if (match && origin && (err_info = sr_edit_diff_set_origin(match, origin, 1))) {
        free(origin);
        return err_info;
    }

    /* fix origin in diff */
    if (diff_node && origin && (err_info = sr_edit_diff_set_origin(diff_node, origin, 1))) {
        free(origin);
        return err_info;
    }
    free(origin);

    if ((prev_op == EDIT_AUTO_REMOVE) || ((prev_op == EDIT_PURGE) && diff_node)) {
        /* we have removed one subtree of data from another case/one default leaf-list instance/one purged instance,
         * try this whole edit again */
        prev_op = 0;
        diff_node = NULL;
        goto reapply;
    } else if (next_op == EDIT_FINISH) {
        return NULL;
    }

    /* next recursive iteration */
    if (flags & EDIT_APPLY_CHECK_OP_R) {
        /* once we start just checking operations, we do not want to work with diff in recursive calls */
        diff_parent = NULL;
        diff_root = NULL;
    }

    if (diff_root) {
        /* update diff parent */
        diff_parent = diff_node;
    }

    if (flags & EDIT_APPLY_REPLACE_R) {
        /* remove all non-default children that are not in the edit, recursively */
        LY_LIST_FOR_SAFE(lyd_child_no_keys(match), next, child) {
            if (child->flags & LYD_DEFAULT) {
                continue;
            }

            if ((err_info = sr_edit_find(lyd_child_no_keys(edit_node), child, EDIT_DELETE, 0, NULL, 0, oper_edit,
                    &edit_match, NULL))) {
                return err_info;
            }
            if (!edit_match && (err_info = sr_edit_apply_r(&((struct lyd_node_inner *)match)->child, match, child,
                    EDIT_DELETE, diff_parent, diff_root, oper_edit, flags, change))) {
                return err_info;
            }
        }
    }

    /* apply edit recursively */
    LY_LIST_FOR(lyd_child_no_keys(edit_node), child) {
        if (flags & EDIT_APPLY_CHECK_OP_R) {
            /* we do not operate with any datastore data or diff anymore */
            err_info = sr_edit_apply_r(NULL, NULL, child, op, NULL, NULL, oper_edit, flags, change);
        } else {
            err_info = sr_edit_apply_r(&((struct lyd_node_inner *)match)->child, match, child, op, diff_parent,
                    diff_root, oper_edit, flags, change);
        }
        if (err_info) {
            return err_info;
        }
    }

    if (diff_root && diff_parent) {
        /* remove any redundant nodes */
        if (sr_diff_is_redundant(diff_parent)) {
            if (diff_parent == *diff_root) {
                *diff_root = (*diff_root)->next;
            }
            lyd_free_tree(diff_parent);
        }
    }

    return NULL;

op_error:
    assert(err_info);
    sr_errinfo_new(&err_info, err_info->err[0].err_code, "Applying operation \"%s\" failed.", sr_edit_op2str(op));
    return err_info;
}

sr_error_info_t *
sr_edit_mod_apply(const struct lyd_node *edit, const struct lys_module *ly_mod, int oper_edit, struct lyd_node **data,
        struct lyd_node **diff, int *change)
{
    sr_error_info_t *err_info = NULL;
    const struct lyd_node *root;
    struct lyd_node *mod_diff = NULL;

    if (change) {
        *change = 0;
    }

    LY_LIST_FOR(edit, root) {
        if (lyd_owner_module(root) != ly_mod) {
            /* skip data nodes from different modules */
            continue;
        }

        /* apply relevant nodes from the edit datatree */
        if ((err_info = sr_edit_apply_r(data, NULL, root, EDIT_CONTINUE, NULL, diff ? &mod_diff : NULL, oper_edit, 0,
                change))) {
            goto cleanup;
        }

        if (diff && mod_diff) {
            /* merge diffs */
            if (!*diff) {
                *diff = mod_diff;
                mod_diff = NULL;
            } else {
                if (lyd_diff_merge_tree(diff, NULL, mod_diff, NULL, NULL, 0)) {
                    goto cleanup;
                }
                lyd_free_siblings(mod_diff);
                mod_diff = NULL;
            }
        }
    }

cleanup:
    lyd_free_siblings(mod_diff);
    return err_info;
}

/**
 * @brief Check whether a descendant operation should replace a parent operation (is superior to).
 * Also, check whether the operation is even allowed.
 *
 * @param[in,out] new_op Descendant operation, may be rewritten for the actual updated operation if @p is_superior is 1.
 * @param[in] cur_op Parent operation (that will be inherited by default).
 * @param[out] is_superior non-zero if the new operation is superior (replace the current operation), 0 if not.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_is_superior_op(enum edit_op *new_op, enum edit_op cur_op, int *is_superior)
{
    sr_error_info_t *err_info = NULL;

    *is_superior = 0;

    /* actually, cur_op cannot be purge because that would mean a descendant node was created and
     * since this can happen only for lists without keys, there is no way to address them (and create descendants),
     * but whatever, be robust */

    switch (cur_op) {
    case EDIT_CREATE:
        if ((*new_op == EDIT_DELETE) || (*new_op == EDIT_REPLACE) || (*new_op == EDIT_REMOVE) || (*new_op == EDIT_PURGE)) {
            goto op_error;
        }
        /* do not overwrite */
        break;
    case EDIT_DELETE:
    case EDIT_PURGE:
        /* no operation allowed */
        goto op_error;
    case EDIT_REPLACE:
        if ((*new_op == EDIT_DELETE) || (*new_op == EDIT_REMOVE) || (*new_op == EDIT_PURGE)) {
            goto op_error;
        }
        /* do not overwrite */
        break;
    case EDIT_REMOVE:
        if ((*new_op == EDIT_DELETE) || (*new_op == EDIT_REPLACE)) {
            goto op_error;
        } else if ((*new_op == EDIT_CREATE) || (*new_op == EDIT_MERGE)) {
            /* remove + create/merge = replace */
            *new_op = EDIT_REPLACE;
            *is_superior = 1;
        }
        break;
    case EDIT_MERGE:
        if ((*new_op == EDIT_DELETE) || (*new_op == EDIT_REPLACE) || (*new_op == EDIT_REMOVE) || (*new_op == EDIT_PURGE)) {
            goto op_error;
        }
        if (*new_op == EDIT_REPLACE) {
            *is_superior = 1;
        }
        break;
    case EDIT_NONE:
        if ((*new_op == EDIT_REPLACE) || (*new_op == EDIT_MERGE)) {
            *is_superior = 1;
        }
        break;
    case EDIT_ETHER:
        if ((*new_op == EDIT_REPLACE) || (*new_op == EDIT_MERGE) || (*new_op == EDIT_NONE)) {
            *is_superior = 1;
        }
        break;
    default:
        SR_ERRINFO_INT(&err_info);
        return err_info;
    }

    return NULL;

op_error:
    sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED, "Operation \"%s\" cannot have children with operation \"%s\".",
            sr_edit_op2str(cur_op), sr_edit_op2str(*new_op));
    return err_info;
}

static sr_error_info_t *
sr_edit_add_check_same_node_op(sr_session_ctx_t *session, const char *xpath, const char *value, enum edit_op op,
        LY_ERR lyrc)
{
    sr_error_info_t *err_info = NULL;
    char *uniq_xpath;
    enum edit_op cur_op;
    struct ly_set *set;
    LY_ERR lr;

    if (lyrc == LY_EEXIST) {
        /* build an expression identifying a single node */
        if (value && (xpath[strlen(xpath) - 1] != ']')) {
            if (asprintf(&uniq_xpath, "%s[.='%s']", xpath, value) == -1) {
                uniq_xpath = NULL;
            }
        } else {
            uniq_xpath = strdup(xpath);
        }
        if (!uniq_xpath) {
            SR_ERRINFO_MEM(&err_info);
            return err_info;
        }

        /* find the node */
        lr = lyd_find_xpath(session->dt[session->ds].edit, uniq_xpath, &set);
        free(uniq_xpath);
        if (lr || (set->count > 1)) {
            SR_ERRINFO_INT(&err_info);
        } else if (set->count == 1) {
            cur_op = sr_edit_diff_find_oper(set->dnodes[0], 1, NULL);
            if (op == cur_op) {
                /* same node with same operation, silently ignore and clear the error */
                ly_set_free(set, NULL);
                ly_err_clean(session->conn->ly_ctx, NULL);
                return NULL;
            } else {
                ly_err_clean(session->conn->ly_ctx, NULL);
                sr_errinfo_new(&err_info, SR_ERR_UNSUPPORTED, "Node \"%s\" already in edit with \"%s\" operation "
                        "(new operation \"%s\").", LYD_NAME(set->dnodes[0]), sr_edit_op2str(cur_op), sr_edit_op2str(op));
            }
        } /* else set->number == 0; it must be leaf and there already is one with another value, error */
        ly_set_free(set, NULL);
    }

    if (!err_info) {
        sr_errinfo_new_ly(&err_info, session->conn->ly_ctx);
    }
    sr_errinfo_new(&err_info, SR_ERR_INVAL_ARG, "Invalid datastore edit.");
    return err_info;
}

sr_error_info_t *
sr_edit_set_oper(struct lyd_node *edit, const char *op)
{
    const char *attr_full_name;
    sr_error_info_t *err_info = NULL;

    if (!strcmp(op, "none") || !strcmp(op, "ether") || !strcmp(op, "purge")) {
        attr_full_name = "sysrepo:operation";
    } else {
        attr_full_name = "ietf-netconf:operation";
    }

    if (edit->schema) {
        if (lyd_new_meta(LYD_CTX(edit), edit, NULL, attr_full_name, op, 0, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(edit));
            return err_info;
        }
    } else {
        if (lyd_new_attr(edit, NULL, attr_full_name, op, NULL)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(edit));
            return err_info;
        }
    }

    return NULL;
}

static const char *
sr_edit_pos2str(sr_move_position_t position)
{
    switch (position) {
    case SR_MOVE_BEFORE:
        return "before";
    case SR_MOVE_AFTER:
        return "after";
    case SR_MOVE_FIRST:
        return "first";
    case SR_MOVE_LAST:
        return "last";
    default:
        return NULL;
    }
}

/**
 * @brief Check new created nodes for forbidden node types.
 *
 * @param[in] parent Created parent.
 * @param[in] node Created node.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_add_check(struct lyd_node *parent, struct lyd_node *node)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *iter;

    for (iter = node; iter != lyd_parent(parent); iter = lyd_parent(iter)) {
        /* check allowed node types */
        if (iter->schema && (iter->schema->nodetype & (LYS_RPC | LYS_ACTION | LYS_NOTIF))) {
            sr_errinfo_new(&err_info, SR_ERR_INVAL_ARG, "RPC/action/notification node \"%s\" cannot be created.",
                    iter->schema->name);
            return err_info;
        }
    }

    return NULL;
}

/**
 * @brief Update parent operations if a new descendant was created.
 *
 * @param[in] node Created node.
 * @param[in] def_operation Default operation.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_add_update_op(struct lyd_node *node, const char *def_operation)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *sibling, *parent;
    enum edit_op op = 0, def_op;
    int own_oper, next_iter_oper, is_sup;

    next_iter_oper = 0;
    for (parent = lyd_parent(node); parent; node = parent, parent = lyd_parent(parent)) {
        if (next_iter_oper) {
            /* we already got and checked the operation before */
            next_iter_oper = 0;
        } else {
            op = sr_edit_diff_find_oper(parent, 1, &own_oper);
            assert(op);

            def_op = sr_edit_str2op(def_operation);
            if ((err_info = sr_edit_is_superior_op(&def_op, op, &is_sup))) {
                return err_info;
            }
            if (!is_sup) {
                /* the parent operation stays so we are done */
                break;
            }
        }

        for (sibling = lyd_child_no_keys(parent); sibling; sibling = sibling->next) {
            if (sibling == node) {
                continue;
            }

            /* there was already another sibling, set its original operation if it does not have any */
            if (!sr_edit_diff_find_oper(sibling, 0, NULL)) {
                if ((err_info = sr_edit_set_oper(sibling, sr_edit_op2str(op)))) {
                    return err_info;
                }
            }
        }

        if (own_oper) {
            /* the operation is defined on the node, delete it */
            sr_edit_del_meta_attr(parent, "operation");

            if (parent->parent) {
                /* check whether our operation is superior even to the next defined operation */
                op = sr_edit_diff_find_oper(lyd_parent(parent), 1, &own_oper);
                assert(op);
                next_iter_oper = 1;
            }

            def_op = sr_edit_str2op(def_operation);
            if ((err_info = sr_edit_is_superior_op(&def_op, op, &is_sup))) {
                return err_info;
            }
            if (!parent->parent || !is_sup) {
                /* it is not, set it on this parent and finish */
                if ((err_info = sr_edit_set_oper(parent, sr_edit_op2str(def_op)))) {
                    return err_info;
                }
                break;
            }
        }
    }

    return NULL;
}

sr_error_info_t *
sr_edit_add(sr_session_ctx_t *session, const char *xpath, const char *value, const char *operation,
        const char *def_operation, const sr_move_position_t *position, const char *keys, const char *val,
        const char *origin, int isolate)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *node = NULL, *p, *parent = NULL;
    const char *meta_val = NULL, *def_origin;
    enum edit_op op;
    int opts, own_oper;
    LY_ERR lyrc;

    assert(!origin || strchr(origin, ':'));

    /* operational DS cannot transform opaque nodes to diff */
    opts = 0;
    if (SR_IS_CONVENTIONAL_DS(session->ds) && (!strcmp(operation, "remove") || !strcmp(operation, "delete") ||
            !strcmp(operation, "purge"))) {
        opts |= LYD_NEW_PATH_OPAQ;
    }

    /* merge the change into existing edit */
    lyrc = lyd_new_path2(isolate ? NULL : session->dt[session->ds].edit, session->conn->ly_ctx, xpath, (void *)value,
            value ? strlen(value) : 0, LYD_ANYDATA_STRING, opts, &parent, &node);
    if (!lyrc && !node) {
        /* NP container existed already (and is always default) */
        lyrc = LY_EEXIST;
    }
    if (lyrc) {
        /* check whether it is an error */
        if ((err_info = sr_edit_add_check_same_node_op(session, xpath, value, sr_edit_str2op(operation), lyrc))) {
            goto error_safe;
        }
        /* node with the same operation already exists, silently ignore */
        return NULL;
    }
    session->dt[session->ds].edit = lyd_first_sibling(session->dt[session->ds].edit);

    /* check all the created nodes for forbidden ones */
    if ((err_info = sr_edit_add_check(parent, node))) {
        goto error_safe;
    }

    if (isolate) {
        /* connect into one edit */
        lyd_insert_sibling(session->dt[session->ds].edit, parent, &session->dt[session->ds].edit);
    }

    /* check arguments */
    if (position) {
        if (!lysc_is_userordered(node->schema)) {
            sr_errinfo_new(&err_info, SR_ERR_INVAL_ARG, "Position can be specified only for user-ordered nodes.");
            goto error_safe;
        }
        if (node->schema->nodetype == LYS_LIST) {
            if (((*position == SR_MOVE_BEFORE) || (*position == SR_MOVE_AFTER)) && !keys) {
                sr_errinfo_new(&err_info, SR_ERR_INVAL_ARG, "Missing relative item for a list move operation.");
                goto error_safe;
            }
            meta_val = keys;
        } else {
            if (((*position == SR_MOVE_BEFORE) || (*position == SR_MOVE_AFTER)) && !val) {
                sr_errinfo_new(&err_info, SR_ERR_INVAL_ARG, "Missing relative item for a leaf-list move operation.");
                goto error_safe;
            }
            meta_val = val;
        }
    }

    op = sr_edit_diff_find_oper(node, 1, &own_oper);
    if (!op) {
        /* add default operation if a new subtree was created */
        if ((parent != node) && ((err_info = sr_edit_set_oper(parent, def_operation)))) {
            goto error_safe;
        }

        if (!session->dt[session->ds].edit) {
            session->dt[session->ds].edit = parent;
        }
    } else {
        assert(session->dt[session->ds].edit && !isolate);

        /* update operations throughout the edit subtree */
        if ((err_info = sr_edit_add_update_op(node, def_operation))) {
            goto error;
        }
    }

    /* add the operation of the node */
    if ((err_info = sr_edit_set_oper(node, operation))) {
        goto error;
    }
    if (position) {
        if (lyd_new_meta(session->conn->ly_ctx, node, NULL, "yang:insert", sr_edit_pos2str(*position), 0, NULL)) {
            sr_errinfo_new_ly(&err_info, session->conn->ly_ctx);
            goto error;
        }
        if (((*position == SR_MOVE_BEFORE) || (*position == SR_MOVE_AFTER)) && lyd_new_meta(session->conn->ly_ctx, node,
                NULL, (node->schema->nodetype == LYS_LIST) ? "yang:key" : "yang:value", meta_val, 0, NULL)) {
            sr_errinfo_new_ly(&err_info, session->conn->ly_ctx);
            goto error;
        }
    }

    if (session->ds == SR_DS_OPERATIONAL) {
        /* add parent origin */
        for (p = lyd_parent(node); p; p = lyd_parent(p)) {
            /* add origin */
            if (p->schema->flags & LYS_CONFIG_R) {
                def_origin = SR_OPER_ORIGIN;
            } else {
                def_origin = SR_CONFIG_ORIGIN;
            }
            if ((err_info = sr_edit_diff_set_origin(p, def_origin, 0))) {
                goto error;
            }
        }

        /* add node origin */
        if ((err_info = sr_edit_diff_set_origin(node, origin, 1))) {
            goto error;
        }
    }

    return NULL;

error:
    if (!isolate) {
        /* completely free the current edit because it could have already been modified */
        lyd_free_all(session->dt[session->ds].edit);
        session->dt[session->ds].edit = NULL;

        sr_errinfo_new(&err_info, SR_ERR_OPERATION_FAILED, "Edit was discarded.");
        return err_info;
    }
    /* fallthrough */
error_safe:
    /* free only the created subtree */
    if (parent && (session->dt[session->ds].edit == parent)) {
        session->dt[session->ds].edit = parent->next;
    }
    lyd_free_tree(parent);
    return err_info;
}

sr_error_info_t *
sr_diff_set_getnext(struct ly_set *set, uint32_t *idx, struct lyd_node **node, sr_change_oper_t *op)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_meta *meta;
    struct lyd_node *parent, *key;

    while (*idx < set->count) {
        *node = set->dnodes[*idx];

        /* find the (inherited) operation of the current edit node */
        meta = NULL;
        for (parent = *node; parent; parent = lyd_parent(parent)) {
            meta = lyd_find_meta(parent->meta, NULL, "yang:operation");
            if (meta) {
                break;
            }
        }
        if (!meta) {
            SR_ERRINFO_INT(&err_info);
            return err_info;
        }

        if ((parent != *node) && lysc_is_userordered(parent->schema) && (lyd_get_meta_value(meta)[0] == 'r')) {
            /* do not return changes for descendants of moved userord lists without operation */
            ++(*idx);
            continue;
        }

        /* decide operation */
        if (meta->value.enum_item->name[0] == 'n') {
            /* skip the node */
            ++(*idx);

            /* in case of lists we want to also skip all their keys */
            if ((*node)->schema->nodetype == LYS_LIST) {
                for (key = lyd_child(*node); key && (key->schema->flags & LYS_KEY); key = key->next) {
                    ++(*idx);
                }
            }
            continue;
        } else if (meta->value.enum_item->name[0] == 'c') {
            *op = SR_OP_CREATED;
        } else if (meta->value.enum_item->name[0] == 'd') {
            *op = SR_OP_DELETED;
        } else if (meta->value.enum_item->name[0] == 'r') {
            if ((*node)->schema->nodetype & (LYS_LEAF | LYS_ANYDATA)) {
                *op = SR_OP_MODIFIED;
            } else if ((*node)->schema->nodetype & (LYS_LIST | LYS_LEAFLIST)) {
                *op = SR_OP_MOVED;
            } else {
                SR_ERRINFO_INT(&err_info);
                return err_info;
            }
        }

        /* success */
        ++(*idx);
        return NULL;
    }

    /* no more changes */
    *node = NULL;
    return NULL;
}

/**
 * @brief Relink and adjust edit node from stored oper data into change edit for subscribers.
 *
 * @param[in] node Edit node, is used.
 * @param[in,out] change_edit Change edit to append to. If NULL, do not generate change edit.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_oper_del_node(struct lyd_node *node, struct lyd_node **change_edit)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *new_parent, *parent, *match;
    enum edit_op op, cur_op;
    int own_op;

    if (!change_edit) {
        /* nothing to do */
        goto cleanup;
    }

    /* find parents in the change edit */
    if ((err_info = sr_edit_diff_create_parents(node, change_edit, &new_parent, &parent))) {
        goto cleanup;
    }

    /* learn the operation */
    op = sr_edit_diff_find_oper(node, 1, &own_op);
    assert(op);
    if ((op == EDIT_MERGE) || (op == EDIT_REMOVE)) {
        /* reverse operation */
        if (op == EDIT_MERGE) {
            op = EDIT_REMOVE;
        } else {
            op = EDIT_MERGE;
        }
    } /* else ether - stays */

    if (new_parent || lyd_find_sibling_first(parent ? lyd_child(parent) : *change_edit, node, &match)) {
        /* set parent operation, if any new */
        if (new_parent && (err_info = sr_edit_set_oper(new_parent, "ether"))) {
            goto cleanup;
        }

        /* remove the current operation */
        if (own_op) {
            sr_edit_del_meta_attr(node, "operation");
        }

        /* set new operation */
        if ((err_info = sr_edit_set_oper(node, sr_edit_op2str(op)))) {
            goto cleanup;
        }

        /* link to the change edit */
        lyd_unlink_tree(node);
        if (parent) {
            lyd_insert_child(parent, node);
        } else {
            lyd_insert_sibling(*change_edit, node, change_edit);
        }
        node = NULL;
    } else {
        /* node already exists so keep it, learn its operation */
        cur_op = sr_edit_diff_find_oper(match, 1, &own_op);

        if (cur_op != op) {
            /* remove current operation */
            if (own_op) {
                sr_edit_del_meta_attr(match, "operation");
            }

            /* set new operation */
            if ((err_info = sr_edit_set_oper(match, sr_edit_op2str(op)))) {
                goto cleanup;
            }
        }
    }

cleanup:
    lyd_free_tree(node);
    return err_info;
}

/**
 * @brief Update a stored edit subtree by deleting nodes belonging to a connection and optionally selected by an xpath.
 *
 * @param[in] subtree Subtree to update, may be freed.
 * @param[in] cid CID of the deleted connection.
 * @param[in] parent_cid CID effective for (inherited from) the @p subtree parent.
 * @param[in] set Set of nodes selected by an xpath, only these can be deleted. If NULL, all the nodes can be deleted.
 * @param[out] child_cid_p CID effective for the @p subtree, 0 if it was deleted.
 * @param[in,out] change_edit Optional change edit created for subscribers based on the changes made in oper edit.
 * @return err_info, NULL on success.
 */
static sr_error_info_t *
sr_edit_oper_del_r(struct lyd_node *subtree, sr_cid_t cid, sr_cid_t parent_cid, struct ly_set *set,
        sr_cid_t *child_cid_p, struct lyd_node **change_edit)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *next, *child;
    struct lyd_meta *meta;
    sr_cid_t cur_cid, child_cid, ch_cid;
    char cid_str[11];

    /* find our CID attribute, if any */
    meta = lyd_find_meta(subtree->meta, NULL, "sysrepo:cid");
    if (meta) {
        cur_cid = meta->value.uint32;
    } else {
        cur_cid = parent_cid;
    }

    /* process children */
    child_cid = 0;
    LY_LIST_FOR_SAFE(lyd_child_no_keys(subtree), next, child) {
        if ((err_info = sr_edit_oper_del_r(child, cid, cur_cid, set, &ch_cid, change_edit))) {
            return err_info;
        }

        /* try to find a child with the parent CID, then we can simply keep it */
        if (ch_cid && (!child_cid || (child_cid != parent_cid))) {
            child_cid = ch_cid;
        }
    }

    if ((cur_cid != cid) || (set && !ly_set_contains(set, subtree, NULL))) {
        /* this node is not owned by the connection or not selected by the xpath, the subtree is kept */
        *child_cid_p = cur_cid;
        return NULL;
    }

    if (child_cid) {
        /* this node was "deleted" but there are still some children */
        if (meta) {
            lyd_free_meta_single(meta);
        }
        if (parent_cid != child_cid) {
            /* update the owner of this node */
            sprintf(cid_str, "%" PRIu32, child_cid);
            if (lyd_new_meta(NULL, subtree, NULL, "sysrepo:cid", cid_str, 0, NULL)) {
                sr_errinfo_new_ly(&err_info, LYD_CTX(subtree));
                return err_info;
            }
        }

        /* this subtree is kept */
        *child_cid_p = child_cid;
    } else {
        /* there are no children left and this node belongs to the deleted connection, relink it to change_edit */
        if ((err_info = sr_edit_oper_del_node(subtree, change_edit))) {
            return err_info;
        }

        /* this subtree was deleted */
        *child_cid_p = 0;
    }

    return NULL;
}

sr_error_info_t *
sr_edit_oper_del(struct lyd_node **edit, sr_cid_t cid, const char *xpath, struct lyd_node **change_edit)
{
    sr_error_info_t *err_info = NULL;
    struct lyd_node *next, *elem;
    struct ly_set *set = NULL;
    sr_cid_t child_cid;

    if (!*edit) {
        return NULL;
    }

    if (xpath) {
        if (lyd_find_xpath(*edit, xpath, &set)) {
            sr_errinfo_new_ly(&err_info, LYD_CTX(*edit));
            goto cleanup;
        }
        if (!set->count) {
            /* no data matches the xpath */
            goto cleanup;
        }
    }

    LY_LIST_FOR_SAFE(*edit, next, elem) {
        if ((err_info = sr_edit_oper_del_r(elem, cid, 0, set, &child_cid, change_edit))) {
            goto cleanup;
        }

        if (!child_cid && (*edit == elem)) {
            /* first top-level node was removed, move the edit */
            *edit = next;
        }
    }

cleanup:
    ly_set_free(set, NULL);
    return err_info;
}
