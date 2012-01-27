/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "config.h"
#include "audiocodecfactory.h"
#include <cstdlib>
#include <algorithm> // for std::find
#include "fileutils.h"

AudioCodecFactory::AudioCodecFactory() :
    codecsMap_(), defaultCodecList_(), libCache_(), codecInMemory_()
{
    typedef std::vector<sfl::Codec*> CodecVector;
    CodecVector codecDynamicList(scanCodecDirectory());

    if (codecDynamicList.empty())
        ERROR("Error - No codecs available");
    else {
        for (CodecVector::const_iterator i = codecDynamicList.begin();
             i != codecDynamicList.end() ; ++i) {
            codecsMap_[(int)(*i)->getPayloadType()] = *i;
            DEBUG("Loaded codec %s" , (*i)->getMimeSubtype().c_str());
        }
    }
}

void AudioCodecFactory::setDefaultOrder()
{
    defaultCodecList_.clear();
    for (CodecsMap::const_iterator i = codecsMap_.begin(); i != codecsMap_.end(); ++i)
        defaultCodecList_.push_back(i->first);
}

std::string
AudioCodecFactory::getCodecName(int payload) const
{
    CodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getMimeSubtype();
    else
        return "";
}

std::vector<int32_t >
AudioCodecFactory::getAudioCodecList() const
{
    std::vector<int32_t> list;
    int size = codecsMap_.size();
    printf("%d\n",size);
    for (CodecsMap::const_iterator i = codecsMap_.begin(); i != codecsMap_.end(); ++i)
        if (i->second)
            list.push_back((int32_t)i->first);

    return list;
}

sfl::Codec*
AudioCodecFactory::getCodec(int payload) const
{
    CodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second;
    else {
        ERROR("CodecDescriptor: cannot find codec %i", payload);
        return NULL;
    }
}

double AudioCodecFactory::getBitRate(int payload) const
{
    CodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter != codecsMap_.end())
        return iter->second->getBitRate();
    else
        return 0.0;
}

int AudioCodecFactory::getSampleRate(int payload) const
{
    CodecsMap::const_iterator iter = codecsMap_.find(payload);

    if (iter!=codecsMap_.end())
        return (iter->second->getClockRate() * 0.001);
	return 0;
}

void AudioCodecFactory::saveActiveCodecs(const std::vector<std::string>& list)
{
    defaultCodecList_.clear();
    // list contains the ordered payload of active codecs picked by the user
    // we used the CodecList vector to save the order.

    for (std::vector<std::string>::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
        int payload = std::atoi(iter->c_str());

        if (isCodecLoaded(payload))
            defaultCodecList_.push_back(static_cast<int>(payload));
    }
}


AudioCodecFactory::~AudioCodecFactory()
{
    for (std::vector<CodecHandlePointer>::const_iterator i =
         codecInMemory_.begin(); i != codecInMemory_.end(); ++i)
        unloadCodec(*i);
}

std::vector<sfl::Codec*> AudioCodecFactory::scanCodecDirectory()
{
    std::vector<sfl::Codec*> codecs;
    std::vector<std::string> dirToScan;

    dirToScan.push_back(std::string(HOMEDIR) + DIR_SEPARATOR_STR "." PACKAGE "/");
    dirToScan.push_back(CODECS_DIR "/");
    const char *envDir = getenv("CODECS_PATH");

    if (envDir)
        dirToScan.push_back(std::string(envDir) + DIR_SEPARATOR_STR);

    const char *progDir = fileutils::get_program_dir();

    if (progDir)
        dirToScan.push_back(std::string(progDir) + DIR_SEPARATOR_STR + "audio/codecs/");

    for (size_t i = 0 ; i < dirToScan.size() ; i++) {
        std::string dirStr = dirToScan[i];
        DEBUG("CodecDescriptor: Scanning %s to find audio codecs....",  dirStr.c_str());

        DIR *dir = opendir(dirStr.c_str());

        if (!dir)
            continue;

        dirent *dirStruct;

        while ((dirStruct = readdir(dir))) {
            std::string file = dirStruct->d_name ;

            if (file == "." or file == "..")
                continue;

            if (seemsValid(file) && !alreadyInCache(file)) {
                sfl::Codec* audioCodec = loadCodec(dirStr+file);

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

sfl::Codec* AudioCodecFactory::loadCodec(const std::string &path)
{
    void * codecHandle = dlopen(path.c_str() , RTLD_LAZY);

    if (!codecHandle) {
        ERROR("%s\n", dlerror());
        return NULL;
    }

    dlerror();

    create_t* createCodec = (create_t*) dlsym(codecHandle , CODEC_ENTRY_SYMBOL);
    char *error = dlerror();

    if (error) {
        ERROR("%s\n", error);
        return NULL;
    }

    sfl::Codec* a = createCodec();

    codecInMemory_.push_back(CodecHandlePointer(a, codecHandle));

    return a;
}


void AudioCodecFactory::unloadCodec(CodecHandlePointer p)
{
    destroy_t* destroyCodec = (destroy_t*) dlsym(p.second , "destroy");

    char *error = dlerror();

    if (error) {
        ERROR("%s\n", error);
        return;
    }

    destroyCodec(p.first);

    dlclose(p.second);
}

sfl::Codec* AudioCodecFactory::instantiateCodec(int payload) const
{
    for (std::vector<CodecHandlePointer>::const_iterator i = codecInMemory_.begin(); i != codecInMemory_.end(); ++i) {
        if (i->first->getPayloadType() == payload) {
            create_t* createCodec = (create_t*) dlsym(i->second , CODEC_ENTRY_SYMBOL);

            char *error = dlerror();

            if (error)
                ERROR("%s\n", error);
            else
                return createCodec();
        }
    }

    return NULL;
}

bool AudioCodecFactory::seemsValid(const std::string &lib)
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
    if (lib.substr(lib.length() - suffix.length() , lib.length()) != suffix)
        return false;


#ifndef HAVE_SPEEX_CODEC

    if (lib.substr(prefix.length() , len) == "speex")
        return false;

#endif

#ifndef HAVE_GSM_CODEC

    if (lib.substr(prefix.length() , len) == "gsm")
        return false;

#endif

#ifndef BUILD_ILBC

    if (lib.substr(prefix.length() , len) == "ilbc")
        return false;

#endif

    if (lib.substr(0, prefix.length()) == prefix)
        if (lib.substr(lib.length() - suffix.length() , suffix.length()) == suffix)
            return true;

    return false;
}

bool
AudioCodecFactory::alreadyInCache(const std::string &lib)
{
    return std::find(libCache_.begin(), libCache_.end(), lib) != libCache_.end();
}

bool AudioCodecFactory::isCodecLoaded(int payload) const
{
    for (CodecsMap::const_iterator i = codecsMap_.begin(); i != codecsMap_.end(); ++i)
        if (i->first == payload)
            return true;

    return false;
}

std::vector <std::string> AudioCodecFactory::getCodecSpecifications(const int32_t& payload) const
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

    return v;
}
