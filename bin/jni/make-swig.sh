#!/usr/bin/env bash
#
#  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

PACKAGE_PATH="$PACKAGEDIR/${PACKAGE//.//}"
mkdir -p $PACKAGE_PATH

echo "Generating jami_wrapper.cpp and java bindings to $PACKAGE_PATH"
swig -v -c++ -java \
-package $PACKAGE \
-outdir $PACKAGE_PATH \
-o $JNIDIR/jami_wrapper.cpp $JNIDIR/jni_interface.i

echo "Generating jamiservice_loader.c..."
python $JNIDIR/JavaJNI2CJNI_Load.py \
-i $PACKAGE_PATH/JamiServiceJNI.java \
-o $JNIDIR/jamiservice_loader.c \
-t $JNIDIR/jamiservice.c.template \
-m JamiService \
-p $PACKAGE

echo "Appending jami_wrapper.cpp..."
cat $JNIDIR/jamiservice_loader.c >> $JNIDIR/jami_wrapper.cpp

echo "SWIG bindings successfully generated !"
exit 0
