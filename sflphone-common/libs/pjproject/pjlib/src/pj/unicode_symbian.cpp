/* $Id: unicode_symbian.cpp 3047 2010-01-06 14:35:13Z bennylp $ */
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
#include <pj/unicode.h>

#include "os_symbian.h"


/*
 * Convert ANSI strings to Unicode strings.
 */
PJ_DEF(wchar_t*) pj_ansi_to_unicode( const char *str, pj_size_t len,
				     wchar_t *wbuf, pj_size_t wbuf_count)
{
    TPtrC8 aForeign((const TUint8*)str, (TInt)len);
    TPtr16 aUnicode((TUint16*)wbuf, (TInt)(wbuf_count-1));
    TInt left;

    left = PjSymbianOS::Instance()->ConvertToUnicode(aUnicode, aForeign);

    if (left != 0) {
	// Error, or there are unconvertable characters
	*wbuf = 0;
    } else {
	if (len < wbuf_count)
	    wbuf[len] = 0;
	else
	    wbuf[len-1] = 0;
    }

    return wbuf;
}


/*
 * Convert Unicode string to ANSI string.
 */
PJ_DEF(char*) pj_unicode_to_ansi( const wchar_t *wstr, pj_size_t len,
				  char *buf, pj_size_t buf_size)
{
    TPtrC16 aUnicode((const TUint16*)wstr, (TInt)len);
    TPtr8 aForeign((TUint8*)buf, (TInt)(buf_size-1));
    TInt left;

    left = PjSymbianOS::Instance()->ConvertFromUnicode(aForeign, aUnicode);

    if (left != 0) {
	// Error, or there are unconvertable characters
	buf[0] = '\0';
    } else {
	if (len < buf_size)
	    buf[len] = '\0';
	else
	    buf[len-1] = '\0';
    }

    return buf;
}


