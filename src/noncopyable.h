/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

/**
 * @file noncopyable.h
 * @brief Simple macro to hide class' copy constructor and assignment operator.
 *        Useful to avoid shallow copying (i.e. classes with pointer members)
 *        Usage: For a class named MyClass, the macro call
 *        NON_COPYABLE(MyClass) should go in the private section of MyClass
 *        WARNING: Since C++11 using this macro make the class also non-movable by default!
 *        You shall re-implement yourself (or at least using declare with =default)
 *        move-constructor and move-assignable function members if they are needed.
 */

#define NON_COPYABLE(ClassName) \
    ClassName(const ClassName&) = delete; \
    ClassName& operator=(const ClassName&) = delete
