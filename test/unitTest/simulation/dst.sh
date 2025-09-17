#!/usr/bin/bash

BUILD_DIR=/home/ierdogan/Documents/jami-client-qt/daemon/build
DATA_DIR=$HOME/.local/share/jami
export CONV_ID=""

DO_BUILD=0
DO_RUN=0
DO_RESET=0
DO_LIST_ACCOUNTS=0

# Configuration
NUM_CYCLES=10;
NUM_EVENTS=500;
SAVE_AS_UNIT_TESTS=1;
ENABLE_EVENT_LOGGER=1;
ENABLE_GIT_LOGGER=1;
IS_UNIT_TEST=0
RESET_REPOS=1;
UNIT_TEST_FOLDER_NAME="";

function config_simulation() {
    while true; do
        read -rp "Enter number of cycles [current: ${NUM_CYCLES}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" =~ ^[1-9][0-9]*$ ]]; then NUM_CYCLES=$input; break; else echo "Please enter a positive integer."; fi
    done
    while true; do
        read -rp "Enter number of events [current: ${NUM_EVENTS}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" =~ ^[1-9][0-9]*$ ]]; then NUM_EVENTS=$input; break; else echo "Please enter a positive integer."; fi
    done
    while true; do
        read -rp "Enable event logger? (1=Yes, 0=No) [current ${ENABLE_EVENT_LOGGER}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" == "0" || "$input" == "1" ]]; then ENABLE_EVENT_LOGGER=$input; break; else echo "Please enter 1 (Yes) or 0 (No)."; fi
    done
    while true; do
        read -rp "Enable git logger? (1=Yes, 0=No) [current ${ENABLE_GIT_LOGGER}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" == "0" || "$input" == "1" ]]; then ENABLE_GIT_LOGGER=$input; break; else echo "Please enter 1 (Yes) or 0 (No)."; fi
    done
    SAVE_AS_UNIT_TESTS=0
    UNIT_TEST_FOLDER_NAME=""
    if [[ $NUM_CYCLES -gt 1 ]]; then
        RESET_REPOS=1
    else
        RESET_REPOS=0
    fi
    IS_UNIT_TEST=0
    echo "Simulation configuration updated."
}

function config_unit_test() {
    while true; do
        read -rp "Enter unit test JSON file name [current: ${UNIT_TEST_FOLDER_NAME}]: " input
        if [[ -z "$input" ]]; then
            break
        fi
        config_file="$(dirname "$0")/data/$input"
        if [[ -f "$config_file" ]]; then
            UNIT_TEST_FOLDER_NAME="$input"
            echo "Unit test JSON file set to: $UNIT_TEST_FOLDER_NAME"
            break
        else
            echo "File '$config_file' does not exist. Please try again."
        fi
    done
    while true; do
        read -rp "Enable event logger? (1=Yes, 0=No) [current ${ENABLE_EVENT_LOGGER}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" == "0" || "$input" == "1" ]]; then ENABLE_EVENT_LOGGER=$input; break; else echo "Please enter 1 (Yes) or 0 (No)."; fi
    done
    while true; do
        read -rp "Enable git logger? (1=Yes, 0=No) [current ${ENABLE_GIT_LOGGER}]: " input
        if [[ -z "$input" ]]; then break; fi
        if [[ "$input" == "0" || "$input" == "1" ]]; then ENABLE_GIT_LOGGER=$input; break; else echo "Please enter 1 (Yes) or 0 (No)."; fi
    done
    SAVE_AS_UNIT_TESTS=1
    NUM_CYCLES=0
    NUM_EVENTS=0
    RESET_REPOS=0
    IS_UNIT_TEST=1
    echo "Unit test configuration updated"
}

function write_config() {
    # Path to the C++ config file (always relative to script location)
    SCRIPT_DIR="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 && pwd)"
    CPP_CONFIG_FILE="$SCRIPT_DIR/conversationRepositoryDST.cpp"

    # Use sed to replace the values in the C++ file
    sed -i \
        -e "s/^constexpr int NUM_CYCLES = .*/constexpr int NUM_CYCLES = $NUM_CYCLES;/" \
        -e "s/^constexpr int NUM_EVENTS = .*/constexpr int NUM_EVENTS = $NUM_EVENTS;/" \
        -e "s/^constexpr bool SAVE_AS_UNIT_TESTS = .*/constexpr bool SAVE_AS_UNIT_TESTS = $( [[ $SAVE_AS_UNIT_TESTS -eq 1 ]] && echo true || echo false );/" \
        -e "s/^constexpr bool ENABLE_EVENT_LOGGER = .*/constexpr bool ENABLE_EVENT_LOGGER = $( [[ $ENABLE_EVENT_LOGGER -eq 1 ]] && echo true || echo false );/" \
        -e "s/^constexpr bool ENABLE_GIT_LOGGER = .*/constexpr bool ENABLE_GIT_LOGGER = $( [[ $ENABLE_GIT_LOGGER -eq 1 ]] && echo true || echo false );/" \
        -e "s/^constexpr bool RESET_REPOS = .*/constexpr bool RESET_REPOS = $( [[ $RESET_REPOS -eq 1 ]] && echo true || echo false );/" \
        -e "s/^constexpr bool IS_UNIT_TEST = .*/constexpr bool IS_UNIT_TEST = $( [[ $IS_UNIT_TEST -eq 1 ]] && echo true || echo false );/" \
        -e "s|^constexpr const char\* UNIT_TEST_FOLDER_NAME = .*;|constexpr const char* UNIT_TEST_FOLDER_NAME = \"$UNIT_TEST_FOLDER_NAME\";|" \
        "$CPP_CONFIG_FILE"
    echo "C++ configuration updated in $CPP_CONFIG_FILE"
}

