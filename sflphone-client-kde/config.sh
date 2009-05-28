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

autocmd cmake $@ ..

echo $@

echo "**********************************************"
echo "Configuration done!" 
echo "Run \`cd build\' to go to the build directory."
echo "Then run \`make\'to build the software."
echo "**********************************************"
