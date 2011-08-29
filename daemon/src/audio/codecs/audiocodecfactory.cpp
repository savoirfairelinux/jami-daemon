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

#include <iostream>
#include <cstdlib>

#include "audiocodecfactory.h"
#include "fileutils.h"

AudioCodecFactory::AudioCodecFactory() : _CodecsMap(), _defaultCodecOrder(), _Cache(), _CodecInMemory()
{
}

void
AudioCodecFactory::init()
{
    std::vector<sfl::Codec*> CodecDynamicList = scanCodecDirectory();
    if (CodecDynamicList.size() == 0)
        _error ("Error - No codecs available");

    for (size_t i = 0 ; i < CodecDynamicList.size() ; i++) {
        _CodecsMap[ (AudioCodecType) CodecDynamicList[i]->getPayloadType() ] = CodecDynamicList[i];
        _debug ("Loaded codec %s" , CodecDynamicList[i]->getMimeSubtype().c_str());
    }
}

void AudioCodecFactory::setDefaultOrder()
{
    _defaultCodecOrder.clear();
    CodecsMap::iterator iter;
    for (iter = _CodecsMap.begin(); iter != _CodecsMap.end(); ++iter)
        _defaultCodecOrder.push_back (iter->first);
}

std::string
AudioCodecFactory::getCodecName (AudioCodecType payload)
{
    CodecsMap::iterator iter = _CodecsMap.find (payload);

    if (iter!=_CodecsMap.end())
        return (iter->second->getMimeSubtype());

    return "";
}

sfl::Codec*
AudioCodecFactory::getCodec (AudioCodecType payload)
{
    CodecsMap::iterator iter = _CodecsMap.find (payload);

    if (iter != _CodecsMap.end())
        return iter->second;

    _error ("CodecDescriptor: cannot find codec %i", payload);

    return NULL;
}

double AudioCodecFactory::getBitRate (AudioCodecType payload)
{
    CodecsMap::iterator iter = _CodecsMap.find (payload);

    if (iter!=_CodecsMap.end())
        return (iter->second->getBitRate());

    return 0.0;
}


int AudioCodecFactory::getSampleRate (AudioCodecType payload) const
{
    CodecsMap::const_iterator iter = _CodecsMap.find (payload);

    if (iter != _CodecsMap.end())
        return iter->second->getClockRate();

	return 0;
}

void AudioCodecFactory::saveActiveCodecs (const std::vector<std::string>& list)
{
    _defaultCodecOrder.clear();
    // list contains the ordered payload of active codecs picked by the user
    // we used the CodecOrder vector to save the order.

    for (size_t i = 0; i < list.size(); i++) {
        int payload = std::atoi (list[i].data());
        if (isCodecLoaded (payload))
            _defaultCodecOrder.push_back ( (AudioCodecType) payload);
    }
}

void
AudioCodecFactory::deleteHandlePointer (void)
{
    for (std::vector<CodecHandlePointer>::const_iterator iter =
            _CodecInMemory.begin(); iter != _CodecInMemory.end(); ++iter)
        unloadCodec (*iter);

    _CodecInMemory.clear();
}

std::vector<sfl::Codec*> AudioCodecFactory::scanCodecDirectory (void)
{
    std::vector<sfl::Codec*> codecs;
    std::vector<std::string> dirToScan;

    dirToScan.push_back (std::string(HOMEDIR) + DIR_SEPARATOR_STR "." PROGDIR "/");
    dirToScan.push_back (CODECS_DIR "/");
    const char *envDir = getenv("CODECS_PATH");
    if (envDir)
        dirToScan.push_back(std::string(envDir) + DIR_SEPARATOR_STR);
    const char *progDir = get_program_dir();
    if (progDir)
        dirToScan.push_back(std::string(progDir) + DIR_SEPARATOR_STR + "audio/codecs/");

    for (size_t i = 0 ; i < dirToScan.size() ; i++) {
        std::string dirStr = dirToScan[i];
        _debug ("CodecDescriptor: Scanning %s to find audio codecs....",  dirStr.c_str());

        DIR *dir = opendir (dirStr.c_str());
        if (!dir)
            continue;

        dirent *dirStruct;
        while ( (dirStruct = readdir (dir))) {
            std::string file = dirStruct->d_name ;
            if (file == CURRENT_DIR or file == PARENT_DIR)
                continue;

            if (seemsValid (file) && !alreadyInCache (file)) {
                sfl::Codec* audioCodec = loadCodec (dirStr+file);
                if (audioCodec) {
                    codecs.push_back (audioCodec);
                    _Cache.push_back (file);
                }
            }
        }

        closedir (dir);
    }

    return codecs;
}

