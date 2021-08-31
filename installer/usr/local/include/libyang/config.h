/**
 * @file config.h
 * @author Radek Krejci <rkrejci@cesnet.cz>
 * @brief Various variables provided by cmake and compile time options.
 *
 * Copyright (c) 2021 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef LY_CONFIG_H_
#define LY_CONFIG_H_

/** size of fixed_mem in lyd_value, minimum is 8 (B) */
#define LYD_VALUE_FIXED_MEM_SIZE 24

/*
 * Plugins
 */
#define LYPLG_SUFFIX ".so"
#define LYPLG_SUFFIX_LEN (sizeof LYPLG_SUFFIX - 1)
#define LYPLG_TYPE_DIR "/usr/local/lib/libyang/extensions"
#define LYPLG_EXT_DIR "/usr/local/lib/libyang/types"

#if (GNU == GNU) || (GNU == Clang)
# define _FORMAT_PRINTF(FORM, ARGS) __attribute__((format (printf, FORM, ARGS)))
#else
# define _FORMAT_PRINTF(FORM, ARGS)
#endif

#endif /* LY_CONFIG_H_ */
