/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: 
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

/*
	Type definitions and helper macros which aren't part of Standard C++
	This will need to be edited on systems where 'char', 'short' and 'int'
   	have sizes different from 8, 16 and 32 bits.
*/

#ifndef __COMMON_H__
#define __COMMON_H__


// #define DEBUG	/**< Defined when compiling code for debugging */

/*
Basic integer types.
Note 'int' is assumed to be in 2s complement format and at least 32 bits in size
*/
typedef unsigned char  uint8;	/**< An 8 bit unsigned integer */
typedef unsigned short uint16;	/**< An 16 bit unsigned integer */
typedef unsigned int   uint32;	/**< An 32 bit unsigned integer */
typedef signed char    int8;	/**< An 8 bit signed integer (2s complement) */
typedef signed short   int16;	/**< An 16 bit signed integer (2s complement) */
typedef signed int     int32;	/**< An 32 bit signed integer (2s complement) */
typedef unsigned int   uint;	/**< An unsigned integer or at least 32 bits */

#ifndef NULL
#define NULL 0		/**< Used to represent a null pointer type */
#endif

#ifdef _MSC_VER		// Compiling for Microsoft Visual C++

#define DEBUGGER	{ _asm int 3 }			/**< Invoke debugger */
#define IMPORT		__declspec(dllexport)	/**< Mark a function which is to be imported from a DLL */
#define EXPORT		__declspec(dllexport)	/**< Mark a function to be exported from a DLL */
#define ASSERT(c)	{ if(!(c)) DEBUGGER; }	/**< Assert that expression 'c' is true */

#else				// Not compiling for Microsoft Visual C++ ...

#define DEBUGGER							/**< Invoke debugger */
#define IMPORT								/**< Mark a function which is to be imported from a DLL */
#define EXPORT								/**< Mark a function to be exported from a DLL */

#endif

#ifdef DEBUG
#define ASSERT_DEBUG(c) ASSERT(c)	/**< Assert that expression 'c' is true (when compiled for debugging)*/
#else
#define ASSERT_DEBUG(c)
#endif

#endif
