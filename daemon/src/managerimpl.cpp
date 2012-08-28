/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logger.h"
#include "managerimpl.h"
#include "account_schema.h"

#include "dbus/callmanager.h"
#include "global.h"
#include "fileutils.h"
#include "sip/sipvoiplink.h"
#include "sip/sipaccount.h"
#include "sip/sipcall.h"

#ifndef ANDROID
#include "im/instant_messaging.h"
#endif

#if HAVE_IAX
#include "iax/iaxaccount.h"
#include "iax/iaxcall.h"
#include "iax/iaxvoiplink.h"
#endif


#include "numbercleaner.h"
#include "config/yamlparser.h"
#include "config/yamlemitter.h"

#ifndef ANDROID
#include "audio/alsa/alsalayer.h"
#endif

#include "audio/sound/tonelist.h"
#include "audio/sound/audiofile.h"
#include "audio/sound/dtmf.h"
#include "history/historynamecache.h"
#include "history/history.h"
#include "manager.h"
#include "dbus/configurationmanager.h"

#ifdef SFL_VIDEO
#include "dbus/video_controls.h"
#endif

#include "conference.h"

#include <cerrno>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <tr1/functional>
#include <iterator>
#include <fstream>
#include <sstream>
#include <sys/types.h> // mkdir(2)
#include <sys/stat.h>  // mkdir(2)
#include "com_savoirfairelinux_sflphone_client_ManagerImpl.h"
#include <android/native_activity.h>

static JavaVM *gJavaVM;
static jobject gManagerObject, gDataObject;
const char *kManagerPath = "com/savoirfairelinux/sflphone/client/ManagerImpl";
const char *kDataPath = "com/savoirfairelinux/sflphone/client/Data";

ManagerImpl::ManagerImpl() :
    preferences(), voipPreferences(), addressbookPreference(),
    hookPreference(),  audioPreference(), shortcutPreferences(),
    hasTriedToRegister_(false), audioCodecFactory(),
#if HAVE_DBUS
	dbus_(),
#endif
	config_(),
    currentCallId_(), currentCallMutex_(), audiodriver_(0), dtmfKey_(),
    toneMutex_(), telephoneTone_(), audiofile_(), audioLayerMutex_(),
    waitingCall_(), waitingCallMutex_(), nbIncomingWaitingCall_(0), path_(),
    callAccountMap_(), callAccountMapMutex_(), IPToIPMap_(),
    mainBuffer_(), conferenceMap_(), history_(), finished_(false)
{
    // initialize random generator for call id
    srand(time(NULL));
}

static std::string getAppPath() {
	JNIEnv *env;
	int status;
	bool isAttached = false;
	std::string path;

	status = gJavaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
	if (status < 0) {
		WARN("getAppPath: failed to get JNI environment, assuming native thread");
		status = gJavaVM->AttachCurrentThread(&env, NULL);
		if (status < 0) {
			ERROR("getAppPath: failed to attach current thread");
			return;
		}
		isAttached = true;
	}

	jclass managerImplClass = env->GetObjectClass(gManagerObject);
	if (!managerImplClass) {
		ERROR("getAppPath: failed to get class reference");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return;
	}

	/* Find the callBack method ID */
	jmethodID method = env->GetStaticMethodID(managerImplClass, "getAppPath", "()Ljava/lang/String;");
	if (!method) {
		ERROR("getAppPath: failed to get callBack method ID");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return;
	}

	jstring jstr = env->CallStaticObjectMethod(managerImplClass, method);
	if (isAttached) {
		gJavaVM->DetachCurrentThread();
		isAttached = false;
	}
    path = env->GetStringUTFChars(jstr, NULL);
	return path;
}

static void callback_handler(char *s) {
	int status;
	JNIEnv *env;
	bool isAttached = false;

	//INFO("callback_handler");

	// FIXME
	status = gJavaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
	if (status < 0) {
		WARN("callback_handler: failed to get JNI environment, assuming native thread");
		status = gJavaVM->AttachCurrentThread(&env, NULL);
		if (status < 0) {
			ERROR("callback_handler: failed to attach current thread");
			return;
		}
		isAttached = true;
	}

	/* construct a java struct */
	jstring js = env->NewStringUTF(s);
	jclass managerImplClass = env->GetObjectClass(gManagerObject);
	if (!managerImplClass) {
		ERROR("callback_handler: failed to get class reference");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return;
	}

	/* Find the callBack method ID */
	jmethodID method = env->GetStaticMethodID(managerImplClass, "callBack", "(Ljava/lang/String;)V");
	if (!method) {
		ERROR("callback_handler: failed to get callBack method ID");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return;
	}

	env->CallStaticVoidMethod(managerImplClass, method, js);
	if (isAttached) {
		gJavaVM->DetachCurrentThread();
		isAttached = false;
	}
}

void *native_thread_start(void *arg) {
	sleep(1);
	INFO("native_thread");
	callback_handler((char *) "Called from native thread");
}

JNIEXPORT void JNICALL Java_com_savoirfairelinux_sflphone_client_ManagerImpl_callVoid
  (JNIEnv *env, jclass cls) {
	pthread_t native_thread;

	INFO("callVoid");

	callback_handler((char *) "Called from Java thread");
	if (pthread_create(&native_thread, NULL, native_thread_start, NULL)) {
		ERROR("callVoid: failed to create a native thread");
	}
}

JNIEXPORT jobject JNICALL Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getNewData
(JNIEnv *env, jclass cls, jint i, jstring s) {
	INFO("getNewData");

	jclass dataCachedClass = env->GetObjectClass(gDataObject);
	if(!dataCachedClass) {
		ERROR("getNewData: failed to get cached Data Class");
		return  NULL;
	}
	INFO("getNewData: cached Data Class found");

	jmethodID dataConstructor = env->GetMethodID(dataCachedClass, "<init>", "(ILjava/lang/String;)V");
	if (!dataConstructor) {
		ERROR("getNewData: failed to get constructor");
	}
	INFO("getNewData: Data constructor found");

	jobject dataObject = env->NewObject(dataCachedClass, dataConstructor, i, s);
	if (!dataObject) {
		ERROR("getNewData: failed to create object");
		return NULL;
	}
	return dataObject;
}

/*
 * Class: org_wooyd_android_JNIExample_JNIExampleInterface
 * Method: getDataString
 * Signature: (Lcom/savoirfairelinux/sflphone/client/Data;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getDataString
(JNIEnv *env, jclass cls, jobject dataObject) {
	INFO("getDataString");

	jclass dataCachedClass = env->GetObjectClass(gDataObject);
	if(!dataCachedClass) {
		ERROR("getDataString: failed to get class reference");
		return NULL;
	}
	INFO("getDataString: ObjectClass Data found");

	jfieldID dataStringField = env->GetFieldID(dataCachedClass, "s", "Ljava/lang/String;");
	if(!dataStringField) {
		ERROR("getDataString: failed to get field ID");
		return NULL;
	}
	INFO("getDataString: FieldID s found");

	jstring dataStringValue = (jstring) env->GetObjectField(dataObject, dataStringField);
	return dataStringValue;
}

void initClassHelper(JNIEnv *env, const char *path, jobject *objptr) {
    jclass cls;

	INFO("initClassHelper");

	cls= env->FindClass(path);
	if(!cls) {
        ERROR("initClassHelper: failed to get %s class reference", path);
        return;
    }
    jmethodID constr = env->GetMethodID(cls, "<init>", "()V");
	INFO("initClassHelper: %s method found", path);

    if(!constr) {
        ERROR("initClassHelper: failed to get %s constructor", path);
        return;
    }
    jobject obj = env->NewObject(cls, constr);
	INFO("initClassHelper: %s constructor found", path);

    if(!obj) {
        ERROR("initClassHelper: failed to create a %s object", path);
        return;
    }
	/* protect cached object instances from Android GC */
    (*objptr) = env->NewGlobalRef(obj);
	INFO("initClassHelper: object found %x", objptr);
}

void deinitClassHelper(JNIEnv *env, jobject obj) {
	INFO("deinitClassHelper");

	/* delete cached object instances */
    env->DeleteGlobalRef(obj);
	INFO("deinitClassHelper: object %x deleted", obj);
}

JNINativeMethod methods[] =
{
	/*
	 * name,
	 * signature,
	 * funcPtr
	 */
	{
		"initN",
		"(Ljava/lang/String;)V",
		(void *) Java_com_savoirfairelinux_sflphone_client_ManagerImpl_initN
	},
	{
		"callVoid",
		"()V",
		(void *) Java_com_savoirfairelinux_sflphone_client_ManagerImpl_callVoid
	},
	{
		"getNewData",
		"(ILjava/lang/String;)Lcom/savoirfairelinux/sflphone/client/Data;",
		(void *) Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getNewData
	},
	{
		"getDataString",
		"(Lcom/savoirfairelinux/sflphone/client/Data;)Ljava/lang/String;",
		(void *) Java_com_savoirfairelinux_sflphone_client_ManagerImpl_getDataString
	}
};

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
	jclass clazz;
	int numMethods = sizeof(methods) / sizeof(methods[0]);

	INFO("JNI_OnLoad");

	/* get env */
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
		ERROR("JNI_OnLoad: failed to get the environment using GetEnv()");
        return -1;
    }
	INFO("JNI_Onload: GetEnv %p", env);

	/* cache VM object as it won't change in near future */
	gJavaVM = vm;
	INFO("JNI_Onload: JavaVM %p", gJavaVM);

    /* Get jclass with env->FindClass */
	clazz = env->FindClass(kManagerPath);
	if (!clazz) {
        ERROR("JNI_Onload: whoops, %s class not found!", kManagerPath);
	}

	/* put instances of class object we need into cache */
    initClassHelper(env, kManagerPath, &gManagerObject);
    initClassHelper(env, kDataPath, &gDataObject);

	if(env->RegisterNatives(clazz, methods, numMethods) != JNI_OK)
	{
		ERROR("JNI_Onload: Failed to register native methods");
		return -1;
	}

	INFO("JNI_Onload: Native functions registered");

    return JNI_VERSION_1_6;
}

void JNI_OnUnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
	jclass clazz;

	INFO("JNI_OnUnLoad");

	/* get env */
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
		ERROR("JNI_OnUnLoad: failed to get the environment using GetEnv()");
        return;
    }
	INFO("JNI_OnUnLoad: GetEnv %p", env);

    /* Get jclass with env->FindClass */
	clazz = env->FindClass(kManagerPath);
	if (!clazz) {
        ERROR("JNI_OnUnLoad: whoops, %s class not found!", kManagerPath);
	}

	/* put instances of class object we need into cache */
    deinitClassHelper(env, gManagerObject);
    deinitClassHelper(env, gDataObject);

	env->UnregisterNatives(clazz);
	INFO("JNI_OnUnLoad: Native functions unregistered");
}

static int ManagerImpl::getSipLogLevel() {
	JNIEnv *env;
	int status;
	bool isAttached = false;

	status = gJavaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
	if (status < 0) {
		WARN("getSipLogLevel: failed to get JNI environment, assuming native thread");
		status = gJavaVM->AttachCurrentThread(&env, NULL);
		if (status < 0) {
			ERROR("getSipLogLevel: failed to attach current thread");
			return 6;
		}
		isAttached = true;
	}

	jclass managerImplClass = env->GetObjectClass(gManagerObject);
	if (!managerImplClass) {
		ERROR("getSipLogLevel: failed to get class reference");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return 6;
	}

	/* Find the callBack method ID */
	jmethodID method = env->GetStaticMethodID(managerImplClass, "getSipLogLevel", "()I");
	if (!method) {
		ERROR("getSipLogLevel: failed to get callBack method ID");
		if(isAttached)
			gJavaVM->DetachCurrentThread();
		return 6;
	}

	int level = (int) env->CallStaticIntMethod(managerImplClass, method);
	if (isAttached) {
		gJavaVM->DetachCurrentThread();
		isAttached = false;
	}
	return level;
}

