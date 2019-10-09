/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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
  #define DRING_IMPORT __declspec(dllimport)
  #define DRING_EXPORT __declspec(dllexport)
  #define DRING_HIDDEN
#else
  #define DRING_IMPORT __attribute__ ((visibility ("default")))
  #define DRING_EXPORT __attribute__ ((visibility ("default")))
  #define DRING_HIDDEN __attribute__ ((visibility ("hidden")))
#endif

// Now we use the generic helper definitions above to define DRING_PUBLIC and DRING_LOCAL.
// DRING_PUBLIC is used for the public API symbols. It is either DLL imports or DLL exports (or does nothing for static build)
// DRING_LOCAL is used for non-api symbols.

#ifdef dring_EXPORTS // defined if DRing is compiled as a shared library
  #ifdef DRING_BUILD // defined if we are building the DRing shared library (instead of using it)
    #define DRING_PUBLIC DRING_EXPORT
  #else
    #define DRING_PUBLIC DRING_IMPORT
  #endif // DRING_BUILD
  #define DRING_LOCAL DRING_HIDDEN
#else // dring_EXPORTS is not defined: this means DRing is a static lib.
  #define DRING_PUBLIC
  #define DRING_LOCAL
#endif // dring_EXPORTS

#ifdef NDEBUG
  #define DRING_TESTABLE DRING_EXPORT
#else
  #define DRING_TESTABLE
#endif