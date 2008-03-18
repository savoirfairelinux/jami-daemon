/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include <iostream>
#include <cstdlib>

#include "codecDescriptor.h"

CodecDescriptor::CodecDescriptor() 
{
  //init();
  //#ifdef HAVE_SPEEX
  //_codecMap[PAYLOAD_CODEC_SPEEX] = new CodecSpeex(PAYLOAD_CODEC_SPEEX); // TODO: this is a variable payload!
  //#endif
}

CodecDescriptor::~CodecDescriptor()
{
}

  void
CodecDescriptor::deleteHandlePointer( void )
{
  int i;
  for( i = 0 ; i < _CodecInMemory.size() ; i++)
  {
    unloadCodec( _CodecInMemory[i] );
  }

}

  void
CodecDescriptor::init()
{
  std::vector<AudioCodec*> CodecDynamicList = scanCodecDirectory();
  _nbCodecs = CodecDynamicList.size();
  if( _nbCodecs <= 0 ){
    _debug(" Error - No codecs available in directory %s\n" , CODECS_DIR);
    //exit(0);
  }

  int i;
  for( i = 0 ; i < _nbCodecs ; i++ ) {
    _CodecsMap[(AudioCodecType)CodecDynamicList[i]->getPayload()] = CodecDynamicList[i];
    _debug("%s\n" , CodecDynamicList[i]->getCodecName().c_str());
  }
}

  void
CodecDescriptor::setDefaultOrder()
{
  _codecOrder.clear();
  CodecsMap::iterator iter = _CodecsMap.begin();
  while( iter != _CodecsMap.end())
  {
    _codecOrder.push_back(iter->first);
    iter->second->setState( true );
    iter ++ ;
  }
}

  std::string
CodecDescriptor::getCodecName(AudioCodecType payload)
{
  std::string resNull = "";
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) {
    return (iter->second->getCodecName());
  }
  return resNull;
}

  AudioCodec* 
CodecDescriptor::getCodec(AudioCodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) {
    return (iter->second);
  }
  return NULL;
}

  bool 
CodecDescriptor::isActive(AudioCodecType payload) 
{
  int i;
  for(i=0 ; i < _codecOrder.size() ; i++)
  {
    if(_codecOrder[i] == payload)
      return true;
  }
  return false;
}

  void 
CodecDescriptor::removeCodec(AudioCodecType payload)
{
}

  void
CodecDescriptor::addCodec(AudioCodecType payload)
{
}

  double 
CodecDescriptor::getBitRate(AudioCodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getBitRate());
  else
    return 0.0;
}

  double 
CodecDescriptor::getBandwidthPerCall(AudioCodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getBandwidth());
  else
    return 0.0;
}

  int
CodecDescriptor::getSampleRate(AudioCodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getClockRate());
  else
    return 0;
}

  int
CodecDescriptor::getChannel(AudioCodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getChannel());
  else
    return 0;
}

  void
CodecDescriptor::saveActiveCodecs(const std::vector<std::string>& list)
{
  _codecOrder.clear();
  // list contains the ordered payload of active codecs picked by the user
  // we used the CodecOrder vector to save the order.
  int i=0;
  int payload;
  size_t size = list.size();
  while( i < size )
  {
    payload = std::atoi(list[i].data());
    if( isCodecLoaded( payload ) ) {
      _codecOrder.push_back((AudioCodecType)payload);
      _CodecsMap.find((AudioCodecType)payload)->second->setState( true );
    }
    i++;
  }
}

  std::vector<AudioCodec*>