void ManagerImpl::init(const std::string &config_file)
{
    path_ = config_file.empty() ? retrieveConfigPath() : config_file;
    DEBUG("Configuration file path: %s", path_.c_str());

    try {
        FILE *file = fopen(path_.c_str(), "rb");

        if (file) {
            Conf::YamlParser parser(file);
            parser.serializeEvents();
            parser.composeEvents();
            parser.constructNativeData();
            loadAccountMap(parser);
            fclose(file);
        } else {
            WARN("Config file not found: creating default account map");
            loadDefaultAccountMap();
        }
    } catch (const Conf::YamlParserException &e) {
        ERROR("%s", e.what());
    }

    initAudioDriver();

    {
        ost::MutexLock lock(audioLayerMutex_);
        if (audiodriver_) {
            telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), audiodriver_->getSampleRate()));
            dtmfKey_.reset(new DTMF(getMainBuffer().getInternalSamplingRate()));
        }
    }

    history_.load(preferences.getHistoryLimit());
    registerAccounts();
}

JNIEXPORT void JNICALL Java_com_savoirfairelinux_sflphone_client_ManagerImpl_initN
  (JNIEnv *jenv, jclass obj, jstring jconfig_file)
{
	const std::string config_file;
    jmethodID getAppPath;
	jobject appPath;
	std::string str;
	int status;
	JNIEnv *env;
	bool isAttached = false;

	DEBUG("initN");

	// FIXME
	status = gJavaVM->GetEnv((void **) &env, JNI_VERSION_1_6);
	if (status < 0) {
		WARN("initN: failed to get JNI environment, assuming native thread");
		status = gJavaVM->AttachCurrentThread(&env, NULL);
		if (status < 0) {
			ERROR("initN: failed to attach current thread");
			return;
		}
		isAttached = true;
	}

	config_file = std::string(env->GetStringUTFChars(jconfig_file, 0));
	/* here we go: 20 lines of code to simply call a java method... JNI sucks... */
	jclass managerImplClass = env->GetObjectClass(gManagerObject);
	if (!managerImplClass) {
		ERROR("initN: failed to get ManagerImpl class");
		return;
	}

	/* get the getAppPath method defined in java */
	getAppPath = env->GetStaticMethodID(managerImplClass, "getAppPath", "()Ljava/lang/String;");
	if (!getAppPath) {
        ERROR("initN: whoops, getAppPath method not found!");
		return;
	}
	DEBUG("initN: getAppPath method found");

	/* call getAppPath method */
	appPath = env->CallStaticObjectMethod(obj, getAppPath);
	if (!appPath) {
        ERROR("initN: whoops, getAppPath cannot be called!");
		return;
	}
	DEBUG("initN: getAppPath returned");

	/* detach current thread */
	if (isAttached) {
		gJavaVM->DetachCurrentThread();
		isAttached = false;
	}

	/* convert it to c++ string */
	str = env->GetStringUTFChars((jstring) appPath, NULL);
	if (str.empty()) {
		ERROR("initN: whoops, appPath is empty!");
		return;
	}
	INFO("initN: Application path: %s", str.c_str());

	DEBUG("initN: creating manager");
	DEBUG("initN: setting application path");
	Manager::instance().setPath(str);
	DEBUG("initN: initializing manager");
	Manager::instance().init(config_file);

	INFO("initN: End");
	return;
}

void ManagerImpl::setPath(const std::string &path) {
	history_.setPath(path);
}

#if HAVE_DBUS
void ManagerImpl::run()
{
    DEBUG("Starting DBus event loop");
    dbus_.exec();
}
#endif

void ManagerImpl::finish()
{
    if (finished_)
        return;

    finished_ = true;
    // Unset signal handlers
    signal(SIGHUP, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    std::vector<std::string> callList(getCallList());
    DEBUG("Hangup %zu remaining call(s)", callList.size());

    for (std::vector<std::string>::iterator iter = callList.begin();
            iter != callList.end(); ++iter)
        hangupCall(*iter);

    saveConfig();

    unregisterAllAccounts();

    SIPVoIPLink::destroy();
#if HAVE_IAX
    IAXVoIPLink::unloadAccountMap();
#endif

    {
        ost::MutexLock lock(audioLayerMutex_);

        delete audiodriver_;
        audiodriver_ = NULL;
    }

#if HAVE_DBUS
    dbus_.exit();
#endif
}

bool ManagerImpl::isCurrentCall(const std::string& callId) const
{
    return currentCallId_ == callId;
}

bool ManagerImpl::hasCurrentCall() const
{
    return not currentCallId_.empty();
}

std::string
ManagerImpl::getCurrentCallId() const
{
    return currentCallId_;
}

void ManagerImpl::unsetCurrentCall()
{
    switchCall("");
}

void ManagerImpl::switchCall(const std::string& id)
{
    ost::MutexLock m(currentCallMutex_);
    DEBUG("----- Switch current call id to %s -----", id.c_str());
    currentCallId_ = id;
}

///////////////////////////////////////////////////////////////////////////////
// Management of events' IP-phone user
///////////////////////////////////////////////////////////////////////////////
/* Main Thread */

bool ManagerImpl::outgoingCall(const std::string& account_id,
                               const std::string& call_id,
                               const std::string& to,
                               const std::string& conf_id)
{
    if (call_id.empty()) {
        DEBUG("New outgoing call abort, missing callid");
        return false;
    }

    // Call ID must be unique
    if (not getAccountFromCall(call_id).empty()) {
        ERROR("Call id already exists in outgoing call");
        return false;
    }

    DEBUG("New outgoing call %s to %s", call_id.c_str(), to.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    std::string prefix(hookPreference.getNumberAddPrefix());

    std::string to_cleaned(NumberCleaner::clean(to, prefix));

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {
        DEBUG("Has current call (%s) put it onhold", current_call_id.c_str());

        // if this is not a conference and this and is not a conference participant
        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id))
            onHoldCall(current_call_id);
        else if (isConference(current_call_id) and not isConferenceParticipant(call_id))
            detachParticipant(MainBuffer::DEFAULT_ID, current_call_id);
    }

    DEBUG("Selecting account %s", account_id.c_str());

    // fallback using the default sip account if the specied doesn't exist
    std::string use_account_id = "";
    if (!accountExists(account_id)) {
        WARN("Account does not exist, trying with default SIP account");
        use_account_id = SIPAccount::IP2IP_PROFILE;
    }
    else {
        use_account_id = account_id;
    }

    associateCallToAccount(call_id, use_account_id);

    try {
        Call *call = getAccountLink(account_id)->newOutgoingCall(call_id, to_cleaned);

        // try to reverse match the peer name using the cache
        if (call->getDisplayName().empty()) {
            const std::string pseudo_contact_name(HistoryNameCache::getInstance().getNameFromHistory(call->getPeerNumber(), getAccountFromCall(call_id)));
            if (not pseudo_contact_name.empty())
                call->setDisplayName(pseudo_contact_name);
        }
        switchCall(call_id);
        call->setConfId(conf_id);
    } catch (const VoipLinkException &e) {
        callFailure(call_id);
        ERROR("%s", e.what());
        return false;
    } catch (ost::Socket *) {
        callFailure(call_id);
        ERROR("Could not bind socket");
        return false;
    }

    getMainBuffer().dumpInfo();

    return true;
}

//THREAD=Main : for outgoing Call
bool ManagerImpl::answerCall(const std::string& call_id)
{
    DEBUG("Answer call %s", call_id.c_str());

    // If sflphone is ringing
    stopTone();

    // set playback mode to VOICE
    AudioLayer *al = getAudioDriver();
    if(al) al->setPlaybackMode(AudioLayer::VOICE);

    // store the current call id
    std::string current_call_id(getCurrentCallId());

    Call *call = getCallFromCallID(call_id);

    if (call == NULL)
        ERROR("Call is NULL");

    // in any cases we have to detach from current communication
    if (hasCurrentCall()) {

        DEBUG("Currently conversing with %s", current_call_id.c_str());

        if (not isConference(current_call_id) and not isConferenceParticipant(current_call_id)) {
            DEBUG("Answer call: Put the current call (%s) on hold", current_call_id.c_str());
            onHoldCall(current_call_id);
        } else if (isConference(current_call_id) and not isConferenceParticipant(call_id)) {
            // if we are talking to a conference and we are answering an incoming call
            DEBUG("Detach main participant from conference");
            detachParticipant(MainBuffer::DEFAULT_ID, current_call_id);
        }
    }

    try {
        const std::string account_id = getAccountFromCall(call_id);
        getAccountLink(account_id)->answer(call);
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
    }

    // if it was waiting, it's waiting no more
    removeWaitingCall(call_id);

    // if we dragged this call into a conference already
    if (isConferenceParticipant(call_id))
        switchCall(call->getConfId());
    else
        switchCall(call_id);

    // Connect streams
    addStream(call_id);

    getMainBuffer().dumpInfo();

    // Start recording if set in preference
    if (audioPreference.getIsAlwaysRecording())
        setRecordingCall(call_id);

#if HAVE_DBUS
    // update call state on client side
    dbus_.getCallManager()->callStateChanged(call_id, "CURRENT");
#endif

    return true;
}

//THREAD=Main
void ManagerImpl::hangupCall(const std::string& callId)
{
    DEBUG("Hangup call %s", callId.c_str());

    // store the current call id
    std::string currentCallId(getCurrentCallId());

    stopTone();

    // set playback mode to NONE
    AudioLayer *al = getAudioDriver();
    if(al) al->setPlaybackMode(AudioLayer::NONE);

#if HAVE_DBUS
    /* Broadcast a signal over DBus */
    DEBUG("Send DBUS call state change (HUNGUP) for id %s", callId.c_str());
    dbus_.getCallManager()->callStateChanged(callId, "HUNGUP");
#endif

    if (not isValidCall(callId) and not isIPToIP(callId)) {
        ERROR("Could not hang up call, call not valid");
        return;
    }

    // Disconnect streams
    removeStream(callId);

    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else {
        // we are not participating in a conference, current call switched to ""
        if (not isConference(currentCallId))
            unsetCurrentCall();
    }

    if (isIPToIP(callId)) {
        /* Direct IP to IP call */
        try {
            Call * call = SIPVoIPLink::instance()->getSipCall(callId);
            if (call) {
                history_.addCall(call, preferences.getHistoryLimit());
                SIPVoIPLink::instance()->hangup(callId);
                saveHistory();
            }
        } catch (const VoipLinkException &e) {
            ERROR("%s", e.what());
        }
    } else {
        std::string accountId(getAccountFromCall(callId));
        Call * call = getCallFromCallID(callId);
        if (call) {
            history_.addCall(call, preferences.getHistoryLimit());
            VoIPLink *link = getAccountLink(accountId);
            link->hangup(callId);
            removeCallAccount(callId);
            saveHistory();
        }
    }

    getMainBuffer().dumpInfo();
}

bool ManagerImpl::hangupConference(const std::string& id)
{
    DEBUG("Hangup conference %s", id.c_str());

    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end()) {
        Conference *conf = iter_conf->second;

        if (conf) {
            ParticipantSet participants(conf->getParticipantList());

            for (ParticipantSet::const_iterator iter = participants.begin();
                    iter != participants.end(); ++iter)
                hangupCall(*iter);
        } else {
            ERROR("No such conference %s", id.c_str());
            return false;
        }
    }

    unsetCurrentCall();

    getMainBuffer().dumpInfo();

    return true;
}


