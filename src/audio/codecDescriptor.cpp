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
  _debug("Destroy codecs handles\n");
  int i;
  for( i = 0 ; i < _CodecInMemory.size() ; i++)
  {
    unloadCodec( _CodecInMemory[i] );
  }

}

  void
CodecDescriptor::init()
{
  _debug("Scanning %s to find audio codecs....\n",  CODECS_DIR);
  std::vector<AudioCodec*> CodecDynamicList = scanCodecDirectory();
  _nbCodecs = CodecDynamicList.size();
  if( _nbCodecs <= 0 ){
    _debug("Error - No codecs available in directory %s\n", CODECS_DIR);
    exit(0);
  }

  int i;
  for( i = 0 ; i < _nbCodecs ; i++ ) {
    _CodecsMap[(CodecType)CodecDynamicList[i]->getPayload()] = CodecDynamicList[i];
    _debug("Dynamic codec = %s\n" , CodecDynamicList[i]->getCodecName().c_str());
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
  }
}

  std::string
CodecDescriptor::getCodecName(CodecType payload)
{
  std::string resNull = "";
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) {
    return (iter->second->getCodecName());
  }
  return resNull;
}

  AudioCodec* 
CodecDescriptor::getCodec(CodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) {
    return (iter->second);
  }
  return NULL;
}

  bool 
CodecDescriptor::isActive(CodecType payload) 
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
CodecDescriptor::removeCodec(CodecType payload)
{
  CodecMap::iterator iter = _codecMap.begin();
  while(iter!=_codecMap.end()) {
    if (iter->first == payload) {
      _debug("Codec %s removed from the list", getCodecName(payload).data());
      _codecMap.erase(iter);
      break;
    }
    iter++;
  }

}

  void
CodecDescriptor::addCodec(CodecType payload)
{
}

  double 
CodecDescriptor::getBitRate(CodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getBitRate());
  else
    return 0.0;
}

  double 
CodecDescriptor::getBandwidthPerCall(CodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getBandwidth());
  else
    return 0.0;
}

  int
CodecDescriptor::getSampleRate(CodecType payload)
{
  CodecsMap::iterator iter = _CodecsMap.find(payload);
  if (iter!=_CodecsMap.end()) 
    return (iter->second->getClockRate());
  else
    return 0;
}

  int
CodecDescriptor::getChannel(CodecType payload)
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
    _codecOrder.push_back((CodecType)payload);
    _CodecsMap.find((CodecType)payload)->second->setState( true );
    i++;
  }
}

  std::vector<AudioCodec*>
CodecDescriptor::scanCodecDirectory( void )
{
  std::vector<AudioCodec*> codecs;
  std::string tmp;
  std::string codecDir = CODECS_DIR;
  codecDir.append("/");
  std::string current = ".";
  std::string previous = "..";
  DIR *dir = opendir( codecDir.c_str() );
  AudioCodec* audioCodec;
  if( dir ){
    dirent *dirStruct;
    while( dirStruct = readdir( dir )) {
      tmp =  dirStruct -> d_name ;
      if( tmp == current || tmp == previous){}
      else{	
	_debug("Codec : %s\n", tmp.c_str());
	audioCodec = loadCodec( codecDir.append(tmp) );
	codecs.push_back( audioCodec );
	codecDir = CODECS_DIR;
	codecDir.append("/");
      }
    }
  }
  closedir( dir );
  return codecs;
}

/*CodecDescriptor::getDescription( std::string fileName )
{
  int pos = fileName.length() - 12 ;
  return fileName.substr(9, pos);
}
*/
  AudioCodec*
CodecDescriptor::loadCodec( std::string path )
{
  _debug("Load path %s\n", path.c_str());
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
  //_debug("Add %s in the list.\n" , a->getCodecName().c_str());
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
    

