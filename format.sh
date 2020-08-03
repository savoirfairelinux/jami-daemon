#!/bin/bash

format_file()
{
    if [ -f "${1}" ]; then
        clang-format -i -style=file "${1}"
    fi
}

format_files()
{
    for file in $1; do
        echo -ne "Formatting: ${file}\\033[0K\\r"
        format_file "${file}"
    done
}

exit_if_no_files()
{
    if [ -z "$1" ]; then
        echo No files to format
        exit 1
    fi
}

display_help()
{
    echo "Usage: $0 [OPTION...] -- Clang format source files with a .clang-format file" >&2
    echo
    echo "   -c           format only committed files"
    echo
}

if [ "$1" == "-h" ]; then
    display_help
    exit 0
fi

case "${1}" in
  -c )
    files=$(git diff-index --cached --name-only HEAD | grep -iE '\.(cpp|cxx|cc|h|hpp)')
    exit_if_no_files "$files"
    echo Formatting committed source files...
    format_files "$files"
    ;;
  * )
    files=$(find src -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)')
    exit_if_no_files "$files"
    echo Formatting all source files...
    format_files "$files"
    ;;
esac