//THREAD=Main
void ManagerImpl::onHoldCall(const std::string& callId)
{
    DEBUG("Put call %s on hold", callId.c_str());

    stopTone();

    std::string current_call_id(getCurrentCallId());

    try {
        if (isIPToIP(callId)) {
            SIPVoIPLink::instance()->onhold(callId);
        } else {
            /* Classic call, attached to an account */
            std::string account_id(getAccountFromCall(callId));

            if (account_id.empty()) {
                DEBUG("Account ID %s or callid %s doesn't exist in call onHold", account_id.c_str(), callId.c_str());
                return;
            }

            getAccountLink(account_id)->onhold(callId);
        }
    } catch (const VoipLinkException &e) {
        ERROR("%s", e.what());
    }

    // Unbind calls in main buffer
    removeStream(callId);

    // Remove call from teh queue if it was still there
    removeWaitingCall(callId);

    // keeps current call id if the action is not holding this call or a new outgoing call
    // this could happen in case of a conference
    if (current_call_id == callId)
        unsetCurrentCall();

#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(callId, "HOLD");
#endif

    getMainBuffer().dumpInfo();
}

//THREAD=Main
void ManagerImpl::offHoldCall(const std::string& callId)
{
    std::string codecName;

    DEBUG("Put call %s off hold", callId.c_str());

    stopTone();

    std::string currentCallId(getCurrentCallId());

    //Place current call on hold if it isn't
    if (hasCurrentCall()) {
        if (not currentCallId.empty() and not isConference(currentCallId) and not isConferenceParticipant(currentCallId)) {
            DEBUG("Has current call (%s), put on hold", currentCallId.c_str());
            onHoldCall(currentCallId);
        } else if (isConference(currentCallId) && callId != currentCallId) {
            holdConference(currentCallId);
        } else if (isConference(currentCallId) and not isConferenceParticipant(callId))
            detachParticipant(MainBuffer::DEFAULT_ID, currentCallId);
    }

    if (isIPToIP(callId))
        SIPVoIPLink::instance()->offhold(callId);
    else {
        /* Classic call, attached to an account */
        const std::string accountId(getAccountFromCall(callId));
        DEBUG("Setting offhold, Account %s, callid %s", accountId.c_str(), callId.c_str());
        Call * call = getCallFromCallID(callId);

        if (call)
            getAccountLink(accountId)->offhold(callId);
    }

#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(callId, "UNHOLD");
#endif

    if (isConferenceParticipant(callId)) {
        Call *call = getCallFromCallID(callId);
        if (call)
            switchCall(call->getConfId());

    } else
        switchCall(callId);

    addStream(callId);

    getMainBuffer().dumpInfo();
}

//THREAD=Main
bool ManagerImpl::transferCall(const std::string& callId, const std::string& to)
{
    if (isConferenceParticipant(callId)) {
        removeParticipant(callId);
    } else if (not isConference(getCurrentCallId()))
        unsetCurrentCall();

    // Direct IP to IP call
    if (isIPToIP(callId)) {
        SIPVoIPLink::instance()->transfer(callId, to);
    } else {
        std::string accountID(getAccountFromCall(callId));

        if (accountID.empty())
            return false;

        VoIPLink *link = getAccountLink(accountID);
        link->transfer(callId, to);
    }

    // remove waiting call in case we make transfer without even answer
    removeWaitingCall(callId);

    getMainBuffer().dumpInfo();

    return true;
}

void ManagerImpl::transferFailed()
{
#if HAVE_DBUS
    dbus_.getCallManager()->transferFailed();
#endif
}

void ManagerImpl::transferSucceeded()
{
#if HAVE_DBUS
    dbus_.getCallManager()->transferSucceeded();
#endif
}

bool ManagerImpl::attendedTransfer(const std::string& transferID, const std::string& targetID)
{
    if (isIPToIP(transferID))
        return SIPVoIPLink::instance()->attendedTransfer(transferID, targetID);

    // Classic call, attached to an account
    std::string accountid(getAccountFromCall(transferID));

    if (accountid.empty())
        return false;

    return getAccountLink(accountid)->attendedTransfer(transferID, targetID);
}

//THREAD=Main : Call:Incoming
void ManagerImpl::refuseCall(const std::string& id)
{
    stopTone();

    if (getCallList().size() <= 1) {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    /* Direct IP to IP call */

    if (isIPToIP(id))
        SIPVoIPLink::instance()->refuse(id);
    else {
        /* Classic call, attached to an account */
        std::string accountid = getAccountFromCall(id);

        if (accountid.empty())
            return;

        getAccountLink(accountid)->refuse(id);

        removeCallAccount(id);
    }

    removeWaitingCall(id);
#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(id, "HUNGUP");
#endif

    // Disconnect streams
    removeStream(id);

    getMainBuffer().dumpInfo();
}

Conference*
ManagerImpl::createConference(const std::string& id1, const std::string& id2)
{
    DEBUG("Create conference with call %s and %s", id1.c_str(), id2.c_str());

    Conference* conf = new Conference;

    conf->add(id1);
    conf->add(id2);

    // Add conference to map
    conferenceMap_.insert(std::make_pair(conf->getConfID(), conf));

#if HAVE_DBUS
    // broadcast a signal over dbus
    dbus_.getCallManager()->conferenceCreated(conf->getConfID());
#endif

    return conf;
}

void ManagerImpl::removeConference(const std::string& conference_id)
{
    DEBUG("Remove conference %s", conference_id.c_str());
    DEBUG("number of participants: %u", conferenceMap_.size());
    ConferenceMap::iterator iter = conferenceMap_.find(conference_id);

    Conference* conf = 0;

    if (iter != conferenceMap_.end())
        conf = iter->second;

    if (conf == 0) {
        ERROR("Conference not found");
        return;
    }

#if HAVE_DBUS
    // broadcast a signal over dbus
    dbus_.getCallManager()->conferenceRemoved(conference_id);
#endif

    // We now need to bind the audio to the remain participant

    // Unbind main participant audio from conference
    getMainBuffer().unBindAll(MainBuffer::DEFAULT_ID);

    ParticipantSet participants(conf->getParticipantList());

    // bind main participant audio to remaining conference call
    ParticipantSet::iterator iter_p = participants.begin();

    if (iter_p != participants.end())
        getMainBuffer().bindCallID(*iter_p, MainBuffer::DEFAULT_ID);

    // Then remove the conference from the conference map
    if (conferenceMap_.erase(conference_id))
        DEBUG("Conference %s removed successfully", conference_id.c_str());
    else
        ERROR("Cannot remove conference: %s", conference_id.c_str());

    delete conf;
}

Conference*
ManagerImpl::getConferenceFromCallID(const std::string& call_id)
{
    Call *call = getCallFromCallID(call_id);
    if (!call)
        return NULL;

    ConferenceMap::const_iterator iter(conferenceMap_.find(call->getConfId()));

    if (iter != conferenceMap_.end())
        return iter->second;
    else
        return NULL;
}

void ManagerImpl::holdConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf == conferenceMap_.end())
        return;

    Conference *conf = iter_conf->second;

    bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                 conf->getState() == Conference::ACTIVE_DETACHED_REC or
                 conf->getState() == Conference::HOLD_REC;

    ParticipantSet participants(conf->getParticipantList());

    for (ParticipantSet::const_iterator iter = participants.begin();
            iter != participants.end(); ++iter) {
        switchCall(*iter);
        onHoldCall(*iter);
    }

    conf->setState(isRec ? Conference::HOLD_REC : Conference::HOLD);
#if HAVE_DBUS
    dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
#endif
}

void ManagerImpl::unHoldConference(const std::string& id)
{
    ConferenceMap::iterator iter_conf = conferenceMap_.find(id);

    if (iter_conf != conferenceMap_.end() and iter_conf->second) {
        Conference *conf = iter_conf->second;

        bool isRec = conf->getState() == Conference::ACTIVE_ATTACHED_REC or
                     conf->getState() == Conference::ACTIVE_DETACHED_REC or
                     conf->getState() == Conference::HOLD_REC;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter = participants.begin(); iter!= participants.end(); ++iter) {
            Call *call = getCallFromCallID(*iter);
            if (call) {
                // if one call is currently recording, the conference is in state recording
                isRec |= call->isRecording();
                offHoldCall(*iter);
            }
        }

        conf->setState(isRec ? Conference::ACTIVE_ATTACHED_REC : Conference::ACTIVE_ATTACHED);
#if HAVE_DBUS
        dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
#endif
    }
}

bool ManagerImpl::isConference(const std::string& id) const
{
    return conferenceMap_.find(id) != conferenceMap_.end();
}

bool ManagerImpl::isConferenceParticipant(const std::string& call_id)
{
    Call *call(getCallFromCallID(call_id));
    return call and not call->getConfId().empty();
}

void ManagerImpl::addParticipant(const std::string& callId, const std::string& conferenceId)
{
    DEBUG("Add participant %s to %s", callId.c_str(), conferenceId.c_str());
    ConferenceMap::iterator iter = conferenceMap_.find(conferenceId);

    if (iter == conferenceMap_.end()) {
        ERROR("Conference id is not valid");
        return;
    }

    Call *call = getCallFromCallID(callId);
    if (call == NULL) {
        ERROR("Call id %s is not valid", callId.c_str());
        return;
    }

    // store the current call id (it will change in offHoldCall or in answerCall)
    std::string current_call_id(getCurrentCallId());

    // detach from prior communication and switch to this conference
    if (current_call_id != callId) {
        if (isConference(current_call_id))
            detachParticipant(MainBuffer::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id);
    }

    // TODO: remove this ugly hack => There should be different calls when double clicking
    // a conference to add main participant to it, or (in this case) adding a participant
    // toconference
    unsetCurrentCall();

    // Add main participant
    addMainParticipant(conferenceId);

    Conference* conf = iter->second;
    switchCall(conf->getConfID());

    // Add coresponding IDs in conf and call
    call->setConfId(conf->getConfID());
    conf->add(callId);

    // Connect new audio streams together
    getMainBuffer().unBindAll(callId);

    std::map<std::string, std::string> callDetails(getCallDetails(callId));
    std::string callState(callDetails.find("CALL_STATE")->second);

    if (callState == "HOLD") {
        conf->bindParticipant(callId);
        offHoldCall(callId);
    } else if (callState == "INCOMING") {
        conf->bindParticipant(callId);
        answerCall(callId);
    } else if (callState == "CURRENT")
        conf->bindParticipant(callId);

    ParticipantSet participants(conf->getParticipantList());

    if (participants.empty())
        ERROR("Participant list is empty for this conference");

    // reset ring buffer for all conference participant
    // flush conference participants only
    for (ParticipantSet::const_iterator p = participants.begin();
            p != participants.end(); ++p)
        getMainBuffer().flush(*p);

    getMainBuffer().flush(MainBuffer::DEFAULT_ID);

    // Connect stream
    addStream(callId);
}

void ManagerImpl::addMainParticipant(const std::string& conference_id)
{
    if (hasCurrentCall()) {
        std::string current_call_id(getCurrentCallId());

        if (isConference(current_call_id))
            detachParticipant(MainBuffer::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id);
    }

    {
        ost::MutexLock lock(audioLayerMutex_);

        ConferenceMap::const_iterator iter = conferenceMap_.find(conference_id);

        if (iter != conferenceMap_.end()) {
            Conference *conf = iter->second;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {
            getMainBuffer().bindCallID(*iter_p, MainBuffer::DEFAULT_ID);
            // Reset ringbuffer's readpointers
            getMainBuffer().flush(*iter_p);
        }

        getMainBuffer().flush(MainBuffer::DEFAULT_ID);

        if (conf->getState() == Conference::ACTIVE_DETACHED)
            conf->setState(Conference::ACTIVE_ATTACHED);
        else if (conf->getState() == Conference::ACTIVE_DETACHED_REC)
            conf->setState(Conference::ACTIVE_ATTACHED_REC);
        else
            WARN("Invalid conference state while adding main participant");

#if HAVE_DBUS
        dbus_.getCallManager()->conferenceChanged(conference_id, conf->getStateStr());
#endif
        }
    }

    switchCall(conference_id);
}

