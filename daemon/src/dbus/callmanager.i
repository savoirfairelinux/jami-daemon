/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
 *          Alexandre Lision <alexnadre.L@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

%header %{

#include <android-jni/callmanagerJNI.h>


typedef struct callmanager_callback
{
    void (*on_new_call_created)(const std::string& accountID,
                                const std::string& callID,
                                const std::string& to);

    void (*on_call_state_changed)(const std::string& callID,
                                  const std::string& state);

    void (*on_incoming_call)(const std::string& accountID,
                             const std::string& callID,
                             const std::string& from);

    void (*on_transfer_state_changed) (const std::string& result);

    void (*on_conference_created) (const std::string& confID);

    void (*on_conference_removed) (const std::string& confID);

    void (*on_conference_state_changed) (const std::string& confID,
                                          const std::string& state);

    void (*on_incoming_message) (const std::string& ID,
                                    const std::string& from,
                                    const std::string& msg);
} callmanager_callback_t;


class Callback {
public:
    virtual ~Callback() {}

    virtual void on_new_call_created(const std::string& arg1,
                                     const std::string& arg2,
                                     const std::string& arg3) {}

    virtual void on_call_state_changed(const std::string& arg1,
                                       const std::string& arg2) {}

    virtual void on_incoming_call(const std::string& arg1,
                                  const std::string& arg2,
                                  const std::string& arg3) {}

    virtual void on_transfer_state_changed (const std::string& arg1) {}

    virtual void on_conference_created (const std::string& arg1) {}

    virtual void on_conference_removed (const std::string& arg1) {}

    virtual void on_conference_state_changed (const std::string& arg1,
                                            const std::string& arg2) {}

    virtual void on_incoming_message(const std::string& ID,
                                    const std::string& from,
                                    const std::string& msg) {}
};


static Callback* registeredCallbackObject = NULL;

void on_new_call_created_wrapper (const std::string& accountID,
                                  const std::string& callID,
                                  const std::string& to) {
    registeredCallbackObject->on_new_call_created(accountID, callID, to);
}

void on_call_state_changed_wrapper(const std::string& callID,
                           const std::string& state) {
    registeredCallbackObject->on_call_state_changed(callID, state);
}

void on_incoming_call_wrapper (const std::string& accountID,
                               const std::string& callID,
                               const std::string& from) {
    registeredCallbackObject->on_incoming_call(accountID, callID, from);
}

void on_transfer_state_changed_wrapper (const std::string& result) {
    registeredCallbackObject->on_transfer_state_changed(result);
}

void on_conference_created_wrapper (const std::string& confID) {
    registeredCallbackObject->on_conference_created(confID);
}

void on_conference_removed_wrapper (const std::string& confID) {
    registeredCallbackObject->on_conference_removed(confID);
}

void on_conference_state_changed_wrapper (const std::string& confID,
                                          const std::string& state) {
    registeredCallbackObject->on_conference_state_changed(confID, state);
}

void on_incoming_message_wrapper(const std::string& ID, const std::string& from, const std::string& msg) {
  registeredCallbackObject->on_incoming_message(ID, from, msg);
}

static struct callmanager_callback wrapper_callback_struct = {
    &on_new_call_created_wrapper,
    &on_call_state_changed_wrapper,
    &on_incoming_call_wrapper,
    &on_transfer_state_changed_wrapper,
    &on_conference_created_wrapper,
    &on_conference_removed_wrapper,
    &on_conference_state_changed_wrapper,
    &on_incoming_message_wrapper,
};

void setCallbackObject(Callback* callback) {
    registeredCallbackObject = callback;
}

%}

%feature("director") Callback;

class CallManagerJNI {
public:
    /* Manager::instance().outgoingCall */
    void placeCall(const std::string& accountID,
                   const std::string& callID,
                   const std::string& to);
    /* Manager::instance().refuseCall */
    void refuse(const std::string& callID);
    /* Manager::instance().answerCall */
    void accept(const std::string& callID);
    /* Manager::instance().hangupCall */
    void hangUp(const std::string& callID);
    void hold(const std::string& callID);
    void unhold(const std::string& callID);
    bool transfer(const std::string& callID, const std::string& to);
    bool attendedTransfer(const std::string& transferID, const std::string& targetID);

    void setRecordingCall(const std::string& id);

    bool sendTextMessage(const std::string& callID, const std::string& message, const std::string& from);
    
     /* Conference related methods */

    void removeConference(const std::string& conference_id);
    void joinParticipant(const std::string& sel_callID, const std::string& drag_callID);
    void createConfFromParticipantList(const std::vector< std::string >& participants);
    void createConference(const std::string& id1, const std::string& id2);
    void addParticipant(const std::string& callID, const std::string& confID);
    std::vector<std::string> getParticipantList(const std::string& confID);
    void addMainParticipant(const std::string& confID);
    void detachParticipant(const std::string& callID);
    void joinConference(const std::string& sel_confID, const std::string& drag_confID);
    void hangUpConference(const std::string& confID);
    void holdConference(const std::string& confID);
    void unholdConference(const std::string& confID);
    std::vector<std::string> getConferenceList();
    std::vector<std::string> getCallList();
    std::vector<std::string> getParticipantList(const std::string& confID);
    std::string getConferenceId(const std::string& callID);
    std::map<std::string, std::string> getConferenceDetails(const std::string& callID);

};

class Callback {
public:
    virtual ~Callback();

    virtual void on_new_call_created(const std::string& arg1,
                                     const std::string& arg2,
                                     const std::string& arg3);

    virtual void on_call_state_changed(const std::string& arg1,
                                       const std::string& arg2);

    virtual void on_incoming_call(const std::string& arg1,
                                  const std::string& arg2,
                                  const std::string& arg3);

    virtual void on_transfer_state_changed(const std::string& arg1);

    virtual void on_conference_created(const std::string& arg1);

    virtual void on_conference_removed(const std::string& arg1);

    virtual void on_conference_state_changed(const std::string& arg1,
                                              const std::string& arg2);

    virtual void on_incoming_message(const std::string& ID,
                                    const std::string& from,
                                    const std::string& msg);
};
 
static Callback* registeredCallbackObject = NULL;

void setCallbackObject(Callback* callback) {
    registeredCallbackObject = callback;
}

