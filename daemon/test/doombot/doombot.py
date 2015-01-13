#!/usr/bin/python3

import sys
import os
import argparse
from config import DefaultConfig

__version__ = "0.3"

config = DefaultConfig()
args = {}

for i in range(len(sys.argv)):
    args[sys.argv[i]] = i

########################################################
#                                                      #
#                   Options validation                 #
#                                                      #
########################################################

_USAGE = """Usage: doombot [options] executable_path [executable_args]
Options:
    --help      (-h)                 : Show options and exit
    --directory (-d) path            : Directory full of scripts to run (optional)
    --script    (-s) path            : Run a single script (optional)
    --port      (-p) port number     : Run the server on a specific port (default:5000)
    --interface (-i) interface_name  : Run the server on a specific network interface
    --continue  (  )                 : Continue execution after a SIGABRT (not recommended)
    --version   (-v)                 : Print the version and exit
"""

parser = argparse.ArgumentParser(description='')

parser.add_argument('--version',
                    help='Print the version and exit',
                    action='version',
                    version='SFL DoomBot %s' % __version__)

parser.add_argument('--directory', '-d',
                    help='Directory full of scripts to run (optional)',
                    dest='directory')

parser.add_argument('--script', '-s',
                    help='Run a single script (optional)',
                    dest='script')

parser.add_argument('--port', '-p',
                    help='Run the server on a specific port',
                    dest='port',
                    type=int,
                    default=config.port)

parser.add_argument('--interface', '-i',
                    help='Run the server on a specific network interface',
                    dest='interface')

parser.add_argument('--continue', '-c',
                    help='Continue execution after a SIGABRT (not recommended)',
                    dest='continue',
                    action='store_true',
                    default=config.cont)

config.command = config.command.strip()
config.args = sub_range

doombot_path = os.path.dirname(os.path.realpath(__file__)) + "/"

if not os.path.exists(doombot_path+"gdb_wrapper.py"):
    print("Wrapper script not found")
    exit(1)


########################################################
#                                                      #
#                    Start the server                  #
#                                                      #
########################################################

print("Starting the DoomBot server")
print("Executable               : " + config.command              )
print("Port                     : " + str( config.port           ))
print("Continue on Asserts      : " + str( config.cont           ))
print("Using a script directory : " + str( config.directory != ""))
print("Using a script           : " + str( config.script != ""   ))
print()

# Start the server
import server
server.app.run()
