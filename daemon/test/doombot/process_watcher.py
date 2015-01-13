import threading
import subprocess
import os

# This file is used to wrap the GDB instances
#
#  Doombot master server
#      |-----> process_watcher threads     <=== You are here
#          |----> GDB process
#              |----->Doombot wrapper
#                   -----> Real process
#
# This may seem a little overkill, but using the same
# process for both GDB and the Web server had too many
# limitations.

def launch_process(config):
   """
   Runs the given args in a subprocess.Popen, and then calls the function
   onExit when the subprocess completes.
   onExit is a callable object, and popenArgs is a list/tuple of args that
   would give to subprocess.Popen.
   """
   def runInThread(onExit, popenArgs):
      print(popenArgs)
      #print("starting "+popenArgs.executable)
      proc = subprocess.Popen(**popenArgs)
      proc.wait()
      #output = proc.communicate()[0]
      #error  = proc.communicate()[2]
      onExit(output,error)
      return

   t = { 'args'   : ['gdb', '-x', os.path.dirname(os.path.realpath(__file__)) + "/gdb_wrapper.py" ,'--args'] + config.args}#,
         #'stdout' : subprocess.PIPE ,
         #'stderr' : subprocess.PIPE }
   print("foo")
   def onExit():
      return
   thread = threading.Thread(target=runInThread, args=(onExit, t) )
   thread.start()

   return 4 #session_id TODO
