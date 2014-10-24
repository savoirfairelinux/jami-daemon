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

#include "audiocodecfactory.h"
#include "audiocodec.h"
#include "plugin_manager.h"
#include "fileutils.h"
#include "array_size.h"
#include "logger.h"

#include <dirent.h>
#include <dlfcn.h>

#include <cstdlib>
#include <algorithm> // for std::find
#include <stdexcept>
#include <sstream>

AudioCodecFactory::AudioCodecFactory(PluginManager& pluginManager)
    : pluginManager_(pluginManager)
{
    /* Plugin has a C binding, this lambda is used to make the brigde
     * with our C++ binding by providing 'this' access.
     */
    const auto callback = [this](void* data) {
        if (auto codec = reinterpret_cast<sfl::AudioCodec*>(data)) {
            this->registerAudioCodec(codec);
            return 0;
        }
        return -1;
    };

    pluginManager_.registerService("registerAudioCodec", callback);

    scanCodecDirectory();
    if (codecsMap_.empty())
        SFL_ERR("No codecs available");
}

AudioCodecFactory::~AudioCodecFactory()
{
    pluginManager_.unRegisterService("registerAudioCodec");
}

void
AudioCodecFactory::registerAudioCodec(sfl::AudioCodec* codec)
{
    codecsMap_[(int) codec->getPayloadType()] = std::shared_ptr<sfl::AudioCodec>(codec);
    SFL_DBG("Loaded codec %s" , codec->getMimeSubtype().c_str());
}

void
AudioCodecFactory::setDefaultOrder()
{
    defaultCodecList_.clear();

    for (const auto& codec : codecsMap_)
        defaultCodecList_.push_back(codec.first);
}

std::string
AudioCodecFactory::getCodecName(Id payload) const
{
    for (const auto& item : codecsMap_) {
        const auto& codec = item.second;
        if (codec and codec->getPayloadType() == payload)
            return codec->getMimeSubtype();
    }
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

std::shared_ptr<sfl::AudioCodec>
AudioCodecFactory::getCodec(int payload) const
{
    const auto iter = codecsMap_.find(payload);
    if (iter != codecsMap_.end())
        return iter->second;
    SFL_ERR("Cannot find codec %i", payload);
    return nullptr;
}

std::shared_ptr<sfl::AudioCodec>
AudioCodecFactory::getCodec(const std::string &name) const
{
    for (const auto& item : codecsMap_) {
        std::ostringstream os;
        const auto& codec = item.second;
        if (!codec)
            continue;

        const std::string channels(codec->getSDPChannels());
        os << "/" << codec->getSDPClockRate();
        if (not channels.empty())
            os << "/" << channels;

        const std::string match(codec->getMimeSubtype() + os.str());
        SFL_DBG("Trying %s", match.c_str());
        if (name.find(match) != std::string::npos) {
            SFL_DBG("Found match");
            return codec;
        }
    }

    SFL_ERR("Cannot find codec %s", name.c_str());
    return nullptr;
}

double
AudioCodecFactory::getBitRate(int payload) const
{
    auto iter = codecsMap_.find(payload);
    if (iter != codecsMap_.end())
        return iter->second->getBitRate();
    return 0.0;
}

int
AudioCodecFactory::getSampleRate(int payload) const
{
    auto iter = codecsMap_.find(payload);
    if (iter != codecsMap_.end())
        return iter->second->getClockRate();
    return 0;
}

unsigned
AudioCodecFactory::getChannels(int payload) const
{
    auto iter = codecsMap_.find(payload);
    if (iter != codecsMap_.end())
        return iter->second->getChannels();
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

void
AudioCodecFactory::scanCodecDirectory()
{
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

    for (const auto& dirStr : dirToScan) {
        SFL_DBG("Scanning %s to find audio codecs....",  dirStr.c_str());

        DIR *dir = opendir(dirStr.c_str());
        if (!dir)
            continue;

        dirent *dirStruct;
        while ((dirStruct = readdir(dir))) {
            std::string file = dirStruct->d_name;

            if (file == "." or file == "..")
                continue;

            if (seemsValid(file))
                pluginManager_.load(dirStr + file);
        }

        closedir(dir);
    }
}

sfl::AudioCodec*
AudioCodecFactory::instantiateCodec(int payload) const
{
    for (const auto& item : codecsMap_) {
        const auto& codec = item.second;
        if (codec->getPayloadType() == payload) {
            try {
                return codec->clone();
            } catch (const std::runtime_error &e) {
                SFL_ERR("%s", e.what());
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
        "speex",
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
    const std::string *end = validCodecs + SFL_ARRAYSIZE(validCodecs);

    return find(validCodecs, end, name) != end;
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
