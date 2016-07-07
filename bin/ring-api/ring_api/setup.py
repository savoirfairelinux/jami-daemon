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

from setuptools import setup, Extension
from Cython.Build import cythonize
from Cython.Distutils import build_ext

# It will generate a shared library (.so)
setup(name='dring_cython',
    ext_modules = cythonize(Extension(
        'dring_cython', # library name
        sources=['wrappers/dring_cython.pyx',
            'callbacks/cb_client.cpp'],
        language='c++',
        extra_compile_args=['-std=c++11'],
        extra_link_args=['-std=c++11'],
        include_dirs = ['/usr/include/dring',
            'extra/hpp/', 'callbacks/', 'wrappers/'],
        libraries=['ring'],
    )),
    cmdclass = {'build_ext' : build_ext}
)
