#
# Copyright (C) 2004-2023 Savoir-faire Linux Inc.
#
# Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#

import sys
import os
import time

try:
    import configparser
except ImportError as e:
    import ConfigParser as configparser
except Exception as e:
    print(str(e))
    exit(1)

from threading import Thread
from random import shuffle

ALL_TEST_NAME = {
        'TestConfig': 'testConfig',
        'LoopCallDht': 'testLoopCallDht',
        'VideoBitrateDHT': 'testLoopCallDhtWithIncBitrate',
        'SimultenousDHTCall' : 'testSimultaneousLoopCallDht',
        'DhtCallHold' : 'testLoopCallDhtWithHold'
        }

class libjamiTester():

# init to default values
    dhtAccountId = ''
    sipAccountId = ''
    dhtTestAccount = ''
    sipTestAccount = ''
    inputFile = ''
    minBitrate = 0
    maxBitrate = 0
    incBitrate = 0
    codecAudio = ''
    codecVideo = ''

    def testConfig(self, ctrl, nbIteration, delay):
        print("**[BEGIN] test config")
        allCodecs = ctrl.getAllCodecs()
        if len(allCodecs) == 0:
            print("error no codec on the system")
            return 0

        print("**[SUCCESS] test config")
        print("**[END] test config")
        return 1
#
# Helpers
#
    def getTestName(self):
        return ALL_TEST_NAME.keys()

    def checkIP2IPAccount(self, ctrl):
        ipAccount = ctrl.getAllAccounts()
        print(ipAccount)
        if len(ipAccount) == 0:
            print("no IP2IP account")
            return 0
        return 1

    def registerAccount(self, ctrl, account):
        print("registering:" + account)
        ctrl.setAccountRegistered(account, True)

    def setActiveCodecs(self, ctrl, account):
        codecList = ctrl.getAllCodecs()
        shuffle(codecList)
        codecVideoId = codecAudioId = 0
        nameCodecAudio = nameCodecVideo = ''

        if (self.codecVideo == 'H264'):
            codecVideoId = 1
            nameCodecVideo = 'H264'
        elif (self.codecVideo == 'VP8'):
            codecVideoId = 2
            nameCodecVideo = 'VP8'
        elif (self.codecVideo == 'MPEG4'):
            codecVideoId = 3
            nameCodecVideo = 'MPEG4'
        elif (self.codecVideo == 'H263'):
            codecVideoId = 4
            nameCodecVideo = 'H263'
        elif (self.codecVideo == 'RANDOM'):
            for aCodec in codecList:
                detail = ctrl.getCodecDetails(account, aCodec)
                if ( detail['CodecInfo.type'] == 'VIDEO'):
                    codecVideoId = aCodec
                    nameCodecVideo =  detail['CodecInfo.name']
                    break


        if (self.codecAudio == 'OPUS'):
            codecAudioId = 5
            nameCodecAudio = 'OPUS'
        if (self.codecAudio == 'G722'):
            codecAudioId = 6
            nameCodecAudio = 'G722'
        if (self.codecAudio == 'SPEEX32'):
            codecAudioId = 7
            nameCodecAudio = 'SPEEX32'
        elif (self.codecAudio == 'SPEEX16'):
            codecAudioId = 8
            nameCodecAudio = 'SPEEX16'
        elif (self.codecAudio == 'SPEEX8'):
            codecAudioId = 9
            nameCodecAudio = 'SPEEX8'
        elif (self.codecAudio == 'PCMA'):
            codecAudioId = 10
            nameCodecAudio = 'PCMA'
        elif (self.codecAudio == 'PCMU'):
            codecAudioId = 11
            nameCodecAudio = 'PCMU'
        elif (self.codecAudio == 'RANDOM'):
            for aCodec in codecList:
                detail = ctrl.getCodecDetails(account, aCodec)
                if ( detail['CodecInfo.type'] == 'AUDIO'):
                    codecAudioId = aCodec
                    nameCodecAudio = detail['CodecInfo.name']
                    break

        if (codecAudioId == 0 or codecVideoId == 0):
            print ("please configure at least one A/V codec !")
            return

        print ("selecting codecs audio= "+nameCodecAudio + " video= "+nameCodecVideo)
        ctrl.setActiveCodecList(account, (str(codecVideoId)+","+ str(codecAudioId)))

    def holdToggle(self, ctrl, callId, delay):
        time.sleep(delay)
        print("Holding: " + callId)
        ctrl.Hold(callId)
        time.sleep(delay)
        print("UnHolding: " + callId)
        ctrl.UnHold(callId)

