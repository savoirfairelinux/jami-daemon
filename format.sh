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
        echo No files to format.
        exit 0
    fi
}

install_hook()
{
    # check for lone repo ring-daemon
    hooks_path=".git/hooks"
    if [ ! -d "$hooks_path=" ]; then
        # or ring-project
        hooks_path="../.git/modules/daemon/hooks"
        if [ ! -d "$hooks_path" ]; then
            echo "Can't find a git directory."
            exit 1
        fi
    fi
    echo Installing pre-commit hook in "$hooks_path".
    echo "/bin/bash $0" > "$hooks_path"/pre-commit
    chmod +x "$hooks_path"/pre-commit
}

display_help()
{
    echo "Usage: $0 [OPTION...] -- Clang format source files with a .clang-format file" >&2
    echo
    echo "   --all           format all files instead of only committed ones"
    echo "   --install       install a pre-commit hook to run this script"
    echo
}

if [ "$1" == "-h" ]; then
    display_help
    exit 0
fi

case "${1}" in
  --all )
    files=$(find src -regex '.*\.\(cpp\|hpp\|cc\|cxx\|h\)')
    exit_if_no_files "$files"
    echo Formatting all source files...
    format_files "$files"
    ;;
  --install )
    install_hook
    ;;
  * )
    files=$(git diff-index --cached --name-only HEAD | grep -iE '\.(cpp|cxx|cc|h|hpp)')
    exit_if_no_files "$files"
    echo Formatting committed source files...
    format_files "$files"
    ;;
esac
