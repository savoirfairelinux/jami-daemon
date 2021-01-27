#!/usr/bin/env bash
#
#  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
#
#  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
#          Alexandre Lision <alexandre.lision@savoirfairelinux.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <https://www.gnu.org/licenses/>.
set -e

JNIDIR=`pwd`
PACKAGE=net.jami.daemon

if [ -z "$PACKAGEDIR" ]; then
   echo "Define PACKAGEDIR: output dir of generated java files"
   exit 1
fi

echo "Checking for SWIG version 4.0.0 or later"
SWIGVER=`swig -version | grep -i "SWIG version" | awk '{print $3}'`
SWIGVER1=`echo $SWIGVER | awk '{split($0, array, ".")} END{print array[1]}'`
SWIGVER2=`echo $SWIGVER | awk '{split($0, array, ".")} END{print array[2]}'`
SWIGVER3=`echo $SWIGVER | awk '{split($0, array, ".")} END{print array[3]}'`
if [[ $SWIGVER1 -lt 4 ]]; then
    echo "error: SWIG version $SWIGVER1.$SWIGVER2.$SWIGVER3 is less than 4.x"
    exit 3
fi

mkdir -p $PACKAGEDIR

echo "Generating ring_wrapper.cpp..."
swig -v -c++ -java \
-package $PACKAGE \
-outdir $PACKAGEDIR \
-o $JNIDIR/ring_wrapper.cpp $JNIDIR/jni_interface.i

echo "Generating ringservice_loader.c..."
python $JNIDIR/JavaJNI2CJNI_Load.py \
-i $PACKAGEDIR/RingserviceJNI.java \
-o $JNIDIR/ringservice_loader.c \
-t $JNIDIR/ringservice.c.template \
-m Ringservice \
-p $PACKAGE

echo "Appending ring_wrapper.cpp..."
cat $JNIDIR/ringservice_loader.c >> $JNIDIR/ring_wrapper.cpp

echo "SWIG bindings successfully generated !"
exit 0
