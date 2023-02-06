/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* File : jni_interface.i */
%module (directors="1") JamiService

#define SWIG_JAVA_ATTACH_CURRENT_THREAD_AS_DAEMON
%include "typemaps.i"
%include "std_string.i" /* std::string typemaps */
%include "enums.swg"
%include "arrays_java.i";
%include "carrays.i";
%include "std_map.i";
%include "std_vector.i";
%include "stdint.i";
%include "data_view.i";
%header %{

#include <android/log.h>

%}

/* void* shall be handled as byte arrays */
%typemap(jni) void * "void *"
%typemap(jtype) void * "byte[]"
%typemap(jstype) void * "byte[]"
%typemap(javain) void * "$javainput"
%typemap(in) void * %{
    $1 = (void*)$input;
%}
%typemap(javadirectorin) void * "$jniinput"
%typemap(out) void * %{
    $result = $1;
%}
%typemap(javaout) void * {
    return $jnicall;
}

/* Convert unsigned char and thus uint8_t to jbyte */
%typemap(jni) unsigned char,      const unsigned char &      "jbyte"
%typemap(jtype) unsigned char,      const unsigned char &      "byte"
%typemap(jstype) unsigned char,      const unsigned char &      "byte"
%typemap(jboxtype) unsigned char,      const unsigned char &      "Byte"
%typemap(directorin, descriptor="B") unsigned char    "$input = (jbyte) $1;"
%typemap(out) unsigned char  %{ $result = (jbyte)$1; %}
%typemap(out) const unsigned char &  %{ $result = (jbyte)*$1; %}

%typecheck(SWIG_TYPECHECK_INT8) /* Java byte */
    jbyte,
    signed char,
    const signed char &,
    unsigned char,
    const unsigned char
    ""

%typecheck(SWIG_TYPECHECK_INT16) /* Java short */
    jshort,
    short,
    const short &
    ""

/* Maps exceptions */
%typemap(throws, throws="java.lang.IllegalArgumentException") std::invalid_argument {
  jclass excep = jenv->FindClass("java/lang/IllegalArgumentException");
  if (excep)
    jenv->ThrowNew(excep, $1.what());
  return $null;
}
%typemap(throws, throws="java.lang.IllegalStateException") std::runtime_error {
  jclass excep = jenv->FindClass("java/lang/IllegalStateException");
  if (excep)
    jenv->ThrowNew(excep, $1.what());
  return $null;
}

/* Avoid uint64_t to be converted to BigInteger */
%apply int64_t { uint64_t };
%apply int64_t { const uint64_t };
%apply int64_t { std::streamsize };
%apply uint64_t { time_t };

namespace std {

%typemap(javacode) map<string, string> %{
  public static $javaclassname toSwig(java.util.Map<String,String> in) {
    $javaclassname n = new $javaclassname();
    for (java.util.Map.Entry<String, String> entry : in.entrySet()) {
      if (entry.getValue() != null) {
        n.put(entry.getKey(), entry.getValue());
      }
    }
    return n;
  }

  public java.util.HashMap<String,String> toNative() {
    java.util.HashMap<String,String> out = new java.util.HashMap<>((int)size());
    for (Entry<String, String> e : entrySet())
        out.put(e.getKey(), e.getValue());
    return out;
  }

  public java.util.HashMap<String,String> toNativeFromUtf8() {
      java.util.HashMap<String,String> out = new java.util.HashMap<>((int)size());
      for (String s : keys()) {
          try {
              out.put(s, new String(getRaw(s), "utf-8"));
          } catch (java.io.UnsupportedEncodingException e) {
          }
      }
      return out;
  }
  public void setUnicode(String key, String value) {
    setRaw(key, Blob.bytesFromString(value));
  }
%}

%extend map<string, string> {
    std::vector<std::string> keys() const {
        std::vector<std::string> k;
        k.reserve($self->size());
        for (const auto& i : *$self) {
            k.emplace_back(i.first);
        }
        return k;
    }
    void setRaw(const std::string& key, jami::String value) {
        (*$self)[key] = std::move(value.str);
    }
    jami::DataView getRaw(const std::string& key) {
        const auto& v = $self->at(key);
        return {(const uint8_t*)v.data(), v.size()};
    }
}

%template(StringMap) map<string, string>;
%template(StringVect) vector<string>;

%typemap(javacode) vector< map<string,string> > %{
  public java.util.ArrayList<java.util.Map<String, String>> toNative() {
    int size = size();
    java.util.ArrayList<java.util.Map<String, String>> out = new java.util.ArrayList<>(size);
    for (int i = 0; i < size; ++i) {
        out.add(get(i).toNative());
    }
    return out;
  }
%}
%template(VectMap) vector< map<string,string> >;
%template(IntegerMap) map<string,int>;
%template(IntVect) vector<int32_t>;
%template(UintVect) vector<uint32_t>;

%typemap(javacode) vector<uint8_t> %{
  public static byte[] bytesFromString(String in) {
    try {
      return in.getBytes("UTF-8");
    } catch (java.io.UnsupportedEncodingException e) {
      return in.getBytes();
    }
  }
  public static Blob fromString(String in) {
    Blob n = new Blob();
    n.setBytes(bytesFromString(in));
    return n;
  }
%}

%extend vector<uint8_t> {
    jami::DataView getBytes() {
      return {self->data(), self->size()};
    }
    void setBytes(jami::Data data) {
      *self = std::move(data.data);
    }
}
%template(Blob) vector<uint8_t>;
%template(FloatVect) vector<float>;
}

