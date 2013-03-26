/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstdlib>
#include <dlfcn.h>
#include <algorithm> // for std::find
#include <dlfcn.h>

#include "audiocodec.h"
#include "audiocodecfactory.h"
#include "fileutils.h"
#include "array_size.h"
#include "logger.h"

AudioCodecFactory::AudioCodecFactory() :
    codecsMap_(), defaultCodecList_(), libCache_(), codecInMemory_()
{
    typedef std::vector<sfl::AudioCodec*> AudioCodecVector;
    AudioCodecVector codecDynamicList(scanCodecDirectory());

    if (codecDynamicList.empty())
        ERROR("No codecs available");
    else {
        for (AudioCodecVector::const_iterator iter = codecDynamicList.begin(); iter != codecDynamicList.end() ; ++iter) {
            codecsMap_[(int)(*iter)->getPayloadType()] = *iter;
            DEBUG("Loaded codec %s" , (*iter)->getMimeSubtype().c_str());
        }
    }
}

void
AudioCodecFactory::setDefaultOrder()
{
    defaultCodecList_.clear();
    for (AudioCodecsMap::const_iterator i = codecsMap_.begin(); i != codecsMap_.end(); ++i)
        defaultCodecList_.push_back(i->first);
}

std::string
AudioCodecFactory::getCodecName(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getMimeSubtype();
    else
        return "";
}

std::vector<int32_t>
AudioCodecFactory::getCodecList() const
{
    std::vector<int32_t> list;
    for (AudioCodecsMap::const_iterator iter = codecsMap_.begin(); iter != codecsMap_.end(); ++iter)
        if (iter->second)
            list.push_back((int32_t) iter->first);

    return list;
}

sfl::AudioCodec*
AudioCodecFactory::getCodec(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return static_cast<sfl::AudioCodec *>(iter->second);
    else {
        ERROR("Cannot find codec %i", payload);
        return NULL;
    }
}

double
AudioCodecFactory::getBitRate(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getBitRate();
    else
        return 0.0;
}


int
AudioCodecFactory::getSampleRate(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getClockRate();
    else
        return 0;
}

unsigned
AudioCodecFactory::getChannels(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getChannels();
    else
        return 0;
}

void
AudioCodecFactory::saveActiveCodecs(const std::vector<std::string>& list)
{
    defaultCodecList_.clear();
    // list contains the ordered payload of active codecs picked by the user
    // we used the codec vector to save the order.

    for (std::vector<std::string>::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
        int payload = std::atoi(iter->c_str());

        if (isCodecLoaded(payload))
            defaultCodecList_.push_back(static_cast<int>(payload));
    }
}


AudioCodecFactory::~AudioCodecFactory()
{
    for (std::vector<AudioCodecHandlePointer>::iterator iter =
         codecInMemory_.begin(); iter != codecInMemory_.end(); ++iter)
        unloadCodec(*iter);
}

std::vector<sfl::AudioCodec*>
AudioCodecFactory::scanCodecDirectory()
{
    std::vector<sfl::AudioCodec*> codecs;
    std::vector<std::string> dirToScan;

    dirToScan.push_back(fileutils::get_home_dir() + DIR_SEPARATOR_STR "." PACKAGE "/");
    dirToScan.push_back(CODECS_DIR "/");
    const char *envDir = getenv("CODECS_PATH");

    if (envDir)
        dirToScan.push_back(std::string(envDir) + DIR_SEPARATOR_STR);

    const char *progDir = fileutils::get_program_dir();

    if (progDir)
        dirToScan.push_back(std::string(progDir) + DIR_SEPARATOR_STR + "audio/codecs/");

    for (size_t i = 0 ; i < dirToScan.size() ; i++) {
        std::string dirStr = dirToScan[i];
        DEBUG("Scanning %s to find audio codecs....",  dirStr.c_str());

        DIR *dir = opendir(dirStr.c_str());

        if (!dir)
            continue;

        dirent *dirStruct;

        while ((dirStruct = readdir(dir))) {
            std::string file = dirStruct->d_name ;

            if (file == "." or file == "..")
                continue;

            if (seemsValid(file) && !alreadyInCache(file)) {
                sfl::AudioCodec *audioCodec(loadCodec(dirStr + file));

                if (audioCodec) {
                    codecs.push_back(audioCodec);
                    libCache_.push_back(file);
                }
            }
        }

        closedir(dir);
    }

    return codecs;
}