#
# tests
#

# testLoopCallDht
# perform <nbIteration> DHT calls using <delay> between each call

    def testLoopCallDht(self, ctrl, nbIteration, delay):
        print("**[BEGIN] DHT Call Test")

        count = 0
        currBitrate = self.minBitrate
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setActiveCodecs(ctrl, self.dhtAccountId)
            print("setting video bitrate to "+str(currBitrate))
            ctrl.setVideoCodecBitrate(self.dhtAccountId, currBitrate)

            callId = ctrl.Call(self.dhtTestAccount)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.inputFile)

            time.sleep(delay)

            ctrl.HangUp(callId)
            count += 1

        print("**[SUCCESS] DHT Call Test")
        print("**[END] DHT Call Test")

# testLoopCallDhtWithHold
# perform <nbIteration> DHT calls using <delay> between each call
# perform stress hold/unhold between each call

    def testLoopCallDhtWithHold(self, ctrl, nbIteration, delay):
        print("**[BEGIN] DHT Call Test With Hold")

        count = 0
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setActiveCodecs(ctrl, self.dhtAccountId)

            callId = ctrl.Call(self.dhtTestAccount)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.inputFile)

            delayHold = 5
            nbHold = delay / (delayHold * 2)
            countHold = 0

            while countHold < nbHold:
                self.holdToggle(ctrl, callId, delayHold)
                countHold = countHold + 1


            ctrl.HangUp(callId)
            count += 1

        print("**[SUCCESS] DHT Call Test With Hold")
        print("**[END] DHT Call Test With Hold")


# testLoopCallDhtWithIncBitrate
# perform <nbIteration> DHT calls using <delay> between each call
# inc bitrate between each iteration

    def testLoopCallDhtWithIncBitrate(self, ctrl, nbIteration, delay):
        print("**[BEGIN] VIDEO Bitrate Test")

        count = 0
        currBitrate = self.minBitrate

        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setActiveCodecs(ctrl, self.dhtAccountId)
            print("setting video bitrate to "+str(currBitrate))
            ctrl.setVideoCodecBitrate(self.dhtAccountId, currBitrate)

            callId = ctrl.Call(self.dhtTestAccount)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.inputFile)

            time.sleep(delay)

            ctrl.HangUp(callId)
            count += 1

            currBitrate += self.incBitrate
            if (currBitrate > self.maxBitrate):
                currBitrate = self.minBitrate

        print("**[SUCCESS] VIDEO Bitrate Test")
        print("**[END] VIDEO Bitrate Test")

# testSimultaneousLoopCallDht
# perform <nbIteration> simultaneous DHT calls using <delay> between each call

    def testSimultaneousLoopCallDht(self, ctrl, nbIteration, delay):
        print("**[BEGIN] Simultaneous DHT call test")
        count = 0
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))
            self.setActiveCodecs(ctrl, self.dhtAccountId)

            # do all the calls
            currCall = 0
            NB_SIMULTANEOUS_CALL = 10;
            while currCall <= NB_SIMULTANEOUS_CALL:
                ctrl.Call(self.dhtTestAccount)
                time.sleep(1)
                currCall = currCall + 1

                time.sleep(delay)

            # hangup each call
            for callId in ctrl.getAllCalls():
                ctrl.HangUp(callId)

            count += 1

        print("**[SUCCESS] Simultaneous DHT call test")
        print("**[END] Simultaneous DHT call test")




# Main function

    def start(self, ctrl, testName):

        if not testName in ALL_TEST_NAME:
            print(("wrong test name"))
            return

        #getConfig
        self.dhtAccountId = ctrl.getAllAccounts('RING')[0]
        self.sipAccountId = ctrl.getAllAccounts('SIP')[0]

        config = configparser.ConfigParser()
        config.read('test_config.ini')
        self.dhtTestAccount = str(config['dht']['testAccount'])
        self.sipTestAccount = str(config['sip']['testAccount'])
        self.inputFile = str(config['codec']['inputFile'])
        self.minBitrate = int(config['codec']['minBitrate'])
        self.maxBitrate = int(config['codec']['maxBitrate'])
        self.incBitrate = int(config['codec']['incBitrate'])
        self.codecAudio = str(config['codec']['codecAudio'])
        self.codecVideo = str(config['codec']['codecVideo'])

        testNbIteration = config['iteration']['nbIteration']
        delay = config['iteration']['delay']

        getattr(self, ALL_TEST_NAME[testName])(ctrl,int(testNbIteration), int(delay))
