/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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

#pragma once

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define LIBJAMI_IMPORT __declspec(dllimport)
#define LIBJAMI_EXPORT __declspec(dllexport)
#define LIBJAMI_HIDDEN
#else
#define LIBJAMI_IMPORT __attribute__((visibility("default")))
#define LIBJAMI_EXPORT __attribute__((visibility("default")))
#define LIBJAMI_HIDDEN __attribute__((visibility("hidden")))
#endif

// Now we use the generic helper definitions above to define LIBJAMI_PUBLIC and LIBJAMI_LOCAL.
// LIBJAMI_PUBLIC is used for the public API symbols. It is either DLL imports or DLL exports (or does
// nothing for static build) LIBJAMI_LOCAL is used for non-api symbols.

#ifdef jami_EXPORTS // defined if Jami is compiled as a shared library
#ifdef LIBJAMI_BUILD  // defined if we are building the Jami shared library (instead of using it)
#define LIBJAMI_PUBLIC LIBJAMI_EXPORT
#else
#define LIBJAMI_PUBLIC LIBJAMI_IMPORT
#endif // LIBJAMI_BUILD
#define LIBJAMI_LOCAL LIBJAMI_HIDDEN
#else // jami_EXPORTS is not defined: this means Jami is a static lib.
#define LIBJAMI_PUBLIC
#define LIBJAMI_LOCAL
#endif // jami_EXPORTS

#ifdef DEBUG
#define LIBJAMI_TESTABLE LIBJAMI_EXPORT
#else
#define LIBJAMI_TESTABLE
#endif