Call *
ManagerImpl::getCallFromCallID(const std::string &callID)
{
    Call *call = NULL;

    call = SIPVoIPLink::instance()->getSipCall(callID);
#if HAVE_IAX
    if(call != NULL)
        return call;

    call = IAXVoIPLink::getIaxCall(callID);
#endif

    return call;
}

void ManagerImpl::joinParticipant(const std::string& callId1, const std::string& callId2)
{
    DEBUG("Join participants %s, %s", callId1.c_str(), callId2.c_str());
    if (callId1 == callId2) {
        ERROR("Cannot join participant %s to itself", callId1.c_str());
        return;
    }

    // Set corresponding conference ids for call 1
    Call *call1 = getCallFromCallID(callId1);

    if (call1 == NULL) {
        ERROR("Could not find call %s", callId1.c_str());
        return;
    }

    // Set corresponding conderence details
    Call *call2 = getCallFromCallID(callId2);

    if (call2 == NULL) {
        ERROR("Could not find call %s", callId2.c_str());
        return;
    }

    std::map<std::string, std::string> call1Details(getCallDetails(callId1));
    std::map<std::string, std::string> call2Details(getCallDetails(callId2));

    std::string current_call_id(getCurrentCallId());
    DEBUG("Current Call ID %s", current_call_id.c_str());

    // detach from the conference and switch to this conference
    if ((current_call_id != callId1) and (current_call_id != callId2)) {
        // If currently in a conference
        if (isConference(current_call_id))
            detachParticipant(MainBuffer::DEFAULT_ID, current_call_id);
        else
            onHoldCall(current_call_id); // currently in a call
    }


    Conference *conf = createConference(callId1, callId2);

    call1->setConfId(conf->getConfID());
    getMainBuffer().unBindAll(callId1);

    call2->setConfId(conf->getConfID());
    getMainBuffer().unBindAll(callId2);

    // Process call1 according to its state
    std::string call1_state_str(call1Details.find("CALL_STATE")->second);
    DEBUG("Process call %s state: %s", callId1.c_str(), call1_state_str.c_str());

    if (call1_state_str == "HOLD") {
        conf->bindParticipant(callId1);
        offHoldCall(callId1);
    } else if (call1_state_str == "INCOMING") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else if (call1_state_str == "CURRENT") {
        conf->bindParticipant(callId1);
    } else if (call1_state_str == "INACTIVE") {
        conf->bindParticipant(callId1);
        answerCall(callId1);
    } else
        WARN("Call state not recognized");

    // Process call2 according to its state
    std::string call2_state_str(call2Details.find("CALL_STATE")->second);
    DEBUG("Process call %s state: %s", callId2.c_str(), call2_state_str.c_str());

    if (call2_state_str == "HOLD") {
        conf->bindParticipant(callId2);
        offHoldCall(callId2);
    } else if (call2_state_str == "INCOMING") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else if (call2_state_str == "CURRENT") {
        conf->bindParticipant(callId2);
    } else if (call2_state_str == "INACTIVE") {
        conf->bindParticipant(callId2);
        answerCall(callId2);
    } else
        WARN("Call state not recognized");

    // Switch current call id to this conference
    switchCall(conf->getConfID());
    conf->setState(Conference::ACTIVE_ATTACHED);

    // set recording sampling rate
    {
        ost::MutexLock lock(audioLayerMutex_);
        if (audiodriver_)
            conf->setRecordingSmplRate(audiodriver_->getSampleRate());
    }

    getMainBuffer().dumpInfo();
}

void ManagerImpl::createConfFromParticipantList(const std::vector< std::string > &participantList)
{
    // we must at least have 2 participant for a conference
    if (participantList.size() <= 1) {
        ERROR("Participant number must be higher or equal to 2");
        return;
    }

    Conference *conf = new Conference;

    int successCounter = 0;

    for (std::vector<std::string>::const_iterator p = participantList.begin();
         p != participantList.end(); ++p) {
        std::string numberaccount(*p);
        std::string tostr(numberaccount.substr(0, numberaccount.find(",")));
        std::string account(numberaccount.substr(numberaccount.find(",") + 1, numberaccount.size()));

        std::string generatedCallID(getNewCallID());

        // Manager methods may behave differently if the call id participates in a conference
        conf->add(generatedCallID);

        unsetCurrentCall();

        // Create call
        bool callSuccess = outgoingCall(account, generatedCallID, tostr, conf->getConfID());

        // If not able to create call remove this participant from the conference
        if (!callSuccess)
            conf->remove(generatedCallID);
        else {
#if HAVE_DBUS
            dbus_.getCallManager()->newCallCreated(account, generatedCallID, tostr);
#endif
            successCounter++;
        }
    }

    // Create the conference if and only if at least 2 calls have been successfully created
    if (successCounter >= 2) {
        conferenceMap_[conf->getConfID()] = conf;
#if HAVE_DBUS
        dbus_.getCallManager()->conferenceCreated(conf->getConfID());
#endif

        {
            ost::MutexLock lock(audioLayerMutex_);

            if (audiodriver_)
                conf->setRecordingSmplRate(audiodriver_->getSampleRate());
        }

        getMainBuffer().dumpInfo();
    } else {
        delete conf;
    }
}

void ManagerImpl::detachParticipant(const std::string& call_id,
                                    const std::string& current_id)
{
    DEBUG("Detach participant %s (current id: %s)", call_id.c_str(),
           current_id.c_str());
    std::string current_call_id(getCurrentCallId());

    if (call_id != MainBuffer::DEFAULT_ID) {
        Call *call = getCallFromCallID(call_id);

        if (call == NULL) {
            ERROR("Could not find call %s", call_id.c_str());
            return;
        }

        Conference *conf = getConferenceFromCallID(call_id);

        if (conf == NULL) {
            ERROR("Call is not conferencing, cannot detach");
            return;
        }

        std::map<std::string, std::string> call_details(getCallDetails(call_id));
        std::map<std::string, std::string>::iterator iter_details(call_details.find("CALL_STATE"));

        if (iter_details == call_details.end()) {
            ERROR("Could not find CALL_STATE");
            return;
        }

        if (iter_details->second == "RINGING") {
            // You've dragged a ringing call into the conference, and now
            // you're detaching it but haven't answered it yet, so you shouldn't put it on hold
            removeParticipant(call_id);
        } else {
            onHoldCall(call_id);
            removeParticipant(call_id);
        }
    } else {
        DEBUG("Unbind main participant from conference %d");
        getMainBuffer().unBindAll(MainBuffer::DEFAULT_ID);

        if (not isConference(current_call_id)) {
            ERROR("Current call id (%s) is not a conference", current_call_id.c_str());
            return;
        }

        ConferenceMap::iterator iter = conferenceMap_.find(current_call_id);

        Conference *conf = iter->second;
        if (iter == conferenceMap_.end() or conf == 0) {
            DEBUG("Conference is NULL");
            return;
        }

        if (conf->getState() == Conference::ACTIVE_ATTACHED)
            conf->setState(Conference::ACTIVE_DETACHED);
        else if (conf->getState() == Conference::ACTIVE_ATTACHED_REC)
            conf->setState(Conference::ACTIVE_DETACHED_REC);
        else
            WARN("Undefined behavior, invalid conference state in detach participant");

#if HAVE_DBUS
        dbus_.getCallManager()->conferenceChanged(conf->getConfID(),
                                                  conf->getStateStr());
#endif

        unsetCurrentCall();
    }
}

void ManagerImpl::removeParticipant(const std::string& call_id)
{
    DEBUG("Remove participant %s", call_id.c_str());

    // this call is no longer a conference participant
    Call *call(getCallFromCallID(call_id));
    if (call == 0) {
        ERROR("Call not found");
        return;
    }

    ConferenceMap::const_iterator iter = conferenceMap_.find(call->getConfId());

    Conference *conf = iter->second;
    if (iter == conferenceMap_.end() or conf == 0) {
        ERROR("No conference with id %s, cannot remove participant", call->getConfId().c_str());
        return;
    }

    conf->remove(call_id);
    call->setConfId("");

    removeStream(call_id);

    getMainBuffer().dumpInfo();
#if HAVE_DBUS
    dbus_.getCallManager()->conferenceChanged(conf->getConfID(), conf->getStateStr());
#endif
    processRemainingParticipants(*conf);
}

void ManagerImpl::processRemainingParticipants(Conference &conf)
{
    const std::string current_call_id(getCurrentCallId());
    ParticipantSet participants(conf.getParticipantList());
    const size_t n = participants.size();
    DEBUG("Process remaining %d participant(s) from conference %s",
           n, conf.getConfID().c_str());

    if (n > 1) {
        // Reset ringbuffer's readpointers
        for (ParticipantSet::const_iterator p = participants.begin();
             p != participants.end(); ++p)
            getMainBuffer().flush(*p);

        getMainBuffer().flush(MainBuffer::DEFAULT_ID);
    } else if (n == 1) {
        // this call is the last participant, hence
        // the conference is over
        ParticipantSet::iterator p = participants.begin();

        Call *call = getCallFromCallID(*p);
        if (call) {
            call->setConfId("");
            // if we are not listening to this conference
            if (current_call_id != conf.getConfID())
                onHoldCall(call->getCallId());
            else
                switchCall(*p);
        }

        DEBUG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
    } else {
        DEBUG("No remaining participants, remove conference");
        removeConference(conf.getConfID());
        unsetCurrentCall();
    }
}

void ManagerImpl::joinConference(const std::string& conf_id1,
                                 const std::string& conf_id2)
{
    DEBUG("Join conferences %s and %s", conf_id1.c_str(), conf_id2.c_str());

    if (conferenceMap_.find(conf_id1) == conferenceMap_.end()) {
        ERROR("Not a valid conference ID: %s", conf_id1.c_str());
        return;
    }

    if (conferenceMap_.find(conf_id2) == conferenceMap_.end()) {
        ERROR("Not a valid conference ID: %s", conf_id2.c_str());
        return;
    }

    Conference *conf = conferenceMap_.find(conf_id1)->second;
    ParticipantSet participants(conf->getParticipantList());

    for (ParticipantSet::const_iterator p = participants.begin();
            p != participants.end(); ++p) {
        detachParticipant(*p, "");
        addParticipant(*p, conf_id2);
    }
}

void ManagerImpl::addStream(const std::string& call_id)
{
    DEBUG("Add audio stream %s", call_id.c_str());
    Call *call = getCallFromCallID(call_id);

    if (call and isConferenceParticipant(call_id)) {
        DEBUG("Add stream to conference");

        // bind to conference participant
        ConferenceMap::iterator iter = conferenceMap_.find(call->getConfId());

        if (iter != conferenceMap_.end() and iter->second) {
            Conference* conf = iter->second;

            conf->bindParticipant(call_id);

            ParticipantSet participants(conf->getParticipantList());

            // reset ring buffer for all conference participant
            for (ParticipantSet::const_iterator iter_p = participants.begin();
                    iter_p != participants.end(); ++iter_p)
                getMainBuffer().flush(*iter_p);

            getMainBuffer().flush(MainBuffer::DEFAULT_ID);
        }

    } else {
        DEBUG("Add stream to call");

        // bind to main
        getMainBuffer().bindCallID(call_id, MainBuffer::DEFAULT_ID);

        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->flushUrgent();
        audiodriver_->flushMain();
    }

    getMainBuffer().dumpInfo();
}

void ManagerImpl::removeStream(const std::string& call_id)
{
    DEBUG("Remove audio stream %s", call_id.c_str());
    getMainBuffer().unBindAll(call_id);
    getMainBuffer().dumpInfo();
}

