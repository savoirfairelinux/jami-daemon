/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include <stdexcept>

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
        for (const auto &codec: codecDynamicList) {
            codecsMap_[(int) codec->getPayloadType()] = codec;
            DEBUG("Loaded codec %s" , codec->getMimeSubtype().c_str());
        }
    }
}

void
AudioCodecFactory::setDefaultOrder()
{
    defaultCodecList_.clear();

    for (const auto &codec : codecsMap_)
        defaultCodecList_.push_back(codec.first);
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

    for (const auto &codec : codecsMap_)
        if (codec.second)
            list.push_back((int32_t) codec.first);

    return list;
}

sfl::AudioCodec*
AudioCodecFactory::getCodec(int payload) const
{
    AudioCodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second;
    else {
        ERROR("Cannot find codec %i", payload);
        return NULL;
    }
}

sfl::AudioCodec*
AudioCodecFactory::getCodec(const std::string &name) const
{
    for (const auto iter : codecsMap_) {
        std::ostringstream os;
        const std::string channels(iter.second->getSDPChannels());
        os << "/" << iter.second->getSDPClockRate();
        if (not channels.empty())
            os << "/" << channels;

        const std::string match(iter.second->getMimeSubtype() + os.str());
        DEBUG("Trying %s", match.c_str());
        if (name.find(match) != std::string::npos) {
            DEBUG("Found match");
            return iter.second;
        }
    }

    ERROR("Cannot find codec %s", name.c_str());
    return NULL;
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

    for (const auto &codec : list) {
        int payload = std::atoi(codec.c_str());

        if (isCodecLoaded(payload))
            defaultCodecList_.push_back(static_cast<int>(payload));
    }
}


AudioCodecFactory::~AudioCodecFactory()
{
    for (auto &codec : codecInMemory_)
        unloadCodec(codec);
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

    if (progDir) {
#ifdef __ANDROID__
        dirToScan.push_back(std::string(progDir) + DIR_SEPARATOR_STR + "lib/");
#else
        dirToScan.push_back(std::string(progDir) + DIR_SEPARATOR_STR + "audio/codecs/");
#endif
	}

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
    // Clear any existing error
    dlerror();

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

    try {
        sfl::AudioCodec *a = createCodec();

        if (a)
            codecInMemory_.push_back(AudioCodecHandlePointer(a, codecHandle));
        else
            dlclose(codecHandle);

        return a;
    } catch (const std::runtime_error &e) {
        ERROR("%s", e.what());
        dlclose(codecHandle);
        return nullptr;
    }
}


void
AudioCodecFactory::unloadCodec(AudioCodecHandlePointer &ptr)
{
    destroy_t *destroyCodec = 0;
    // flush last error
    dlerror();

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
    for (const auto &codec : codecInMemory_) {
        if (codec.first->getPayloadType() == payload) {
            try {
                return codec.first->clone();
            } catch (const std::runtime_error &e) {
                ERROR("%s", e.what());
                return nullptr;
            }
        }
    }

    return nullptr;
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
        ""
    };

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
    for (const auto &codec : codecsMap_)
        if (codec.first == payload)
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