sfl::AudioCodec *
AudioCodecFactory::loadCodec(const std::string &path)
{
    void * codecHandle = dlopen(path.c_str(), RTLD_NOW);

    if (!codecHandle) {
        ERROR("%s", dlerror());
        return NULL;
    }

    create_t* createCodec = (create_t*) dlsym(codecHandle, AUDIO_CODEC_ENTRY_SYMBOL);
    const char *error = dlerror();

    if (error) {
        ERROR("%s", error);
        dlclose(codecHandle);
        return NULL;
    }

    sfl::AudioCodec *a = static_cast<sfl::AudioCodec *>(createCodec());
    if (a)
        codecInMemory_.push_back(AudioCodecHandlePointer(a, codecHandle));

    return a;
}


void
AudioCodecFactory::unloadCodec(AudioCodecHandlePointer &ptr)
{
    destroy_t *destroyCodec = 0;
    if (ptr.second)
        destroyCodec = (destroy_t*) dlsym(ptr.second, "destroy");

    const char *error = dlerror();

    if (error) {
        ERROR("%s", error);
        return;
    }

    if (ptr.first and destroyCodec)
        destroyCodec(ptr.first);

    if (ptr.second)
        dlclose(ptr.second);
}

sfl::AudioCodec*
AudioCodecFactory::instantiateCodec(int payload) const
{
    std::vector<AudioCodecHandlePointer>::const_iterator iter;

    sfl::AudioCodec *result = NULL;
    for (iter = codecInMemory_.begin(); iter != codecInMemory_.end(); ++iter) {
        if (iter->first->getPayloadType() == payload) {
            create_t* createCodec = (create_t*) dlsym(iter->second , AUDIO_CODEC_ENTRY_SYMBOL);

            const char *error = dlerror();

            if (error)
                ERROR("%s", error);
            else
                result = static_cast<sfl::AudioCodec *>(createCodec());
        }
    }

    return result;
}

bool
AudioCodecFactory::seemsValid(const std::string &lib)
{
    // The name of the shared library seems valid  <==> it looks like libcodec_xxx.so
    // We check this
    static const std::string prefix("libcodec_");
    static const std::string suffix(".so");

    ssize_t len = lib.length() - prefix.length() - suffix.length();

    if (len < 0)
        return false;

    // Second: check the extension of the file name.
    // If it is different than SFL_CODEC_VALID_EXTEN , not a SFL shared library
    if (lib.substr(lib.length() - suffix.length(), lib.length()) != suffix)
        return false;

    static const std::string validCodecs[] = {
    "ulaw",
    "alaw",
    "g722",
    "g729", //G729 have to be loaded first, if it is valid or not is checked later
    "opus", //Opus have to be loaded first, if it is valid or not is checked later
    "opus_stereo",
#ifdef HAVE_SPEEX_CODEC
    "speex_nb",
    "speex_wb",
    "speex_ub",
#endif

#ifdef HAVE_GSM_CODEC
    "gsm",
#endif

#ifdef BUILD_ILBC
    "ilbc",
#endif
    ""};

    const std::string name(lib.substr(prefix.length(), len));
    const std::string *end = validCodecs + ARRAYSIZE(validCodecs);

    return find(validCodecs, end, name) != end;
}

bool
AudioCodecFactory::alreadyInCache(const std::string &lib)
{
    return std::find(libCache_.begin(), libCache_.end(), lib) != libCache_.end();
}

bool
AudioCodecFactory::isCodecLoaded(int payload) const
{
    AudioCodecsMap::const_iterator iter;

    for (iter = codecsMap_.begin(); iter != codecsMap_.end(); ++iter)
        if (iter->first == payload)
            return true;

    return false;
}

std::vector <std::string>
AudioCodecFactory::getCodecSpecifications(const int32_t& payload) const
{
    std::vector<std::string> v;
    std::stringstream ss;

    // Add the name of the codec
    v.push_back(getCodecName(static_cast<int>(payload)));

    // Add the sample rate
    ss << getSampleRate(static_cast<int>(payload));
    v.push_back(ss.str());
    ss.str("");

    // Add the bit rate
    ss << getBitRate(static_cast<int>(payload));
    v.push_back(ss.str());
    ss.str("");

    // Add the channel number
    ss << getChannels(static_cast<int>(payload));
    v.push_back(ss.str());

    return v;
}