/* not parsed by SWIG but needed by generated C files */
%header %{

#include <functional>

%}

/* parsed by SWIG to generate all the glue */
/* %include "../managerimpl.h" */
/* %include <client/callmanager.h> */

%include "managerimpl.i"
%include "callmanager.i"
%include "configurationmanager.i"
%include "datatransfer.i"
%include "presencemanager.i"
%include "videomanager.i"
%include "plugin_manager_interface.i"
%include "conversation.i"

#include "jami/callmanager_interface.h"

%inline %{
/* some functions that need to be declared in *_wrap.cpp
 * that are not declared elsewhere in the c++ code
 */

void init(ConfigurationCallback* confM, Callback* callM, PresenceCallback* presM, DataTransferCallback* dataM, VideoCallback* videoM, ConversationCallback* convM) {
    using namespace std::placeholders;

    using std::bind;
    using libjami::exportable_callback;
    using libjami::CallSignal;
    using libjami::ConfigurationSignal;
    using libjami::DataTransferInfo;
    using libjami::DataTransferSignal;
    using libjami::PresenceSignal;
    using libjami::VideoSignal;
    using libjami::ConversationSignal;

    using SharedCallback = std::shared_ptr<libjami::CallbackWrapperBase>;

    // Call event handlers
    const std::map<std::string, SharedCallback> callEvHandlers = {
        exportable_callback<CallSignal::StateChange>(bind(&Callback::callStateChanged, callM, _1, _2, _3, _4)),
        exportable_callback<CallSignal::TransferFailed>(bind(&Callback::transferFailed, callM)),
        exportable_callback<CallSignal::TransferSucceeded>(bind(&Callback::transferSucceeded, callM)),
        exportable_callback<CallSignal::RecordPlaybackStopped>(bind(&Callback::recordPlaybackStopped, callM, _1)),
        exportable_callback<CallSignal::VoiceMailNotify>(bind(&Callback::voiceMailNotify, callM, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingMessage>(bind(&Callback::incomingMessage, callM, _1, _2, _3, _4)),
        exportable_callback<CallSignal::IncomingCall>(bind(&Callback::incomingCall, callM, _1, _2, _3)),
        exportable_callback<CallSignal::IncomingCallWithMedia>(bind(&Callback::incomingCallWithMedia, callM, _1, _2, _3, _4)),
        exportable_callback<CallSignal::MediaChangeRequested>(bind(&Callback::mediaChangeRequested, callM, _1, _2, _3)),
        exportable_callback<CallSignal::RecordPlaybackFilepath>(bind(&Callback::recordPlaybackFilepath, callM, _1, _2)),
        exportable_callback<CallSignal::ConferenceCreated>(bind(&Callback::conferenceCreated, callM, _1, _2)),
        exportable_callback<CallSignal::ConferenceChanged>(bind(&Callback::conferenceChanged, callM, _1, _2, _3)),
        exportable_callback<CallSignal::ConferenceRemoved>(bind(&Callback::conferenceRemoved, callM, _1, _2)),
        exportable_callback<CallSignal::UpdatePlaybackScale>(bind(&Callback::updatePlaybackScale, callM, _1, _2, _3)),
        exportable_callback<CallSignal::RecordingStateChanged>(bind(&Callback::recordingStateChanged, callM, _1, _2)),
        exportable_callback<CallSignal::RtcpReportReceived>(bind(&Callback::onRtcpReportReceived, callM, _1, _2)),
        exportable_callback<CallSignal::OnConferenceInfosUpdated>(bind(&Callback::onConferenceInfosUpdated, callM, _1, _2)),
        exportable_callback<CallSignal::PeerHold>(bind(&Callback::peerHold, callM, _1, _2)),
        exportable_callback<CallSignal::AudioMuted>(bind(&Callback::audioMuted, callM, _1, _2)),
        exportable_callback<CallSignal::VideoMuted>(bind(&Callback::videoMuted, callM, _1, _2)),
        exportable_callback<CallSignal::ConnectionUpdate>(bind(&Callback::connectionUpdate, callM, _1, _2)),
        exportable_callback<CallSignal::RemoteRecordingChanged>(bind(&Callback::remoteRecordingChanged, callM, _1, _2, _3)),
        exportable_callback<CallSignal::MediaNegotiationStatus>(bind(&Callback::mediaNegotiationStatus, callM, _1, _2, _3))
    };

    // Configuration event handlers
    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::VolumeChanged>(bind(&ConfigurationCallback::volumeChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&ConfigurationCallback::accountsChanged, confM)),
        exportable_callback<ConfigurationSignal::StunStatusFailed>(bind(&ConfigurationCallback::stunStatusFailure, confM, _1)),
        exportable_callback<ConfigurationSignal::AccountDetailsChanged>(bind(&ConfigurationCallback::accountDetailsChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&ConfigurationCallback::registrationStateChanged, confM, _1, _2, _3, _4)),
        exportable_callback<ConfigurationSignal::VolatileDetailsChanged>(bind(&ConfigurationCallback::volatileAccountDetailsChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::KnownDevicesChanged>(bind(&ConfigurationCallback::knownDevicesChanged, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::ExportOnRingEnded>(bind(&ConfigurationCallback::exportOnRingEnded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::Error>(bind(&ConfigurationCallback::errorAlert, confM, _1)),
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>(bind(&ConfigurationCallback::incomingAccountMessage, confM, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::AccountMessageStatusChanged>(bind(&ConfigurationCallback::accountMessageStatusChanged, confM, _1, _2, _3, _4, _5 )),
        exportable_callback<ConfigurationSignal::NeedsHost>(bind(&ConfigurationCallback::needsHost, confM, _1, _2 )),
        exportable_callback<ConfigurationSignal::ActiveCallsChanged>(bind(&ConfigurationCallback::activeCallsChanged, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ProfileReceived>(bind(&ConfigurationCallback::profileReceived, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ComposingStatusChanged>(bind(&ConfigurationCallback::composingStatusChanged, confM, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::IncomingTrustRequest>(bind(&ConfigurationCallback::incomingTrustRequest, confM, _1, _2, _3, _4, _5 )),
        exportable_callback<ConfigurationSignal::ContactAdded>(bind(&ConfigurationCallback::contactAdded, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::ContactRemoved>(bind(&ConfigurationCallback::contactRemoved, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::CertificatePinned>(bind(&ConfigurationCallback::certificatePinned, confM, _1 )),
        exportable_callback<ConfigurationSignal::CertificatePathPinned>(bind(&ConfigurationCallback::certificatePathPinned, confM, _1, _2 )),
        exportable_callback<ConfigurationSignal::CertificateExpired>(bind(&ConfigurationCallback::certificateExpired, confM, _1 )),
        exportable_callback<ConfigurationSignal::CertificateStateChanged>(bind(&ConfigurationCallback::certificateStateChanged, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::GetHardwareAudioFormat>(bind(&ConfigurationCallback::getHardwareAudioFormat, confM, _1 )),
        exportable_callback<ConfigurationSignal::GetAppDataPath>(bind(&ConfigurationCallback::getAppDataPath, confM, _1, _2 )),
        exportable_callback<ConfigurationSignal::GetDeviceName>(bind(&ConfigurationCallback::getDeviceName, confM, _1 )),
        exportable_callback<ConfigurationSignal::RegisteredNameFound>(bind(&ConfigurationCallback::registeredNameFound, confM, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::NameRegistrationEnded>(bind(&ConfigurationCallback::nameRegistrationEnded, confM, _1, _2, _3 )),
        exportable_callback<ConfigurationSignal::UserSearchEnded>(bind(&ConfigurationCallback::userSearchEnded, confM, _1, _2, _3, _4 )),
        exportable_callback<ConfigurationSignal::MigrationEnded>(bind(&ConfigurationCallback::migrationEnded, confM, _1, _2)),
        exportable_callback<ConfigurationSignal::DeviceRevocationEnded>(bind(&ConfigurationCallback::deviceRevocationEnded, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::AccountProfileReceived>(bind(&ConfigurationCallback::accountProfileReceived, confM, _1, _2, _3)),
        exportable_callback<ConfigurationSignal::MessageSend>(bind(&ConfigurationCallback::messageSend, confM, _1))
    };

    // Presence event handlers
    const std::map<std::string, SharedCallback> presenceEvHandlers = {
        exportable_callback<PresenceSignal::NewServerSubscriptionRequest>(bind(&PresenceCallback::newServerSubscriptionRequest, presM, _1 )),
        exportable_callback<PresenceSignal::ServerError>(bind(&PresenceCallback::serverError, presM, _1, _2, _3 )),
        exportable_callback<PresenceSignal::NewBuddyNotification>(bind(&PresenceCallback::newBuddyNotification, presM, _1, _2, _3, _4 )),
        exportable_callback<PresenceSignal::NearbyPeerNotification>(bind(&PresenceCallback::nearbyPeerNotification, presM, _1, _2, _3, _4)),
        exportable_callback<PresenceSignal::SubscriptionStateChanged>(bind(&PresenceCallback::subscriptionStateChanged, presM, _1, _2, _3 ))
    };

    const std::map<std::string, SharedCallback> dataTransferEvHandlers = {
        exportable_callback<DataTransferSignal::DataTransferEvent>(bind(&DataTransferCallback::dataTransferEvent, dataM, _1, _2, _3, _4, _5)),
    };

    const std::map<std::string, SharedCallback> videoEvHandlers = {
        exportable_callback<VideoSignal::GetCameraInfo>(bind(&VideoCallback::getCameraInfo, videoM, _1, _2, _3, _4)),
        exportable_callback<VideoSignal::SetParameters>(bind(&VideoCallback::setParameters, videoM, _1, _2, _3, _4, _5)),
        exportable_callback<VideoSignal::SetBitrate>(bind(&VideoCallback::setBitrate, videoM, _1, _2)),
        exportable_callback<VideoSignal::RequestKeyFrame>(bind(&VideoCallback::requestKeyFrame, videoM, _1)),
        exportable_callback<VideoSignal::StartCapture>(bind(&VideoCallback::startCapture, videoM, _1)),
        exportable_callback<VideoSignal::StopCapture>(bind(&VideoCallback::stopCapture, videoM, _1)),
        exportable_callback<VideoSignal::DecodingStarted>(bind(&VideoCallback::decodingStarted, videoM, _1, _2, _3, _4, _5)),
        exportable_callback<VideoSignal::DecodingStopped>(bind(&VideoCallback::decodingStopped, videoM, _1, _2, _3)),
    };

    const std::map<std::string, SharedCallback> conversationHandlers = {
        exportable_callback<ConversationSignal::ConversationLoaded>(bind(&ConversationCallback::conversationLoaded, convM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::MessagesFound>(bind(&ConversationCallback::messagesFound, convM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::MessageReceived>(bind(&ConversationCallback::messageReceived, convM, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationProfileUpdated>(bind(&ConversationCallback::conversationProfileUpdated, convM, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestReceived>(bind(&ConversationCallback::conversationRequestReceived, convM, _1, _2, _3)),
        exportable_callback<ConversationSignal::ConversationRequestDeclined>(bind(&ConversationCallback::conversationRequestDeclined, convM, _1, _2)),
        exportable_callback<ConversationSignal::ConversationReady>(bind(&ConversationCallback::conversationReady, convM, _1, _2)),
        exportable_callback<ConversationSignal::ConversationRemoved>(bind(&ConversationCallback::conversationRemoved, convM, _1, _2)),
        exportable_callback<ConversationSignal::ConversationMemberEvent>(bind(&ConversationCallback::conversationMemberEvent, convM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::OnConversationError>(bind(&ConversationCallback::onConversationError, convM, _1, _2, _3, _4)),
        exportable_callback<ConversationSignal::ConversationPreferencesUpdated>(bind(&ConversationCallback::conversationPreferencesUpdated, convM, _1, _2, _3))
    };

    if (!libjami::init(static_cast<libjami::InitFlag>(libjami::LIBJAMI_FLAG_DEBUG)))
        return;

    registerSignalHandlers(callEvHandlers);
    registerSignalHandlers(configEvHandlers);
    registerSignalHandlers(presenceEvHandlers);
    registerSignalHandlers(dataTransferEvHandlers);
    registerSignalHandlers(videoEvHandlers);
    registerSignalHandlers(conversationHandlers);

    libjami::start();
}


%}
#ifndef SWIG
/* some bad declarations */
#endif