sfl::Codec* AudioCodecFactory::loadCodec (std::string path)
{
    void * codecHandle = dlopen (path.c_str() , RTLD_LAZY);
    if (!codecHandle) {
    	_error("%s\n", dlerror());
    	return NULL;
    }

    dlerror();

    create_t* createCodec = (create_t*) dlsym (codecHandle , "create");
    char *error = dlerror();
    if (error) {
    	_error("%s\n", error);
    	return NULL;
    }

    sfl::Codec* a = createCodec();

    _CodecInMemory.push_back (CodecHandlePointer (a, codecHandle));

    return a;
}


void AudioCodecFactory::unloadCodec (CodecHandlePointer p)
{
    destroy_t* destroyCodec = (destroy_t*) dlsym (p.second , "destroy");

    char *error = dlerror();
    if (error) {
    	_error("%s\n", error);
    	return;
    }

    destroyCodec (p.first);

    dlclose (p.second);
}

sfl::Codec* AudioCodecFactory::instantiateCodec (AudioCodecType payload)
{
    std::vector< CodecHandlePointer >::iterator iter;

    for (iter = _CodecInMemory.begin(); iter != _CodecInMemory.end(); ++iter) {
        if (iter->first->getPayloadType() == payload) {
            create_t* createCodec = (create_t*) dlsym (iter->second , "create");

            char *error = dlerror();
            if (error)
            	_error("%s\n", error);
            else
				return createCodec();
        }
    }

    return NULL;
}



sfl::Codec* AudioCodecFactory::getFirstCodecAvailable (void)
{
    CodecsMap::iterator iter = _CodecsMap.begin();

    if (iter != _CodecsMap.end())
        return iter->second;
    else
        return NULL;
}

bool AudioCodecFactory::seemsValid (std::string lib)
{
    // The name of the shared library seems valid  <==> it looks like libcodec_xxx.so
    // We check this
    std::string prefix = SFL_CODEC_VALID_PREFIX;
    std::string suffix = SFL_CODEC_VALID_EXTEN;

    ssize_t len = lib.length() - prefix.length() - suffix.length();
    if (len < 0)
        return false;

    // Second: check the extension of the file name.
    // If it is different than SFL_CODEC_VALID_EXTEN , not a SFL shared library
    if (lib.substr (lib.length() - suffix.length() , lib.length()) != suffix)
        return false;


#ifndef HAVE_SPEEX_CODEC
    if (lib.substr (prefix.length() , len) == SPEEX_STRING_DESCRIPTION)
        return false;
#endif

#ifndef HAVE_GSM_CODEC
    if (lib.substr (prefix.length() , len) == GSM_STRING_DESCRIPTION)
        return false;
#endif

#ifndef BUILD_ILBC
    if (lib.substr (prefix.length() , len) == ILBC_STRING_DESCRIPTION)
        return false;
#endif

    if (lib.substr (0, prefix.length()) == prefix)
        if (lib.substr (lib.length() - suffix.length() , suffix.length()) == suffix)
            return true;

	return false;
}

bool
AudioCodecFactory::alreadyInCache (std::string lib)
{
    for (size_t i = 0 ; i < _Cache.size() ; i++)
        if (_Cache[i] == lib)
            return true;

    return false;
}

bool AudioCodecFactory::isCodecLoaded (int payload)
{
	CodecsMap::iterator iter;
	for (iter = _CodecsMap.begin(); iter != _CodecsMap.end(); ++iter)
        if (iter -> first == payload)
            return true;

    return false;
}

std::vector <std::string> AudioCodecFactory::getCodecSpecifications (const int32_t& payload)
{
    std::vector<std::string> v;
    std::stringstream ss;

    // Add the name of the codec
    v.push_back (getCodecName ( (AudioCodecType) payload));

    // Add the sample rate
    ss << getSampleRate ( (AudioCodecType) payload);
    v.push_back ( (ss.str()).data());
    ss.str ("");

    // Add the bit rate
    ss << getBitRate ( (AudioCodecType) payload);
    v.push_back ( (ss.str()).data());
    ss.str ("");

    return v;
}
