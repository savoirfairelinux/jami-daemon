#!/usr/bin/python3
import time
import os

# This file is used to wrap the GDB instances
#
#  Doombot master server
#      |-----> process_watcher threads
#          |----> GDB process
#              |----->Doombot wrapper       <=== You are here
#                   -----> Real process
#
# This may seem a little overkill, but using the same
# process for both GDB and the Web server had too many
# limitations.

backtrace_collecton = {}
total_sig = 0
max_run = 10

class Bt_info:
   content = ""
   count = 1
   bt_hash = ""
   bt_type = ""
   def to_xml(self):
      result = ""
      result += "      <backtrace>\n"
      result += "         <signature>" + self.bt_hash+"</signature>\n"
      result += "         <type>" + self.bt_type+"</type>\n"
      result += "         <count>" + str(self.count)  +"</count>\n"
      result += "         <content>" + self.content  +"         </content>\n"
      result += "      </backtrace>\n"
      return result

#Generate output
def to_xml():
   result = ""
   result += "<doombot>\n"
   result += "  <backtraces>\n"
   for key,bt in backtrace_collecton.items():
      result += bt.to_xml()
   result += "  </backtraces>\n"
   result += "</doombot>\n"
   print(result)
   f = open('/tmp/dommbot','w')
   f.write(result)
   f.close()

def run():
   if total_sig <= max_run:
      time.sleep(4)
      gdb.execute("run 2>&1 > /dev/null")#,False,True)
   else:
      to_xml()

def get_backtrace_identity(trace):
   result = ""
   counter = 0
   for line in trace.split('\n'):
      fields =  line.split()
      if fields[3][0:2] != "__":
         result = result + fields[-1]
         counter += 1
      if counter >= 3:
         break
   return result

def get_backtrace(bt_type):
   output = gdb.execute("bt",False,True)
   bt_hash = get_backtrace_identity(output)
   if not bt_hash in backtrace_collecton:
      print("\n\n\nADDING "+bt_type+ " "+ bt_hash+" to list")
      info = Bt_info()
      info.content = output
      info.bt_hash = bt_hash
      info.bt_type = bt_type
      backtrace_collecton[bt_hash] = info
   else:
      backtrace_collecton[bt_hash].count += 1
      print("\n\n\nEXISTING " +bt_type+ " ("+  str(backtrace_collecton[bt_hash].count)+")")
   run()

def stop_handler (event):
   if isinstance(event,gdb.SignalEvent):
      global total_sig
      if event.stop_signal == "SIGSEGV":
         total_sig +=1
         get_backtrace(event.stop_signal)
      if event.stop_signal == "SIGABRT":
         print("SIGABRT")
         total_sig +=1
         get_backtrace(event.stop_signal)
   elif isinstance(event,gdb.BreakpointEvent):
      print("BREAKPOINT "+ str(event.breakpoint.expression) +" " \
         +str(event.breakpoint.location)+" "\
         +str(event.breakpoint.condition) + " " \
         +str(event.breakpoint.commands))
      for k,v in event.breakpoints:
         print("HERE "+str(k)+" "+str(v))

#Force restart
def exit_handler(event):
   if (ExitedEvent.exit_code == 0):
      gdb.run("quit")

gdb.events.stop.connect (stop_handler)
gdb.events.exited.connect (exit_handler)

gdb.execute("set confirm off")
gdb.execute("set height 0")
os.system('export MALLOC_CHECK=2')
#gdb.execute("file /home/etudiant/prefix/lib/sflphone/sflphoned")
run()