//THREAD=Main
void ManagerImpl::saveConfig()
{
    DEBUG("Saving Configuration to XDG directory %s", path_.c_str());
    AudioLayer *audiolayer = getAudioDriver();
    if (audiolayer != NULL) {
        audioPreference.setVolumemic(audiolayer->getCaptureGain());
        audioPreference.setVolumespkr(audiolayer->getPlaybackGain());
    }

    try {
        Conf::YamlEmitter emitter(path_.c_str());

        for (AccountMap::iterator iter = SIPVoIPLink::instance()->getAccounts().begin();
             iter != SIPVoIPLink::instance()->getAccounts().end(); ++iter)
            iter->second->serialize(emitter);

#if HAVE_IAX
        for (AccountMap::iterator iter = IAXVoIPLink::getAccounts().begin(); iter != IAXVoIPLink::getAccounts().end(); ++iter)
            iter->second->serialize(emitter);
#endif

        preferences.serialize(emitter);
        voipPreferences.serialize(emitter);
        addressbookPreference.serialize(emitter);
        hookPreference.serialize(emitter);
        audioPreference.serialize(emitter);
#ifdef SFL_VIDEO
        getVideoControls()->getVideoPreferences().serialize(emitter);
#endif
        shortcutPreferences.serialize(emitter);

        emitter.serializeData();
    } catch (const Conf::YamlEmitterException &e) {
        ERROR("ConfigTree: %s", e.what());
    }
}

//THREAD=Main
void ManagerImpl::sendDtmf(const std::string& id, char code)
{
    std::string accountid(getAccountFromCall(id));
    playDtmf(code);
    getAccountLink(accountid)->carryingDTMFdigits(id, code);
}

//THREAD=Main | VoIPLink
void ManagerImpl::playDtmf(char code)
{
    stopTone();

    if (not voipPreferences.getPlayDtmf()) {
        DEBUG("Do not have to play a tone...");
        return;
    }

    // length in milliseconds
    int pulselen = voipPreferences.getPulseLength();

    if (pulselen == 0) {
        DEBUG("Pulse length is not set...");
        return;
    }

    ost::MutexLock lock(audioLayerMutex_);

    // numbers of int = length in milliseconds / 1000 (number of seconds)
    //                = number of seconds * SAMPLING_RATE by SECONDS

    // fast return, no sound, so no dtmf
    if (audiodriver_ == NULL || dtmfKey_.get() == 0) {
        DEBUG("No audio layer...");
        return;
    }

    // number of data sampling in one pulselen depends on samplerate
    // size (n sampling) = time_ms * sampling/s
    //                     ---------------------
    //                            ms/s
    int size = (int)((pulselen * (float) audiodriver_->getSampleRate()) / 1000);

    // this buffer is for mono
    // TODO <-- this should be global and hide if same size
    std::vector<SFLDataFormat> buf(size);

    // Handle dtmf
    dtmfKey_->startTone(code);

    // copy the sound
    if (dtmfKey_->generateDTMF(buf)) {
        // Put buffer to urgentRingBuffer
        // put the size in bytes...
        // so size * 1 channel (mono) * sizeof (bytes for the data)
        // audiolayer->flushUrgent();
        audiodriver_->startStream();
        audiodriver_->putUrgent(&(*buf.begin()), size * sizeof(SFLDataFormat));
    }

    // TODO Cache the DTMF
}

// Multi-thread
bool ManagerImpl::incomingCallWaiting() const
{
    return nbIncomingWaitingCall_ > 0;
}

void ManagerImpl::addWaitingCall(const std::string& id)
{
    ost::MutexLock m(waitingCallMutex_);
    waitingCall_.insert(id);
    nbIncomingWaitingCall_++;
}

void ManagerImpl::removeWaitingCall(const std::string& id)
{
    ost::MutexLock m(waitingCallMutex_);

    if (waitingCall_.erase(id))
        nbIncomingWaitingCall_--;
}

///////////////////////////////////////////////////////////////////////////////
// Management of event peer IP-phone
////////////////////////////////////////////////////////////////////////////////
// SipEvent Thread
void ManagerImpl::incomingCall(Call &call, const std::string& accountId)
{
    stopTone();
    const std::string callID(call.getCallId());

    associateCallToAccount(callID, accountId);

    if (accountId.empty())
        setIPToIPForCall(callID, true);
    else {
        // strip sip: which is not required and bring confusion with ip to ip calls
        // when placing new call from history (if call is IAX, do nothing)
        std::string peerNumber(call.getPeerNumber());

        const char SIP_PREFIX[] = "sip:";
        size_t startIndex = peerNumber.find(SIP_PREFIX);

        if (startIndex != std::string::npos)
            call.setPeerNumber(peerNumber.substr(startIndex + sizeof(SIP_PREFIX) - 1));
    }

    if (not hasCurrentCall()) {
        call.setConnectionState(Call::RINGING);
        ringtone(accountId);
    }

    addWaitingCall(callID);

    std::string number(call.getPeerNumber());

    std::string from("<" + number + ">");
#if HAVE_DBUS
    dbus_.getCallManager()->incomingCall(accountId, callID, call.getDisplayName() + " " + from);
#endif
}


//THREAD=VoIP
#if HAVE_INSTANT_MESSAGING
void ManagerImpl::incomingMessage(const std::string& callID,
                                  const std::string& from,
                                  const std::string& message)
{
    if (isConferenceParticipant(callID)) {
        Conference *conf = getConferenceFromCallID(callID);

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            if (*iter_p == callID)
                continue;

            std::string accountId(getAccountFromCall(*iter_p));

            DEBUG("Send message to %s, (%s)", (*iter_p).c_str(), accountId.c_str());

            Account *account = getAccount(accountId);

            if (!account) {
                ERROR("Failed to get account while sending instant message");
                return;
            }

            account->getVoIPLink()->sendTextMessage(callID, message, from);
        }

#if HAVE_DBUS
        // in case of a conference we must notify client using conference id
        dbus_.getCallManager()->incomingMessage(conf->getConfID(), from, message);

    } else
        dbus_.getCallManager()->incomingMessage(callID, from, message);
#else
    }
#endif
}

//THREAD=VoIP
bool ManagerImpl::sendTextMessage(const std::string& callID, const std::string& message, const std::string& from)
{
    if (isConference(callID)) {
        DEBUG("Is a conference, send instant message to everyone");
        ConferenceMap::iterator it = conferenceMap_.find(callID);

        if (it == conferenceMap_.end())
            return false;

        Conference *conf = it->second;

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            std::string accountId = getAccountFromCall(*iter_p);

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(*iter_p, message, from);
        }

        return true;
    }

    if (isConferenceParticipant(callID)) {
        DEBUG("Call is participant in a conference, send instant message to everyone");
        Conference *conf = getConferenceFromCallID(callID);

        if (!conf)
            return false;

        ParticipantSet participants(conf->getParticipantList());

        for (ParticipantSet::const_iterator iter_p = participants.begin();
                iter_p != participants.end(); ++iter_p) {

            const std::string accountId(getAccountFromCall(*iter_p));

            Account *account = getAccount(accountId);

            if (!account) {
                DEBUG("Failed to get account while sending instant message");
                return false;
            }

            account->getVoIPLink()->sendTextMessage(*iter_p, message, from);
        }
    } else {
        Account *account = getAccount(getAccountFromCall(callID));

        if (!account) {
            DEBUG("Failed to get account while sending instant message");
            return false;
        }

        account->getVoIPLink()->sendTextMessage(callID, message, from);
    }
    return true;
}
#endif // HAVE_INSTANT_MESSAGING

//THREAD=VoIP CALL=Outgoing
void ManagerImpl::peerAnsweredCall(const std::string& id)
{
    DEBUG("Peer answered call %s", id.c_str());

    // The if statement is usefull only if we sent two calls at the same time.
    if (isCurrentCall(id)) {
        stopTone();

        // set playback mode to VOICE
        AudioLayer *al = getAudioDriver();
        if(al) al->setPlaybackMode(AudioLayer::VOICE);
    }

    // Connect audio streams
    addStream(id);

    {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->flushMain();
        audiodriver_->flushUrgent();
    }

    if (audioPreference.getIsAlwaysRecording())
        setRecordingCall(id);

#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(id, "CURRENT");
#endif
}

//THREAD=VoIP Call=Outgoing
void ManagerImpl::peerRingingCall(const std::string& id)
{
    DEBUG("Peer call %s ringing", id.c_str());

    if (isCurrentCall(id))
        ringback();

#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(id, "RINGING");
#endif
}

//THREAD=VoIP Call=Outgoing/Ingoing
void ManagerImpl::peerHungupCall(const std::string& call_id)
{
    DEBUG("Peer hungup call %s", call_id.c_str());

    if (isConferenceParticipant(call_id)) {
        removeParticipant(call_id);
    } else if (isCurrentCall(call_id)) {
        stopTone();
        unsetCurrentCall();

        // set playback mode to NONE
        AudioLayer *al = getAudioDriver();
        if(al) al->setPlaybackMode(AudioLayer::NONE);
    }

    /* Direct IP to IP call */
    if (isIPToIP(call_id)) {
        Call * call = SIPVoIPLink::instance()->getSipCall(call_id);
        history_.addCall(call, preferences.getHistoryLimit());
        SIPVoIPLink::instance()->hangup(call_id);
        saveHistory();
    } else {
        const std::string account_id(getAccountFromCall(call_id));
        VoIPLink *link = getAccountLink(account_id);
        Call * call = getCallFromCallID(call_id);
        history_.addCall(call, preferences.getHistoryLimit());
        link->peerHungup(call_id);
        saveHistory();
    }

#if HAVE_DBUS
    /* Broadcast a signal over DBus */
    dbus_.getCallManager()->callStateChanged(call_id, "HUNGUP");
#endif

    removeWaitingCall(call_id);
    removeCallAccount(call_id);
    removeStream(call_id);

    if (getCallList().empty()) {
        DEBUG("Stop audio stream, there are no calls remaining");
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }
}

