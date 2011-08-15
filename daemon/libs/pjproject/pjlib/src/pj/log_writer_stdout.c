/* $Id: log_writer_stdout.c 2660 2009-04-28 19:38:43Z nanang $ */
/* 
 * Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Teluu Inc. (http://www.teluu.com)
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
#include <pj/log.h>
#include <pj/os.h>
#include <pj/compat/stdfileio.h>


static void term_set_color(int level)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
    pj_term_set_color(pj_log_get_color(level));
#else
    PJ_UNUSED_ARG(level);
#endif
}

static void term_restore_color(void)
{
#if defined(PJ_TERM_HAS_COLOR) && PJ_TERM_HAS_COLOR != 0
    /* Set terminal to its default color */
    pj_term_set_color(pj_log_get_color(77));
#endif
}


PJ_DEF(void) pj_log_write(int level, const char *buffer, int len)
{
    PJ_CHECK_STACK();
    PJ_UNUSED_ARG(len);

    /* Copy to terminal/file. */
    if (pj_log_get_decor() & PJ_LOG_HAS_COLOR) {
	term_set_color(level);
	printf("%s", buffer);
	term_restore_color();
    } else {
	printf("%s", buffer);
    }
}

