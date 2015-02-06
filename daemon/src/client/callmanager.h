/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __RING_CALLMANAGER_H__
#define __RING_CALLMANAGER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdexcept>
#include <map>
#include <vector>
#include <string>

#include "callmanager_interface.h"

namespace ring {

struct CallManager {
  #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  ::DRing::call_ev_handlers evHandlers_;
  #pragma GCC diagnostic warning "-Wmissing-field-initializers"
};

extern CallManager callManager;

void callStateChanged(const std::string& callID, const std::string& state);

void transferFailed();
void transferSucceeded();

void recordPlaybackStopped(const std::string& path);

void voiceMailNotify(const std::string& callID, int32_t nd_msg);

void onIncomingMessage(const std::string& ID, const std::string& from, const std::string& msg);
void onIncomingCall(const std::string& accountID, const std::string& callID, const std::string& from);

void recordPlaybackFilepath(const std::string& id, const std::string& filename);

void conferenceCreated(const std::string& confID);
void conferenceChanged(const std::string& confID,const std::string& state);

void updatePlaybackScale(const std::string&, int32_t, int32_t);
void conferenceRemoved(const std::string&);
void newCallCreated(const std::string&, const std::string&, const std::string&);
void sipCallStateChanged(const std::string&, const std::string&, int32_t);
void recordingStateChanged(const std::string& callID, bool state);
void secureSdesOn(const std::string& arg);
void secureSdesOff(const std::string& arg);

void secureZrtpOn(const std::string& callID, const std::string& cipher);
void secureZrtpOff(const std::string& callID);
void showSAS(const std::string& callID, const std::string& sas, bool verified);
void zrtpNotSuppOther(const std::string& callID);
void zrtpNegotiationFailed(const std::string& callID, const std::string& arg2, const std::string& arg3);

void onRtcpReportReceived(const std::string& callID, const std::map<std::string, int>& stats);
} // namespace ring

#endif//CALLMANAGER_H