//THREAD=VoIP
void ManagerImpl::callBusy(const std::string& id)
{
    DEBUG("Call %s busy", id.c_str());
#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(id, "BUSY");
#endif

    if (isCurrentCall(id)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    removeCallAccount(id);
    removeWaitingCall(id);
}

//THREAD=VoIP
void ManagerImpl::callFailure(const std::string& call_id)
{
#if HAVE_DBUS
    dbus_.getCallManager()->callStateChanged(call_id, "FAILURE");
#endif

    if (isCurrentCall(call_id)) {
        playATone(Tone::TONE_BUSY);
        unsetCurrentCall();
    }

    if (isConferenceParticipant(call_id)) {
        DEBUG("Call %s participating in a conference failed", call_id.c_str());
        // remove this participant
        removeParticipant(call_id);
    }

    removeCallAccount(call_id);
    removeWaitingCall(call_id);
}

//THREAD=VoIP
void ManagerImpl::startVoiceMessageNotification(const std::string& accountId,
        int nb_msg)
{
#if HAVE_DBUS
    dbus_.getCallManager()->voiceMailNotify(accountId, nb_msg);
#endif
}

/**
 * Multi Thread
 */
void ManagerImpl::playATone(Tone::TONEID toneId)
{
    if (not voipPreferences.getPlayTones())
        return;

    {
        ost::MutexLock lock(audioLayerMutex_);

        if (audiodriver_ == NULL) {
            ERROR("Audio layer not initialized");
            return;
        }

        audiodriver_->flushUrgent();
        audiodriver_->startStream();
    }

    if (telephoneTone_.get() != 0) {
        ost::MutexLock lock(toneMutex_);
        telephoneTone_->setCurrentTone(toneId);
    }
}

/**
 * Multi Thread
 */
void ManagerImpl::stopTone()
{
    if (not voipPreferences.getPlayTones())
        return;

    ost::MutexLock lock(toneMutex_);

    if (telephoneTone_.get() != NULL)
        telephoneTone_->setCurrentTone(Tone::TONE_NULL);

    if (audiofile_.get()) {
        std::string filepath(audiofile_->getFilePath());
#if HAVE_DBUS
        dbus_.getCallManager()->recordPlaybackStopped(filepath);
#endif
        audiofile_.reset();
    }
}

/**
 * Multi Thread
 */
void ManagerImpl::playTone()
{
    playATone(Tone::TONE_DIALTONE);
}

/**
 * Multi Thread
 */
void ManagerImpl::playToneWithMessage()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void ManagerImpl::congestion()
{
    playATone(Tone::TONE_CONGESTION);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringback()
{
    playATone(Tone::TONE_RINGTONE);
}

/**
 * Multi Thread
 */
void ManagerImpl::ringtone(const std::string& accountID)
{
    Account *account = getAccount(accountID);

    if (!account) {
        WARN("Invalid account in ringtone");
        return;
    }

    if (!account->getRingtoneEnabled()) {
        ringback();
        return;
    }

    std::string ringchoice = account->getRingtonePath();

    if (ringchoice.find(DIR_SEPARATOR_STR) == std::string::npos) {
        // check inside global share directory
        static const char * const RINGDIR = "ringtones";
        ringchoice = std::string(PROGSHAREDIR) + DIR_SEPARATOR_STR
                     + RINGDIR + DIR_SEPARATOR_STR + ringchoice;
    }

    int audioLayerSmplr = 8000;
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_) {
            ERROR("no audio layer in ringtone");
            return;
        }

        audioLayerSmplr = audiodriver_->getSampleRate();
    }

    {
        ost::MutexLock m(toneMutex_);

        if (audiofile_.get()) {
#if HAVE_DBUS
            dbus_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
#endif
            audiofile_.reset();
        }

        try {
            if (ringchoice.find(".wav") != std::string::npos)
                audiofile_.reset(new WaveFile(ringchoice, audioLayerSmplr));
            else {
                sfl::Codec *codec;
                if (ringchoice.find(".ul") != std::string::npos or ringchoice.find(".au") != std::string::npos)
                    codec = audioCodecFactory.getCodec(PAYLOAD_CODEC_ULAW);
                else
                    throw AudioFileException("Couldn't guess an appropriate decoder");

                audiofile_.reset(new RawFile(ringchoice, static_cast<sfl::AudioCodec *>(codec), audioLayerSmplr));
            }
        } catch (const AudioFileException &e) {
            ERROR("Exception: %s", e.what());
        }
    } // leave mutex

    ost::MutexLock lock(audioLayerMutex_);
    // start audio if not started AND flush all buffers (main and urgent)
    audiodriver_->startStream();
}

AudioLoop* ManagerImpl::getTelephoneTone()
{
    if (telephoneTone_.get()) {
        ost::MutexLock m(toneMutex_);
        return telephoneTone_->getCurrentTone();
    } else
        return NULL;
}

AudioLoop*
ManagerImpl::getTelephoneFile()
{
    ost::MutexLock m(toneMutex_);
    return audiofile_.get();
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////
/**
 * Initialization: Main Thread
 */
std::string ManagerImpl::retrieveConfigPath() const
{
#if ANDROID
    std::string configdir = "/data/data/com.savoirfairelinux.sflphone";
#else
    std::string configdir = std::string(HOMEDIR) + DIR_SEPARATOR_STR +
                             ".config" + DIR_SEPARATOR_STR + PACKAGE;
#endif

    std::string xdg_env(XDG_CONFIG_HOME);
    if (not xdg_env.empty()) {
        configdir = xdg_env;
    }

    if (mkdir(configdir.data(), 0700) != 0) {
        // If directory creation failed
        if (errno != EEXIST)
           DEBUG("Cannot create directory: %s!", configdir.c_str());
    }

    static const char * const PROGNAME = "sflphoned";
    return configdir + DIR_SEPARATOR_STR + PROGNAME + ".yml";
}

std::vector<std::string> ManagerImpl::split_string(std::string s)
{
    std::vector<std::string> list;
    std::string temp;

    while (s.find("/", 0) != std::string::npos) {
        size_t pos = s.find("/", 0);
        temp = s.substr(0, pos);
        s.erase(0, pos + 1);
        list.push_back(temp);
    }

    return list;
}

std::string ManagerImpl::join_string(const std::vector<std::string> &v)
{
    std::ostringstream os;
    std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>(os, "/"));
    return os.str();
}

std::string ManagerImpl::getCurrentAudioCodecName(const std::string& id)
{
    std::string accountid = getAccountFromCall(id);
    VoIPLink* link = getAccountLink(accountid);
    Call* call = getCallFromCallID(id);
    std::string codecName;

    if (call) {
        Call::CallState state = call->getState();

        if (state == Call::ACTIVE or state == Call::CONFERENCING)
            codecName = link->getCurrentAudioCodecName(call);
    }

    return codecName;
}

std::string
ManagerImpl::getCurrentVideoCodecName(const std::string& ID)
{
    std::string accountID = getAccountFromCall(ID);
    VoIPLink* link = getAccountLink(accountID);
    Call *call(getCallFromCallID(ID));
    if (call)
        return link->getCurrentVideoCodecName(call);
    else
        return "";
}

/**
 * Set input audio plugin
 */
void ManagerImpl::setAudioPlugin(const std::string& audioPlugin)
{
    ost::MutexLock lock(audioLayerMutex_);

    audioPreference.setAlsaPlugin(audioPlugin);

    bool wasStarted = audiodriver_->isStarted();

    // Recreate audio driver with new settings
    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

/**
 * Set audio output device
 */
void ManagerImpl::setAudioDevice(int index, AudioLayer::PCMType type)
{
    ost::MutexLock lock(audioLayerMutex_);

    if (!audiodriver_) {
        ERROR("Audio driver not initialized");
        return ;
    }

    const bool wasStarted = audiodriver_->isStarted();
    audiodriver_->updatePreference(audioPreference, index, type);

    // Recreate audio driver with new settings
    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

/**
 * Get list of supported audio output device
 */
std::vector<std::string> ManagerImpl::getAudioOutputDeviceList()
{
    ost::MutexLock lock(audioLayerMutex_);
    return audiodriver_->getPlaybackDeviceList();
}


/**
 * Get list of supported audio input device
 */
std::vector<std::string> ManagerImpl::getAudioInputDeviceList()
{
    ost::MutexLock lock(audioLayerMutex_);
    return audiodriver_->getCaptureDeviceList();
}

/**
 * Get string array representing integer indexes of output and input device
 */
std::vector<std::string> ManagerImpl::getCurrentAudioDevicesIndex()
{
    ost::MutexLock lock(audioLayerMutex_);

    std::vector<std::string> v;

    std::stringstream ssi, sso, ssr;
    sso << audiodriver_->getIndexPlayback();
    v.push_back(sso.str());
    ssi << audiodriver_->getIndexCapture();
    v.push_back(ssi.str());
    ssr << audiodriver_->getIndexRingtone();
    v.push_back(ssr.str());

    return v;
}

int ManagerImpl::isRingtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return 0;
    }

    return account->getRingtoneEnabled();
}

void ManagerImpl::ringtoneEnabled(const std::string& id)
{
    Account *account = getAccount(id);

    if (!account) {
        WARN("Invalid account in ringtone enabled");
        return;
    }

    account->getRingtoneEnabled() ? account->setRingtoneEnabled(false) : account->setRingtoneEnabled(true);
}

bool ManagerImpl::getIsAlwaysRecording() const
{
    return audioPreference.getIsAlwaysRecording();
}

void ManagerImpl::setIsAlwaysRecording(bool isAlwaysRec)
{
    return audioPreference.setIsAlwaysRecording(isAlwaysRec);
}

void ManagerImpl::setRecordingCall(const std::string& id)
{
    Recordable* rec = NULL;

    ConferenceMap::const_iterator it(conferenceMap_.find(id));
    if (it == conferenceMap_.end()) {
        DEBUG("Set recording for call %s", id.c_str());
        std::string accountid(getAccountFromCall(id));
        rec = getCallFromCallID(id);
    } else {
        DEBUG("Set recording for conference %s", id.c_str());
        Conference *conf = it->second;

        if (conf) {
            rec = conf;
            if (rec->isRecording())
                conf->setState(Conference::ACTIVE_ATTACHED);
            else
                conf->setState(Conference::ACTIVE_ATTACHED_REC);
        }
    }

    if (rec == NULL) {
        ERROR("Could not find recordable instance %s", id.c_str());
        return;
    }

    rec->setRecording();
#if HAVE_DBUS
    dbus_.getCallManager()->recordPlaybackFilepath(id, rec->getFilename());
#endif
}

bool ManagerImpl::isRecording(const std::string& id)
{
    Recordable* rec = getCallFromCallID(id);
    return rec and rec->isRecording();
}

bool ManagerImpl::startRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Start recorded file playback %s", filepath.c_str());

    int sampleRate;
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_) {
            ERROR("No audio layer in start recorded file playback");
            return false;
        }

        sampleRate = audiodriver_->getSampleRate();
    }

    {
        ost::MutexLock m(toneMutex_);

        if (audiofile_.get()) {
#if HAVE_DBUS
            dbus_.getCallManager()->recordPlaybackStopped(audiofile_->getFilePath());
#endif
            audiofile_.reset();
        }

        try {
            audiofile_.reset(new WaveFile(filepath, sampleRate));
            audiofile_.get()->setIsRecording(true);
        } catch (const AudioFileException &e) {
            ERROR("Exception: %s", e.what());
        }
    } // release toneMutex

    ost::MutexLock lock(audioLayerMutex_);
    audiodriver_->startStream();

    return true;
}

void ManagerImpl::recordingPlaybackSeek(const double value)
{
    if(audiofile_.get()) {
        audiofile_.get()->seek(value);
    }
}


void ManagerImpl::stopRecordedFilePlayback(const std::string& filepath)
{
    DEBUG("Stop recorded file playback %s", filepath.c_str());

    {
        ost::MutexLock lock(audioLayerMutex_);
        audiodriver_->stopStream();
    }

    {
        ost::MutexLock m(toneMutex_);
        audiofile_.reset();
    }
}

void ManagerImpl::setHistoryLimit(int days)
{
    DEBUG("Set history limit");
    preferences.setHistoryLimit(days);
    saveConfig();
}

int ManagerImpl::getHistoryLimit() const
{
    return preferences.getHistoryLimit();
}

int32_t ManagerImpl::getMailNotify() const
{
    return preferences.getNotifyMails();
}

void ManagerImpl::setMailNotify()
{
    DEBUG("Set mail notify");
    preferences.getNotifyMails() ? preferences.setNotifyMails(true) : preferences.setNotifyMails(false);
    saveConfig();
}

void ManagerImpl::setAudioManager(const std::string &api)
{
    {
        ost::MutexLock lock(audioLayerMutex_);

        if (!audiodriver_)
            return;

        if (api == audioPreference.getAudioApi()) {
            DEBUG("Audio manager chosen already in use. No changes made. ");
            return;
        }
    }

    switchAudioManager();

    saveConfig();
}

std::string ManagerImpl::getAudioManager() const
{
    return audioPreference.getAudioApi();
}


int ManagerImpl::getAudioDeviceIndex(const std::string &name)
{
    int soundCardIndex = 0;

    ost::MutexLock lock(audioLayerMutex_);

    if (audiodriver_ == NULL) {
        ERROR("Audio layer not initialized");
        return soundCardIndex;
    }

    return audiodriver_->getAudioDeviceIndex(name);
}

std::string ManagerImpl::getCurrentAudioOutputPlugin() const
{
    return audioPreference.getAlsaPlugin();
}


std::string ManagerImpl::getNoiseSuppressState() const
{
    return audioPreference.getNoiseReduce() ? "enabled" : "disabled";
}

void ManagerImpl::setNoiseSuppressState(const std::string &state)
{
    audioPreference.setNoiseReduce(state == "enabled");
}

