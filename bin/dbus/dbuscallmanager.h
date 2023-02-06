/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
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
 */

#ifndef __RING_DBUSCALLMANAGER_H__
#define __RING_DBUSCALLMANAGER_H__

#include <vector>
#include <map>
#include <string>

#include "def.h"
#include "dbus_cpp.h"

#if __GNUC__ >= 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "dbuscallmanager.adaptor.h"
#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

#include "callmanager_interface.h"
#include <stdexcept>

class LIBJAMI_PUBLIC DBusCallManager : public cx::ring::Ring::CallManager_adaptor,
                                     public DBus::IntrospectableAdaptor,
                                     public DBus::ObjectAdaptor
{
public:
    DBusCallManager(DBus::Connection& connection);

    // Methods
    std::string placeCall(const std::string& accountId, const std::string& to);
    std::string placeCallWithMedia(const std::string& accountId,
                                   const std::string& to,
                                   const std::vector<std::map<std::string, std::string>>& mediaList);

    bool requestMediaChange(const std::string& accountId,
                            const std::string& callId,
                            const std::vector<std::map<std::string, std::string>>& mediaList);

    bool refuse(const std::string& accountId, const std::string& callId);
    bool accept(const std::string& accountId, const std::string& callId);
    bool acceptWithMedia(const std::string& accountId,
                         const std::string& callId,
                         const std::vector<std::map<std::string, std::string>>& mediaList);
    bool answerMediaChangeRequest(const std::string& accountId,
                                  const std::string& callId,
                                  const std::vector<std::map<std::string, std::string>>& mediaList);
    bool hangUp(const std::string& accountId, const std::string& callId);
    bool hold(const std::string& accountId, const std::string& callId);
    bool unhold(const std::string& accountId, const std::string& callId);
    bool muteLocalMedia(const std::string& accountId,
                        const std::string& callid,
                        const std::string& mediaType,
                        const bool& mute);
    bool transfer(const std::string& accountId, const std::string& callId, const std::string& to);
    bool attendedTransfer(const std::string& accountId,
                          const std::string& transferID,
                          const std::string& targetID);
    std::map<std::string, std::string> getCallDetails(const std::string& accountId,
                                                      const std::string& callId);
    std::vector<std::string> getCallList(const std::string& accountId);
    std::vector<std::map<std::string, std::string>> getConferenceInfos(const std::string& accountId,
                                                                       const std::string& confId);
    bool joinParticipant(const std::string& accountId,
                         const std::string& sel_callId,
                         const std::string& account2Id,
                         const std::string& drag_callId);
    void createConfFromParticipantList(const std::string& accountId,
                                       const std::vector<std::string>& participants);
    void setConferenceLayout(const std::string& accountId,
                             const std::string& confId,
                             const uint32_t& layout);
    void setActiveParticipant(const std::string& accountId,
                              const std::string& confId,
                              const std::string& callId);
    bool isConferenceParticipant(const std::string& accountId, const std::string& call_id);
    bool addParticipant(const std::string& accountId,
                        const std::string& callId,
                        const std::string& account2Id,
                        const std::string& confId);
    bool addMainParticipant(const std::string& accountId, const std::string& confId);
    bool detachLocalParticipant();
    bool detachParticipant(const std::string& accountId, const std::string& callId);
    bool joinConference(const std::string& accountId,
                        const std::string& sel_confId,
                        const std::string& account2Id,
                        const std::string& drag_confId);
    bool hangUpConference(const std::string& accountId, const std::string& confId);
    bool holdConference(const std::string& accountId, const std::string& confId);
    bool unholdConference(const std::string& accountId, const std::string& confId);
    std::vector<std::string> getConferenceList(const std::string& accountId);
    std::vector<std::string> getParticipantList(const std::string& accountId,
                                                const std::string& confId);
    std::string getConferenceId(const std::string& accountId, const std::string& callId);
    std::map<std::string, std::string> getConferenceDetails(const std::string& accountId,
                                                            const std::string& confId);
    std::vector<libjami::MediaMap> currentMediaList(const std::string& accountId,
                                                  const std::string& callId);
    bool startRecordedFilePlayback(const std::string& filepath);
    void stopRecordedFilePlayback();
    bool toggleRecording(const std::string& accountId, const std::string& callId);
    void setRecording(const std::string& accountId, const std::string& callId);
    void recordPlaybackSeek(const double& value);
    bool getIsRecording(const std::string& accountId, const std::string& callId);
    bool switchInput(const std::string& accountId,
                     const std::string& callId,
                     const std::string& input);
    bool switchSecondaryInput(const std::string& accountId,
                              const std::string& conferenceId,
                              const std::string& input);
    void playDTMF(const std::string& key);
    void startTone(const int32_t& start, const int32_t& type);
    void sendTextMessage(const std::string& accountId,
                         const std::string& callId,
                         const std::map<std::string, std::string>& messages,
                         const bool& isMixed);
    void startSmartInfo(const uint32_t& refreshTimeMs);
    void stopSmartInfo();
    void setModerator(const std::string& accountId,
                      const std::string& confId,
                      const std::string& peerId,
                      const bool& state);
    void muteStream(const std::string& accountId,
                    const std::string& confId,
                    const std::string& accountUri,
                    const std::string& deviceId,
                    const std::string& streamId,
                    const bool& state);
    void setActiveStream(const std::string& accountId,
                         const std::string& confId,
                         const std::string& accountUri,
                         const std::string& deviceId,
                         const std::string& streamId,
                         const bool& state);
    void raiseHand(const std::string& accountId,
                   const std::string& confId,
                   const std::string& accountUri,
                   const std::string& deviceId,
                   const bool& state);
    void hangupParticipant(const std::string& accountId,
                           const std::string& confId,
                           const std::string& peerId,
                           const std::string& deviceId);
    // DEPRECATED
    void muteParticipant(const std::string& accountId,
                         const std::string& confId,
                         const std::string& peerId,
                         const bool& state);
    void raiseParticipantHand(const std::string& accountId,
                              const std::string& confId,
                              const std::string& peerId,
                              const bool& state);
};

#endif // __RING_CALLMANAGER_H__
