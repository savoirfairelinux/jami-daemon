/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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
 */

/* Agent */
#include "bindings/bindings.h"

/* Include module's bindings here */
#include "bindings/account.h"
#include "bindings/call.h"
#include "bindings/conversation.h"
#include "bindings/jami.h"
#include "bindings/logger.h"
#include "bindings/signal.h"

void
install_scheme_primitives()
{
    /* Define modules here */
    auto load_module = [](auto name, auto init){
        scm_c_define_module(name, init, NULL);
    };

    load_module("jami", install_jami_primitives);
    load_module("jami account", install_account_primitives);
    load_module("jami call", install_call_primitives);
    load_module("jami conversation", install_conversation_primitives);
    load_module("jami logger bindings", install_logger_primitives);
    load_module("jami signal bindings", install_signal_primitives);
}

/*
 * Register Guile bindings here.
 *
 * 1. Name of the binding
 * 2. Number of required argument to binding
 * 3. Number of optional argument to binding
 * 4. Number of rest argument to binding
 * 5. Pointer to C function to call
 *
 * See info guile:
 *
 * Function: SCM scm_c_define_gsubr(const char *name, int req, int opt, int rst, fcn):
 *
 * Register a C procedure FCN as a “subr” — a primitive subroutine that can be
 * called from Scheme.  It will be associated with the given NAME and bind it in
 * the "current environment".  The arguments REQ, OPT and RST specify the number
 * of required, optional and “rest” arguments respectively.  The total number of
 * these arguments should match the actual number of arguments to FCN, but may
 * not exceed 10.  The number of rest arguments should be 0 or 1.
 * ‘scm_c_make_gsubr’ returns a value of type ‘SCM’ which is a “handle” for the
 * procedure.
 */
void
define_primitive(const char* name, int req, int opt, int rst, void* func)
{
        scm_c_define_gsubr(name, req, opt, rst, func);
        scm_c_export(name, NULL);
}
