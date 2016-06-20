#!/bin/sh

# Program to use the command install recursivly in a folder

magic_func() {
    echo "entering ${1}"
    echo "target $2"

    for file in $1; do
		if [ -f "$file" ]; then
			echo "file : $file"
            echo "installing into $2/$file"

            install -D $file $2/$file

		elif [ -d "$file" ]; then
			echo "directory : $file"
			magic_func "$file/*" "$2"

		else
			echo "not recognized : $file"

		fi
	done

}

magic_func "$1" "$2"