bool ManagerImpl::getEchoCancelState() const
{
    return audioPreference.getEchoCancel();
}

void ManagerImpl::setEchoCancelState(const std::string &state)
{
    audioPreference.setEchoCancel(state == "enabled");
}

int ManagerImpl::getEchoCancelTailLength() const
{
    return audioPreference.getEchoCancelTailLength();
}

void ManagerImpl::setEchoCancelTailLength(int length)
{
    audioPreference.setEchoCancelTailLength(length);
}

int ManagerImpl::getEchoCancelDelay() const
{
    return audioPreference.getEchoCancelDelay();
}

void ManagerImpl::setEchoCancelDelay(int delay)
{
    audioPreference.setEchoCancelDelay(delay);
}

/**
 * Initialization: Main Thread
 */
void ManagerImpl::initAudioDriver()
{
    ost::MutexLock lock(audioLayerMutex_);
    audiodriver_ = audioPreference.createAudioLayer();
}

void ManagerImpl::switchAudioManager()
{
    ost::MutexLock lock(audioLayerMutex_);

    bool wasStarted = audiodriver_->isStarted();
    delete audiodriver_;
    audiodriver_ = audioPreference.switchAndCreateAudioLayer();

    if (wasStarted)
        audiodriver_->startStream();
}

void ManagerImpl::audioSamplingRateChanged(int samplerate)
{
    ost::MutexLock lock(audioLayerMutex_);

    if (!audiodriver_) {
        DEBUG("No Audio driver initialized");
        return;
    }

    // Only modify internal sampling rate if new sampling rate is higher
    int currentSamplerate = mainBuffer_.getInternalSamplingRate();

    if (currentSamplerate >= samplerate) {
        DEBUG("No need to update audio layer sampling rate");
        return;
    } else
        DEBUG("Audio sampling rate changed: %d -> %d", currentSamplerate, samplerate);

    bool wasActive = audiodriver_->isStarted();

    mainBuffer_.setInternalSamplingRate(samplerate);

    delete audiodriver_;
    audiodriver_ = audioPreference.createAudioLayer();

    unsigned int sampleRate = audiodriver_->getSampleRate();

    telephoneTone_.reset(new TelephoneTone(preferences.getZoneToneChoice(), sampleRate));
    dtmfKey_.reset(new DTMF(sampleRate));

    if (wasActive)
        audiodriver_->startStream();
}

//THREAD=Main
std::string ManagerImpl::getConfigString(const std::string& section,
                                         const std::string& name) const
{
    return config_.getConfigTreeItemValue(section, name);
}

//THREAD=Main
void ManagerImpl::setConfig(const std::string& section,
                            const std::string& name, const std::string& value)
{
    config_.setConfigTreeItem(section, name, value);
}

//THREAD=Main
void ManagerImpl::setConfig(const std::string& section,
                            const std::string& name, int value)
{
    std::ostringstream valueStream;
    valueStream << value;
    config_.setConfigTreeItem(section, name, valueStream.str());
}

void ManagerImpl::setAccountsOrder(const std::string& order)
{
    DEBUG("Set accounts order : %s", order.c_str());
    // Set the new config

    preferences.setAccountOrder(order);

    saveConfig();
}

std::vector<std::string> ManagerImpl::getAccountList() const
{
    using std::vector;
    using std::string;
    vector<string> account_order(loadAccountOrder());

    // The IP2IP profile is always available, and first in the list
    Account *account = getIP2IPAccount();

    vector<string> v;
    if (account)
        v.push_back(account->getAccountID());
    else
        ERROR("could not find IP2IP profile in getAccount list");

    // Concatenate all account pointers in a single map
    AccountMap concatenatedMap;
    fillConcatAccountMap(concatenatedMap);

    // If no order has been set, load the default one ie according to the creation date.
    if (account_order.empty()) {
        for (AccountMap::const_iterator iter = concatenatedMap.begin(); iter != concatenatedMap.end(); ++iter) {
            if (iter->first == SIPAccount::IP2IP_PROFILE || iter->first.empty())
                continue;

            if (iter->second)
                v.push_back(iter->second->getAccountID());
        }
    }
    else {
        for (vector<string>::const_iterator iter = account_order.begin(); iter != account_order.end(); ++iter) {
            if (*iter == SIPAccount::IP2IP_PROFILE or iter->empty())
                continue;

            AccountMap::const_iterator account_iter = concatenatedMap.find(*iter);

            if (account_iter != concatenatedMap.end() and account_iter->second)
                v.push_back(account_iter->second->getAccountID());
        }
    }

    return v;
}

std::map<std::string, std::string> ManagerImpl::getAccountDetails(
    const std::string& accountID) const
{
    // Default account used to get default parameters if requested by client (to build new account)
    static const SIPAccount DEFAULT_ACCOUNT("default");

    if (accountID.empty()) {
        DEBUG("Returning default account settings");
        return DEFAULT_ACCOUNT.getAccountDetails();
    }

    Account * account = getAccount(accountID);

    if (account)
        return account->getAccountDetails();
    else {
        ERROR("Get account details on a non-existing accountID %s. Returning default", accountID.c_str());
        return DEFAULT_ACCOUNT.getAccountDetails();
    }
}

// method to reduce the if/else mess.
// Even better, switch to XML !

void ManagerImpl::setAccountDetails(const std::string& accountID,
                                    const std::map<std::string, std::string>& details)
{
    DEBUG("Set account details for %s", accountID.c_str());

    Account* account = getAccount(accountID);

    if (account == NULL) {
        ERROR("Could not find account %s", accountID.c_str());
        return;
    }

    account->setAccountDetails(details);

    // Serialize configuration to disk once it is done
    saveConfig();

    if (account->isEnabled())
        account->registerVoIPLink();
    else
        account->unregisterVoIPLink();

#if HAVE_DBUS
    // Update account details to the client side
    dbus_.getConfigurationManager()->accountsChanged();
#endif
}

std::string
ManagerImpl::addAccount(const std::map<std::string, std::string>& details)
{
    /** @todo Deal with both the accountMap_ and the Configuration */
    std::stringstream accountID;

    accountID << "Account:" << time(NULL);
    std::string newAccountID(accountID.str());

    // Get the type

    std::string accountType;
    if (details.find(CONFIG_ACCOUNT_TYPE) == details.end())
        accountType = "SIP";
    else
        accountType = ((*details.find(CONFIG_ACCOUNT_TYPE)).second);

    DEBUG("Adding account %s", newAccountID.c_str());

    /** @todo Verify the uniqueness, in case a program adds accounts, two in a row. */

    Account* newAccount = NULL;

    if (accountType == "SIP") {
        newAccount = new SIPAccount(newAccountID);
        SIPVoIPLink::instance()->getAccounts()[newAccountID] = newAccount;
    }
#if HAVE_IAX
    else if (accountType == "IAX") {
        newAccount = new IAXAccount(newAccountID);
        IAXVoIPLink::getAccounts()[newAccountID] = newAccount;
    }
#endif
    else {
        ERROR("Unknown %s param when calling addAccount(): %s",
               CONFIG_ACCOUNT_TYPE, accountType.c_str());
        return "";
    }

    newAccount->setAccountDetails(details);

    // Add the newly created account in the account order list
    std::string accountList(preferences.getAccountOrder());

    newAccountID += "/";
    if (not accountList.empty()) {
        // Prepend the new account
        accountList.insert(0, newAccountID);
        preferences.setAccountOrder(accountList);
    } else {
        accountList = newAccountID;
        preferences.setAccountOrder(accountList);
    }

    DEBUG("Getting accounts: %s", accountList.c_str());

    newAccount->registerVoIPLink();

    saveConfig();

#if HAVE_DBUS
    dbus_.getConfigurationManager()->accountsChanged();
#endif

    return accountID.str();
}

void ManagerImpl::removeAccount(const std::string& accountID)
{
    // Get it down and dying
    Account* remAccount = getAccount(accountID);

    if (remAccount != NULL) {
        remAccount->unregisterVoIPLink();
        SIPVoIPLink::instance()->getAccounts().erase(accountID);
#if HAVE_IAX
        IAXVoIPLink::getAccounts().erase(accountID);
#endif
        // http://projects.savoirfairelinux.net/issues/show/2355
        // delete remAccount;
    }

    config_.removeSection(accountID);

    saveConfig();

#if HAVE_DBUS
    dbus_.getConfigurationManager()->accountsChanged();
#endif
}

// ACCOUNT handling
void ManagerImpl::associateCallToAccount(const std::string& callID,
        const std::string& accountID)
{
    ost::MutexLock m(callAccountMapMutex_);
    callAccountMap_[callID] = accountID;
    DEBUG("Associate Call %s with Account %s", callID.data(), accountID.data());
}

std::string ManagerImpl::getAccountFromCall(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    CallAccountMap::iterator iter = callAccountMap_.find(callID);

    return (iter == callAccountMap_.end()) ? "" : iter->second;
}

void ManagerImpl::removeCallAccount(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    callAccountMap_.erase(callID);

    // Stop audio layer if there is no call anymore
    if (callAccountMap_.empty()) {
        ost::MutexLock lock(audioLayerMutex_);

        if (audiodriver_)
            audiodriver_->stopStream();
    }

}

bool ManagerImpl::isValidCall(const std::string& callID)
{
    ost::MutexLock m(callAccountMapMutex_);
    return callAccountMap_.find(callID) != callAccountMap_.end();
}

std::string ManagerImpl::getNewCallID()
{
    std::ostringstream random_id("s");
    random_id << (unsigned) rand();

    // when it's not found, it return ""
    // generate, something like s10000s20000s4394040

    while (not getAccountFromCall(random_id.str()).empty()) {
        random_id.clear();
        random_id << "s";
        random_id << (unsigned) rand();
    }

    return random_id.str();
}

std::vector<std::string> ManagerImpl::loadAccountOrder() const
{
    return split_string(preferences.getAccountOrder());
}

namespace {
    bool isIP2IP(const Conf::YamlNode *node)
    {
        if (!node)
            return false;

        std::string id;
        node->getValue("id", &id);
        return id == "IP2IP";
    }

#if HAVE_IAX
    void loadAccount(const Conf::YamlNode *item, AccountMap &sipAccountMap, AccountMap &iaxAccountMap)
#else
    void loadAccount(const Conf::YamlNode *item, AccountMap &sipAccountMap)
#endif
    {
        if (!item) {
            ERROR("Could not load account");
            return;
        }

        std::string accountType;
        item->getValue("type", &accountType);

        std::string accountid;
        item->getValue("id", &accountid);

        std::string accountAlias;
        item->getValue("alias", &accountAlias);

        if (!accountid.empty() and !accountAlias.empty() and accountid != SIPAccount::IP2IP_PROFILE) {
            Account *a;
#if HAVE_IAX
            if (accountType == "IAX") {
                a = new IAXAccount(accountid);
                iaxAccountMap[accountid] = a;
            }
            else { // assume SIP
#endif
                a = new SIPAccount(accountid);
                sipAccountMap[accountid] = a;
#if HAVE_IAX
            }
#endif

            a->unserialize(*item);
        }
    }

    void registerAccount(std::pair<const std::string, Account*> &item)
    {
        item.second->registerVoIPLink();
    }

    void unregisterAccount(std::pair<const std::string, Account*> &item)
    {
        item.second->unregisterVoIPLink();
    }

    SIPAccount *createIP2IPAccount()
    {
        SIPAccount *ip2ip = new SIPAccount(SIPAccount::IP2IP_PROFILE);
        try {
            SIPVoIPLink::instance()->sipTransport.createSipTransport(*ip2ip);
        } catch (const std::runtime_error &e) {
            ERROR("%s", e.what());
        }
        return ip2ip;
    }

} // end anonymous namespace