CodecDescriptor::scanCodecDirectory( void )
{
  std::vector<AudioCodec*> codecs;
  std::string tmp;
  int i;

  std::string libDir = std::string(CODECS_DIR).append("/");
  std::string homeDir = std::string(HOMEDIR)  + DIR_SEPARATOR_STR + "." + PROGDIR + "/";
  std::vector<std::string> dirToScan;
  dirToScan.push_back(homeDir);
  dirToScan.push_back(libDir);

  for( i = 0 ; i < dirToScan.size() ; i++ )
  {
    std::string dirStr = dirToScan[i];
    _debug("Scanning %s to find audio codecs....\n",  dirStr.c_str());
    DIR *dir = opendir( dirStr.c_str() );
    AudioCodec* audioCodec;
    if( dir ){
      dirent *dirStruct;
      while( dirStruct = readdir( dir )) {
	tmp =  dirStruct -> d_name ;
	if( tmp == CURRENT_DIR || tmp == PARENT_DIR){}
	else{	
	  if( seemsValid( tmp ) && !alreadyInCache( tmp ))
	  {
	    //_debug("Codec : %s\n", tmp.c_str());
	    _Cache.push_back( tmp );
	    audioCodec = loadCodec( dirStr.append(tmp) );
	    codecs.push_back( audioCodec );
	    dirStr = dirToScan[i];
	  }
	}
      }
    }
    closedir( dir );
  }
  return codecs;
}

  AudioCodec*
CodecDescriptor::loadCodec( std::string path )
{
  //_debug("Load path %s\n", path.c_str());
  CodecHandlePointer p;
  using std::cerr;
  void * codecHandle = dlopen( path.c_str() , RTLD_LAZY );
  if( !codecHandle )
    cerr << dlerror() << '\n';
  dlerror();
  create_t* createCodec = (create_t*)dlsym( codecHandle , "create" );
  if( dlerror() )
    cerr << dlerror() << '\n';
  AudioCodec* a = createCodec();
  p = CodecHandlePointer( a, codecHandle );
  _CodecInMemory.push_back(p);

  return a;
}

  void
CodecDescriptor::unloadCodec( CodecHandlePointer p )
{
  // _debug("Unload codec %s\n", p.first->getCodecName().c_str());
  using std::cerr;
  int i;
  destroy_t* destroyCodec = (destroy_t*)dlsym( p.second , "destroy");
  if(dlerror())
    cerr << dlerror() << '\n';
  destroyCodec(p.first);
  dlclose(p.second);
}

  AudioCodec*
CodecDescriptor::getFirstCodecAvailable( void )
{
  CodecsMap::iterator iter = _CodecsMap.begin();
  if( iter != _CodecsMap.end())
    return iter->second;
  else
    return NULL;
}

  bool
CodecDescriptor::seemsValid( std::string lib)
{
  // The name of the shared library seems valid  <==> it looks like libcodec_xxx.so
  // We check this  
  std::string begin = SFL_CODEC_VALID_PREFIX;
  std::string end = SFL_CODEC_VALID_EXTEN;

#ifdef BUILD_SPEEX
  // Nothing special
#else
    if( lib.substr(begin.length() , lib.length() - begin.length() - end.length()) == SPEEX_STRING_DESCRIPTION)
      return false;
#endif

#ifdef BUILD_GSM
  // Nothing special
#else
    if( lib.substr(begin.length() , lib.length() - begin.length() - end.length()) == GSM_STRING_DESCRIPTION )  
      return false;
#endif

#ifdef BUILD_ILBC
  // Nothing special
#else
    if( lib.substr(begin.length() , lib.length() - begin.length() - end.length()) == ILBC_STRING_DESCRIPTION )  
      return false;
#endif

  if(lib.substr(0, begin.length()) == begin)
    if(lib.substr(lib.length() - end.length() , end.length() ) == end)
      return true;
    else
      return false;
  else
    return false;
}

  bool
CodecDescriptor::alreadyInCache( std::string lib )
{
  int i;
  for( i = 0 ; i < _Cache.size() ; i++ )
  {
    if( _Cache[i] == lib ){
      return true;}
  }
  return false;
}

bool
CodecDescriptor::isCodecLoaded( int payload )
{
  int i;
  CodecsMap::iterator iter = _CodecsMap.begin();
  while( iter != _CodecsMap.end())
  {
    if( iter -> first == payload)
      return true;
    iter++;
  }
  return false;
}




