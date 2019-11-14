/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#pragma once

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

#define JAMI_PLUGIN_ABI_VERSION 1 /* 0 doesn't exist, considered as error */
#define JAMI_PLUGIN_API_VERSION 1 /* 0 doesn't exist, considered as error */

C_INTERFACE_START;

typedef struct JAMI_PluginVersion {
  /* plugin is not loadable if this number differs from one
   * stored in the plugin loader */
  uint32_t abi;

  /* a difference on api number may be acceptable, see the loader code */
  uint32_t api;
} JAMI_PluginVersion;

struct JAMI_PluginAPI;

/* JAMI_PluginCreateFunc parameters */
typedef struct JAMI_PluginObjectParams {
  const JAMI_PluginAPI *pluginApi; /* this API */
  const char *type;
} JAMI_PluginObjectParams;

typedef void *(*JAMI_PluginCreateFunc)(JAMI_PluginObjectParams *params,
                                       void *closure);

typedef void (*JAMI_PluginDestroyFunc)(void *object, void *closure);

/* JAMI_PluginAPI.registerObjectFactory data */
typedef struct JAMI_PluginObjectFactory {
  JAMI_PluginVersion version;
  void *closure; /* closure for create */
  JAMI_PluginCreateFunc create;
  JAMI_PluginDestroyFunc destroy;
} JAMI_PluginObjectFactory;

/* Plugins exposed API prototype */
typedef int32_t (*JAMI_PluginFunc)(const JAMI_PluginAPI *api, const char *name,
                                   void *data);

/* JAMI_PluginInitFunc parameters.
 * This structure is filled by the Plugin manager.
 * For backware compatibility, never c
 */
typedef struct JAMI_PluginAPI {
  JAMI_PluginVersion version; /* structure version, always the first data */
  void *context;              /* opaque structure used by next functions */

  /* API usable by plugin implementors */
  JAMI_PluginFunc registerObjectFactory;
  JAMI_PluginFunc invokeService;
} JAMI_PluginAPI;

typedef void (*JAMI_PluginExitFunc)(void);

typedef JAMI_PluginExitFunc (*JAMI_PluginInitFunc)(const JAMI_PluginAPI *api, char const* path);

C_INTERFACE_END;

#define JAMI_DYN_INIT_FUNC_NAME "JAMI_dynPluginInit"
#define JAMI_PLUGIN_INIT_STATIC(fname, pname) JAMI_PLUGIN_INIT(fname, pname)
#define JAMI_PLUGIN_INIT_DYNAMIC(pname)                                        \
  JAMI_PLUGIN_INIT(JAMI_dynPluginInit, pname)

/* Define here platform dependent way to export a declaration x to the dynamic
 * loading system.
 */

/* Default case (like POSIX/.so) */

#define JAMI_PLUGIN_INIT(fname, pname)                                         \
  (EXTERNAL_C_LINKAGE JAMI_PluginExitFunc fname(const JAMI_PluginAPI *pname))
#define JAMI_PLUGIN_EXIT(fname) (EXTERNAL_C_LINKAGE void fname(void))

