#!/bin/sh

echo "Installing SflPhone-KDE"
echo "Creating build directory"
mkdir build
cd build
echo "Executing cmake command"
cmake ../
echo "Changing cmake settings to enable exceptions"
sed -i s/-fno-exceptions/-fexceptions/ ./CMakeFiles/sflphone_kde.dir/flags.make
echo "Executing makefile"
make
echo "Compile done. You may now execute build/sflphone_kde ."