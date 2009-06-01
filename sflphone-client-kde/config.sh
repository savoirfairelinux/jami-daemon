#!/bin/bash

function autocmd()
{
    echo "Running ${1}..."
        $* || {
            echo "Error running ${1}"
                exit 1
        }
}

if [ ! -d "build" ]; then
	mkdir build
fi

cd build

# prefix=`echo $@ | grep -q "--prefix="`
# 
# if $prefix
# then options=$@" -DCMAKE_INSTALL_PREFIX="$prefix_env

options=`echo $@ | sed "s/--prefix=/-DCMAKE_INSTALL_PREFIX=/g"`

autocmd cmake $options ..

echo $options

echo "**********************************************"
echo "Configuration done!" 
echo "Run \`cd build\' to go to the build directory."
echo "Then run \`make\'to build the software."
echo "**********************************************"
