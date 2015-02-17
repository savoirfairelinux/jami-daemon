/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "signal.h"

namespace ring {

SignalHandlerMap& getSignalHandlers()
{
    static SignalHandlerMap handlers = {
        exported_callback<DRing::CallSignal::StateChange>(),
        exported_callback<DRing::CallSignal::TransferFailed>(),
        exported_callback<DRing::CallSignal::TransferSucceeded>(),
        exported_callback<DRing::CallSignal::RecordPlaybackStopped>(),
        exported_callback<DRing::CallSignal::VoiceMailNotify>(),
        exported_callback<DRing::CallSignal::IncomingMessage>(),
        exported_callback<DRing::CallSignal::IncomingCall>(),
        exported_callback<DRing::CallSignal::RecordPlaybackFilepath>(),
        exported_callback<DRing::CallSignal::ConferenceCreated>(),
        exported_callback<DRing::CallSignal::ConferenceChanged>(),
        exported_callback<DRing::CallSignal::UpdatePlaybackScale>(),
        exported_callback<DRing::CallSignal::ConferenceRemoved>(),
        exported_callback<DRing::CallSignal::NewCallCreated>(),
        exported_callback<DRing::CallSignal::SipCallStateChanged>(),
        exported_callback<DRing::CallSignal::RecordingStateChanged>(),
        exported_callback<DRing::CallSignal::SecureSdesOn>(),
        exported_callback<DRing::CallSignal::SecureSdesOff>(),
        exported_callback<DRing::CallSignal::SecureZrtpOn>(),
        exported_callback<DRing::CallSignal::SecureZrtpOff>(),
        exported_callback<DRing::CallSignal::ShowSAS>(),
        exported_callback<DRing::CallSignal::ZrtpNotSuppOther>(),
        exported_callback<DRing::CallSignal::ZrtpNegotiationFailed>(),
        exported_callback<DRing::CallSignal::RtcpReportReceived>(),
    };
    return handlers;
}

}; // namespace ring