void ManagerImpl::loadDefaultAccountMap()
{
    // build a default IP2IP account with default parameters only if does not exist
    AccountMap::const_iterator iter = SIPVoIPLink::instance()->getAccounts().find(SIPAccount::IP2IP_PROFILE);
    if (iter == SIPVoIPLink::instance()->getAccounts().end())
        SIPVoIPLink::instance()->getAccounts()[SIPAccount::IP2IP_PROFILE] = createIP2IPAccount();

    SIPVoIPLink::instance()->getAccounts()[SIPAccount::IP2IP_PROFILE]->registerVoIPLink();
}

void ManagerImpl::loadAccountMap(Conf::YamlParser &parser)
{
    using namespace Conf;
    // build a default IP2IP account with default parameters
    AccountMap::const_iterator iter = SIPVoIPLink::instance()->getAccounts().find(SIPAccount::IP2IP_PROFILE);
    if (iter == SIPVoIPLink::instance()->getAccounts().end())
        SIPVoIPLink::instance()->getAccounts()[SIPAccount::IP2IP_PROFILE] = createIP2IPAccount();

    // load saved preferences for IP2IP account from configuration file
    Sequence *seq = parser.getAccountSequence()->getSequence();

    Sequence::const_iterator ip2ip = std::find_if(seq->begin(), seq->end(), isIP2IP);
    if (ip2ip != seq->end()) {
        SIPVoIPLink::instance()->getAccounts()[SIPAccount::IP2IP_PROFILE]->unserialize(**ip2ip);
    }

    // Force IP2IP settings to be loaded
    // No registration in the sense of the REGISTER method is performed.
    SIPVoIPLink::instance()->getAccounts()[SIPAccount::IP2IP_PROFILE]->registerVoIPLink();

    // build preferences
    preferences.unserialize(*parser.getPreferenceNode());
    voipPreferences.unserialize(*parser.getVoipPreferenceNode());
    addressbookPreference.unserialize(*parser.getAddressbookNode());
    hookPreference.unserialize(*parser.getHookNode());
    audioPreference.unserialize(*parser.getAudioNode());
    shortcutPreferences.unserialize(*parser.getShortcutNode());
#ifdef SFL_VIDEO
    VideoControls *controls(getVideoControls());
    try {
        MappingNode *videoNode = parser.getVideoNode();
        if (videoNode)
            controls->getVideoPreferences().unserialize(*videoNode);
    } catch (const YamlParserException &e) {
        ERROR("No video node in config file");
    }
#endif

    using std::tr1::placeholders::_1;
    // Each valid account element in sequence is a new account to load
    // std::for_each(seq->begin(), seq->end(),
    //        std::tr1::bind(loadAccount, _1, std::tr1::ref(accountMap_)));
#if HAVE_IAX
    std::for_each(seq->begin(), seq->end(),
            std::tr1::bind(loadAccount, _1, std::tr1::ref(SIPVoIPLink::instance()->getAccounts()), std::tr1::ref(IAXVoIPLink::getAccounts())));
#else
    std::for_each(seq->begin(), seq->end(),
            std::tr1::bind(loadAccount, _1, std::tr1::ref(SIPVoIPLink::instance()->getAccounts())));
#endif
}

void ManagerImpl::registerAllAccounts()
{
    std::for_each(SIPVoIPLink::instance()->getAccounts().begin(), SIPVoIPLink::instance()->getAccounts().end(), registerAccount);
#if HAVE_IAX
    std::for_each(IAXVoIPLink::getAccounts().begin(), IAXVoIPLink::getAccounts().end(), registerAccount);
#endif
}

void ManagerImpl::unregisterAllAccounts()
{
    std::for_each(SIPVoIPLink::instance()->getAccounts().begin(), SIPVoIPLink::instance()->getAccounts().end(), unregisterAccount);
#if HAVE_IAX
    std::for_each(IAXVoIPLink::getAccounts().begin(), IAXVoIPLink::getAccounts().end(), unregisterAccount);
#endif
}

bool ManagerImpl::accountExists(const std::string &accountID)
{
    bool ret = false;

    ret = SIPVoIPLink::instance()->getAccounts().find(accountID) != SIPVoIPLink::instance()->getAccounts().end();
#if HAVE_IAX
    if(ret)
        return ret;

    ret = IAXVoIPLink::getAccounts().find(accountID) != IAXVoIPLink::getAccounts().end();
#endif

    return ret;
}

SIPAccount*
ManagerImpl::getIP2IPAccount() const
{
    AccountMap::const_iterator iter = SIPVoIPLink::instance()->getAccounts().find(SIPAccount::IP2IP_PROFILE);
    if(iter == SIPVoIPLink::instance()->getAccounts().end())
        return NULL;

    return static_cast<SIPAccount *>(iter->second);
}

Account*
ManagerImpl::getAccount(const std::string& accountID) const
{
    Account *account = NULL;

    account = getSipAccount(accountID);
    if(account != NULL)
        return account;

#if HAVE_IAX
    account = getIaxAccount(accountID);
    if(account != NULL)
        return account;
#endif

    return NULL;
}

SIPAccount *
ManagerImpl::getSipAccount(const std::string& accountID) const
{
    AccountMap::const_iterator iter = SIPVoIPLink::instance()->getAccounts().find(accountID);
    if(iter != SIPVoIPLink::instance()->getAccounts().end())
        return static_cast<SIPAccount *>(iter->second);

    return NULL;
}

#if HAVE_IAX
IAXAccount *
ManagerImpl::getIaxAccount(const std::string& accountID) const
{
    AccountMap::const_iterator iter = IAXVoIPLink::getAccounts().find(accountID);
    if(iter != IAXVoIPLink::getAccounts().end())
        return static_cast<IAXAccount *>(iter->second);

    return NULL;
}
#endif

void
ManagerImpl::fillConcatAccountMap(AccountMap &concatMap) const
{
    concatMap.insert(SIPVoIPLink::instance()->getAccounts().begin(), SIPVoIPLink::instance()->getAccounts().end());
#if HAVE_IAX
    concatMap.insert(IAXVoIPLink::getAccounts().begin(), IAXVoIPLink::getAccounts().end());
#endif
}

std::string
ManagerImpl::getAccountIdFromNameAndServer(const std::string &userName,
                                           const std::string &server) const
{
    DEBUG("username = %s, server = %s", userName.c_str(), server.c_str());
    // Try to find the account id from username and server name by full match

    AccountMap concatenatedMap;
    fillConcatAccountMap(concatenatedMap);
    for (AccountMap::const_iterator iter = concatenatedMap.begin(); iter != concatenatedMap.end(); ++iter) {
        SIPAccount *account = static_cast<SIPAccount *>(iter->second);
        if (account and account->matches(userName, server))
            return iter->first;
    }

    DEBUG("Username %s or server %s doesn't match any account, using IP2IP", userName.c_str(), server.c_str());
    return SIPAccount::IP2IP_PROFILE;
}

std::map<std::string, int32_t> ManagerImpl::getAddressbookSettings() const
{
    std::map<std::string, int32_t> settings;

    settings["ADDRESSBOOK_ENABLE"] = addressbookPreference.getEnabled();
    settings["ADDRESSBOOK_MAX_RESULTS"] = addressbookPreference.getMaxResults();
    settings["ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"] = addressbookPreference.getPhoto();
    settings["ADDRESSBOOK_DISPLAY_PHONE_BUSINESS"] = addressbookPreference.getBusiness();
    settings["ADDRESSBOOK_DISPLAY_PHONE_HOME"] = addressbookPreference.getHome();
    settings["ADDRESSBOOK_DISPLAY_PHONE_MOBILE"] = addressbookPreference.getMobile();

    return settings;
}

void ManagerImpl::setAddressbookSettings(const std::map<std::string, int32_t>& settings)
{
    addressbookPreference.setEnabled(settings.find("ADDRESSBOOK_ENABLE")->second == 1);
    addressbookPreference.setMaxResults(settings.find("ADDRESSBOOK_MAX_RESULTS")->second);
    addressbookPreference.setPhoto(settings.find("ADDRESSBOOK_DISPLAY_CONTACT_PHOTO")->second == 1);
    addressbookPreference.setBusiness(settings.find("ADDRESSBOOK_DISPLAY_PHONE_BUSINESS")->second == 1);
    addressbookPreference.setHone(settings.find("ADDRESSBOOK_DISPLAY_PHONE_HOME")->second == 1);
    addressbookPreference.setMobile(settings.find("ADDRESSBOOK_DISPLAY_PHONE_MOBILE")->second == 1);
}

void ManagerImpl::setAddressbookList(const std::vector<std::string>& list)
{
    addressbookPreference.setList(ManagerImpl::join_string(list));
    saveConfig();
}

std::vector<std::string> ManagerImpl::getAddressbookList() const
{
    return split_string(addressbookPreference.getList());
}

void ManagerImpl::setIPToIPForCall(const std::string& callID, bool IPToIP)
{
    if (not isIPToIP(callID)) // no IPToIP calls with the same ID
        IPToIPMap_[callID] = IPToIP;
}

bool ManagerImpl::isIPToIP(const std::string& callID) const
{
    std::map<std::string, bool>::const_iterator iter = IPToIPMap_.find(callID);
    return iter != IPToIPMap_.end() and iter->second;
}

std::map<std::string, std::string> ManagerImpl::getCallDetails(const std::string &callID)
{
    // We need here to retrieve the call information attached to the call ID
    // To achieve that, we need to get the voip link attached to the call
    // But to achieve that, we need to get the account the call was made with

    // So first we fetch the account
    const std::string accountid(getAccountFromCall(callID));

    // Then the VoIP link this account is linked with (IAX2 or SIP)
    Call *call = getCallFromCallID(callID);

    if (call) {
        std::map<std::string, std::string> details(call->getDetails());
        details["ACCOUNTID"] = accountid;
        return details;
    } else {
        ERROR("Call is NULL");
        // FIXME: is this even useful?
        return Call::getNullDetails();
    }
}

std::vector<std::map<std::string, std::string> > ManagerImpl::getHistory()
{
    return history_.getSerialized();
}

namespace {
template <typename M, typename V>
void vectorFromMapKeys(const M &m, V &v)
{
    for (typename M::const_iterator it = m.begin(); it != m.end(); ++it)
        v.push_back(it->first);
}
}

std::vector<std::string> ManagerImpl::getCallList() const
{
    std::vector<std::string> v;
    vectorFromMapKeys(callAccountMap_, v);
    return v;
}

std::map<std::string, std::string> ManagerImpl::getConferenceDetails(
    const std::string& confID) const
{
    std::map<std::string, std::string> conf_details;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        conf_details["CONFID"] = confID;
        conf_details["CONF_STATE"] = iter_conf->second->getStateStr();
    }

    return conf_details;
}

std::vector<std::string> ManagerImpl::getConferenceList() const
{
    std::vector<std::string> v;
    vectorFromMapKeys(conferenceMap_, v);
    return v;
}

std::vector<std::string> ManagerImpl::getParticipantList(const std::string& confID) const
{
    std::vector<std::string> v;
    ConferenceMap::const_iterator iter_conf = conferenceMap_.find(confID);

    if (iter_conf != conferenceMap_.end()) {
        const ParticipantSet participants(iter_conf->second->getParticipantList());
        std::copy(participants.begin(), participants.end(), std::back_inserter(v));;
    } else
        WARN("Did not find conference %s", confID.c_str());

    return v;
}

std::string ManagerImpl::getConferenceId(const std::string& callID)
{
    Call *call = getCallFromCallID(callID);
    if (call == NULL) {
        ERROR("Call is NULL");
        return "";
    }

    return call->getConfId();
}

void ManagerImpl::saveHistory()
{
    if (!history_.save())
        ERROR("Could not save history!");
#if HAVE_DBUS
    else
        dbus_.getConfigurationManager()->historyChanged();
#endif
}

void ManagerImpl::clearHistory()
{
    history_.clear();
}
