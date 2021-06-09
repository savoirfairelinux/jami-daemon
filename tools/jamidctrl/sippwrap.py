#!/usr/bin/env python
#
# Copyright (C) 2012 by the Free Software Foundation, Inc.
#
# Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import os

class SippWrapper:
    """ Wrapper that allow for managing sipp command line easily """

    def __init__(self):
        self.commandLine = "./sipp"
        self.remoteServer = ""
        self.remotePort = ""
        self.localInterface = ""
        self.localPort = ""
        self.customScenarioFile = ""
        self.isUserAgenClient = True
        self.launchInBackground = False
        self.numberOfCall = 0
        self.numberOfSimultaneousCall = 0
        self.enableTraceMsg = False
        self.enableTraceShormsg = False
        self.enableTraceScreen = False
        self.enableTraceError = False
        self.enableTraceStat = False
        self.enableTraceCounts = False
        self.enableTraceRtt = False
        self.enableTraceLogs = False

    def buildCommandLine(self, port):
	""" Fill the command line arguments based on specified parameters """

        self.localPort = str(port)

        if not self.remotePort and not self.remoteServer:
            self.isUserAgentClient = False
        elif self.remotePort and not self.remoteServer:
	    print "Error cannot have remote port specified with no server"
            return

        if self.remoteServer:
            self.commandLine += " " + self.remoteServer

        if self.remotePort:
            self.commandLine += ":" + self.remotePort

        if self.localInterface:
            self.commandLine += " -i " + self.localInterface

        if self.localPort:
            self.commandLine += " -p " + self.localPort

        if self.customScenarioFile:
            self.commandLine += " -sf " + self.customScenarioFile
        elif self.isUserAgentClient is True:
            self.commandLine += " -sn uac"
        elif self.isUserAgentClient is False:
            self.commandLine += " -sn uas"

        if self.launchInBackground:
            self.commandLine += " -bg"

        if self.numberOfCall:
            self.commandLine += " -m " + str(self.numberOfCall)

        if self.numberOfSimultaneousCall:
            self.commandLine += " -l " + str(self.numberOfSimultaneousCall)

        if self.enableTraceMsg:
            self.commandLine += " -trace_msg"

        if self.enableTraceShormsg:
            self.commandLine += " -trace_shortmsg"

        if self.enableTraceScreen:
            self.commandLine += " -trace_screen"

        if self.enableTraceError:
            self.commandLine += " -trace_err"

        if self.enableTraceStat:
            self.commandLine += " -trace_stat"

        if self.enableTraceCounts:
            self.commandLine += " -trace_counts"

        if self.enableTraceRtt:
            self.commandLine += " -trace_rtt"

        if self.enableTraceLogs:
            self.commandLine += " -trace_logs"


    def launch(self):
        """ Launch the sipp instance using the specified arguments """

        print self.commandLine
        return os.system(self.commandLine + " 2>&1 > /dev/null")


class SippScreenStatParser:
    """ Class that parse statistic reported by a sipp instance
        report some of the most important value """

    def __init__(self, filename):
        print "Opening " + filename
        self.logfile = open(filename, "r").readlines()
        print self.logfile[39]
        print self.logfile[40]

    def isAnyFailedCall(self):
        """ Look for any failed call
            Return true if there are failed call, false elsewhere """

        # TODO: Find a better way to determine which line to consider
        if "Failed call" not in self.logfile[40]:
            print "Error: Could not find 'Failed call' statistics"
            # We consider this as a failure
            return True

        return "1" in self.logfile[40]

    def isAnySuccessfulCall(self):
        """ Look for any successful call
            Return true if there are successful call, false elsewhere """

        # TODO: Find a better way to determine which line to consider
        if "Successful call" not in self.logfile[39]:
            print "Error: Could not find 'Successful call' statistics"
            return False

        return "1" in self.logfile[39]



def test_result_parsing():
    dirlist = os.listdir("./")

    logfile = [x for x in dirlist if "screen.log" in x]
    testResult = SippScreenStatParser(logfile[0])

    assert(not testResult.isAnyFailedCall())

    assert(testResult.isAnySuccessfulCall())
