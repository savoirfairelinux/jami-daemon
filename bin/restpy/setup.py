#!/usr/bin/env python
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

import sys, distutils
from setuptools import setup, find_packages, Extension

try:
    from Cython.Build import cythonize
    from Cython.Distutils import build_ext

except ImportError:
    from pip import pip
    pip.main(['install', 'cython'])

    from Cython.Build import cythonize
    from Cython.Distutils import build_ext

ext_dring = Extension(
    'ring_api/dring_cython',
    sources=[
	'ring_api/wrappers/dring_cython.pyx',
	'ring_api/callbacks/cb_client.cpp'
    ],
    language='c++',
    extra_compile_args=[
	'-std=c++11'
    ],
    extra_link_args=[
	'-std=c++11'
    ],
    include_dirs = [
	'/usr/include/dring',
	'extra/hpp/',
	'ring_api/callbacks/',
	'ring_api/wrappers/'
    ],
    libraries=[
	'ring'
    ],
)

setup(
    name='ring_api',
    version='0.1.0.dev1',
    description='Python bindings on the Ring-daemon library',
    url='https://github.com/sevaivanov/ring-api',
    author='Seva Ivanov',
    author_email='seva.ivanov@savoirfairelinux.com',
    license='GPLv3+',
    keywords='ring ring.cx ring-api ring_api',
    platforms='any',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Build Tools',
        'License :: OSI Approved :: GNU General Public License v3 or later (GPLv3+)',
        # TODO test earlier
        'Programming Language :: Python :: 3.5',
    ],

    # read: source vs build distribution;1 install: pip install -e .[wiki]
    packages=find_packages(exclude=['wiki', 'tests*']),

    install_requires=[
        'flask',
        'flask_restful',
        'websockets'
    ],

    # Generate shared library
    ext_modules=cythonize(ext_dring),
    cmdclass={
        'build_ext' : build_ext
    },
)
