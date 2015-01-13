#!/usr/bin/python
# -*- coding: utf-8 -*-
from flask import Flask
import config
import process_watcher
app = Flask(__name__)

# This file is used to wrap the GDB instances
#
#  Doombot master server
#      |-----> process_watcher threads
#          |----> GDB process                <=== You are here
#              |----->Doombot wrapper
#                   -----> Real process
#
# This may seem a little overkill, but using the same
# process for both GDB and the Web server had too many
# limitations.

# This file is a hardcoded Web UI for the DoomBot. It print tons inflexible HTML
# is it ugly?: Yes, of course it is. Does it work?: Well enough

def print_banner():
   return "<pre>\
##############################################################################\n\
#                                   --SFL--                                  #\n\
#        /¯¯¯¯\  /¯¯¯¯\ /¯¯¯¯\  /¯¯¯\_/¯¯¯\   /¯¯¯¯¯\  /¯¯¯¯\ |¯¯¯¯¯¯¯|      #\n\
#       / /¯\ | /  /\  \  /\  \| /¯\  /¯\  |  | |¯| | |  /\  | ¯¯| |¯¯       #\n\
#      / /  / |/  | |  | | |  || |  | |  | |  |  ¯ <  |  | | |   | |         #\n\
#     / /__/ / |   ¯   |  ¯   || |  | |  | |  | |¯| | |  |_| |   | |         #\n\
#    |______/   \_____/ \____/ |_|  |_|  |_|  \_____/  \____/    |_|         #\n\
#                                   MASTER                                   #\n\
##############################################################################\n\
</pre>"

def print_head(body):
   return "<html><head><title>DoomBot</title></head><body>%s\
<br /><div>Copyright Savoir-faire Linux (2012-2014)</div></body></html>" \
   % body

def print_options():
   return "<table>\n\
      <tr><td>directory </td><td><code>%s</code></td></tr>\n\
      <tr><td>script    </td><td><code>%s</code></td></tr>\n\
      <tr><td>cont      </td><td><code>%s</code></td>/tr>\n\
      <tr><td>command   </td><td><code>%s</code></td></tr>\n\
      </table>\n" % (str(config.directory), str(config.script and "true" or "false")
                   , str(config.cont and "true" or "false"), str(config.command))

def print_actions():
   return "<a href='http://127.0.0.1:5000/run/'>Run now</a>\n\
   <a href='http://127.0.0.1:5000/kill/'>Kill</a>"

@app.route("/")
def dashboard():
    return print_head(
       #Print the banner
       print_banner () +

       #Print the current options values
       print_options() +

       #Print the possible action
       print_actions()

       #Print the recent issues
    )

@app.route("/run/")
def db_run():
   print("BOB")
   process_watcher.launch_process(config)
   return "Starting the DoomBot"

@app.route("/kill/")
def db_kill():
   return "Killing the DoomBot"
