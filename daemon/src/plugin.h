/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef PLUGIN_H
#define PLUGIN_H 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#ifdef __cplusplus
#define EXTERNAL_C_LINKAGE extern "C"
#define C_INTERFACE_START EXTERNAL_C_LINKAGE {
#define C_INTERFACE_END }
#else
#define C_LINKAGE
#define C_INTERFACE_START
#define C_INTERFACE_END
#endif

#define SFL_PLUGIN_ABI_VERSION 1 /* 0 doesn't exist, considered as error */
#define SFL_PLUGIN_API_VERSION 1 /* 0 doesn't exist, considered as error */

C_INTERFACE_START;

typedef struct SFLPluginVersion {
    /* plugin is not loadable if this number differs from one
     * stored in the plugin loader */
    uint32_t                            abi;

    /* a difference on api number may be acceptable, see the loader code */
    uint32_t                            api;
} SFLPluginVersion;

struct SFLPluginAPI;

/* SFLPluginCreateFunc parameters */
typedef struct SFLPluginObjectParams {
    const SFLPluginAPI*                 api;
    const int8_t *                      type;
    const struct SFLPluginAPI *         pluginApi;
} SFLPluginObjectParams;

typedef void * (*SFLPluginCreateFunc)(SFLPluginObjectParams *params);

typedef int32_t (*SFLPluginDestroyFunc)(void *object);

/* SFLPluginRegisterFunc parameters */
typedef struct SFLPluginRegisterParams {
    SFLPluginVersion                    version;
    SFLPluginCreateFunc                 create;
    SFLPluginDestroyFunc                destroy;
} SFLPluginRegisterParams;

/* Plugin calls this function to register a supported object type */
typedef int32_t (*SFLPluginRegisterFunc)(const SFLPluginAPI* api,
                                         const int8_t *type,
                                         const SFLPluginRegisterParams *params);

/* Plugin calls this function to invoke a service */
typedef int32_t (*SFLPluginInvokeFunc)(const char *name, void *data);

/* SFLPluginInitFunc parameters.
 * This structure is filled by the Plugin manager.
 */
typedef struct SFLPluginAPI {
    SFLPluginVersion                    version;
    void*                               context; // opaque structure used by next functions
    SFLPluginRegisterFunc               registerObject;
    SFLPluginInvokeFunc                 invokeService;
} SFLPluginAPI;

typedef int32_t (*SFLPluginExitFunc)(void);

typedef SFLPluginExitFunc (*SFLPluginInitFunc)(const SFLPluginAPI *api);

C_INTERFACE_END;

#define SFL_PLUGIN_INIT_STATIC SFL_PLUGIN_INIT
#define SFL_PLUGIN_INIT_DYNAMIC(pname) SFL_PLUGIN_INIT(SFL_dynPluginInit, pname)

/* Define here platform dependent way to export a declaration x to the dynamic
 * loading system.
 */

/* Default case (like POSIX/.so) */

#define SFL_PLUGIN_INIT(fname, pname) \
    EXTERNAL_C_LINKAGE SFLPluginExitFunc fname(const SFLPluginAPI *pname)
#define SFL_PLUGIN_EXIT(fname)                  \
    EXTERNAL_C_LINKAGE int32_t fname(void)

#endif /* PLUGIN_H */