function read_config() {
    echo "--- Current DST Configuration ---"
    if [[ $IS_UNIT_TEST -eq 1 ]]; then
        echo "Mode: Unit Test"
        echo "UNIT_TEST_FOLDER_NAME: $UNIT_TEST_FOLDER_NAME"
    else
        echo "Mode: Simulation"
        echo "NUM_CYCLES: $NUM_CYCLES"
        echo "NUM_EVENTS: $NUM_EVENTS"
        echo "ENABLE_EVENT_LOGGER: $ENABLE_EVENT_LOGGER"
        echo "ENABLE_GIT_LOGGER: $ENABLE_GIT_LOGGER"
        echo "SAVE_AS_UNIT_TESTS: $SAVE_AS_UNIT_TESTS"
        echo "RESET_REPOS: $RESET_REPOS"
    fi
}

function config_dst() {
    echo "Configure the DST"
    while true; do
        read -rp "Select one of the following: Simulation (1), Unit test (2), Current config (3) Exit (4): " opt arg
        case $opt in
            1)
                config_simulation
                write_config
                ;;
            2)
                config_unit_test
                write_config
                ;;
            3)
                read_config
                ;;

            4)
                echo "Exiting"
                break
                ;;
            *)
                echo "Unrecognized option."
                ;;
        esac
    done
}

function list_accounts() {
    cd $DATA_DIR
    output=$(ls -l $DATA_DIR | grep -v '^total')
    if [[ -z "$output" ]]; then
        echo "No accounts found."
    else
        echo "$output"
    fi
}

function log_conversation() {
    if [[ "$1" == "--help" || "$1" == "" ]]; then
        echo "Usage: log [accountID] [--help]"
        return
    fi
    if [[ "$CONV_ID" == "" ]]; then
        echo "No conversation ID found. Please run the DST at least once!"
        return
    fi
    # Remove non-alphanumeric characters from account ID
    account_id=$(echo "$1" | tr -cd '[:alnum:]')
    conv_dir="$DATA_DIR/$account_id/conversations/$CONV_ID"
    if [[ ! -d "$conv_dir" ]]; then
        echo "Directory $conv_dir does not exist."
        return
    fi
    cd "$conv_dir"
    if [[ ! -d .git ]]; then
        echo "Not a git repository: $conv_dir"
        return
    fi
    git log --graph --color=always | cat
}

function reset() {
    if [[ "$1" == "--help" ]]; then
        echo "Usage: reset [--help]"
        echo "Removes all accounts in $DATA_DIR."
        return
    fi

    echo -n "Remove all accounts in $DATA_DIR/ ? [Y/n]:"
    read answer
    if [[ "$answer" =~ ^[Yy]$ || -z "$answer" ]]; then
        cd $DATA_DIR
        rm -rf ./*
        echo "All accounts removed."
    else
        echo "Reset aborted."
    fi
}

function build() {
    echo "Building DST..."
    cd $BUILD_DIR
    make -j
}

function run_dst() {
    echo "Starting DST..."
    cd $BUILD_DIR
    DST_OUTPUT=$(./ut_conversationRepositoryDST 2>&1 | tee /dev/tty)
    conversation_id=$(echo "$DST_OUTPUT" | grep 'CONVERSATION_ID_EXPORT:' | head -n1 | sed 's/.*CONVERSATION_ID_EXPORT://')
    if [[ -n "$conversation_id" ]]; then
        echo "Conversation ID saved:" "$conversation_id"
        CONV_ID="$conversation_id"
        export CONV_ID
    else
        echo "No conversation ID found!"
    fi
}




# Interactive infinite loop shell

echo "~Conversation Repository DST CLI~"
while true; do
    read -rp "Run command, help, or quit: " opt arg
    case "$opt" in
        help)
            echo "Available commands: config, run, reset, accounts, log"
            ;;
        config)
            config_dst
            ;;
        run)
            build
            run_dst
            ;;
        reset)
            reset "$arg"
            ;;
        accounts)
            list_accounts
            ;;
        log)
            log_conversation "$arg"
            ;;
        quit|exit)
            echo "Exiting."
            break
            ;;
        *)
            echo "Unknown option: $opt"
            ;;
    esac
done