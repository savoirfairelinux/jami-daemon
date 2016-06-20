#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

from libcpp cimport bool as boolean

cdef extern from "<memory>" namespace "std" nogil:
    cdef cppclass shared_ptr[T]:
        shared_ptr() except +
        T* get()
        T operator*()
        void reset(T*)

cdef extern from "<future>" namespace "std" nogil:
    cdef cppclass shared_future[T]:
        shared_future() except +
        boolean valid() const

    cdef cppclass future[T]:
        future() except +
        boolean valid() const
        shared_future[T] share()

cdef extern from "<functional>" namespace "std" nogil:
    cdef cppclass function[T]:
        function() except +
        function(const T&)
