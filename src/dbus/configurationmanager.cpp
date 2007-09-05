/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 
#include <global.h>
#include <configurationmanager.h>
#include "../manager.h"

const char* ConfigurationManager::SERVER_PATH = "/org/sflphone/SFLPhone/ConfigurationManager";



ConfigurationManager::ConfigurationManager( DBus::Connection& connection )
: DBus::ObjectAdaptor(connection, SERVER_PATH)
{
}

std::map< ::DBus::String, ::DBus::String > 
ConfigurationManager::getAccountDetails( const ::DBus::String& accountID )
{
    _debug("ConfigurationManager::getAccountDetails received\n");

}


void 
ConfigurationManager::addAccount( const std::map< ::DBus::String, ::DBus::String >& details )
{
    _debug("ConfigurationManager::addAccount received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getAccountList(  )
{
    _debug("ConfigurationManager::getAccountList received\n");
    return Manager::instance().getAccountList();
    
}


void 
ConfigurationManager::setSTUN( const std::map< ::DBus::String, ::DBus::String >& details )
{
    _debug("ConfigurationManager::setSTUN received\n");

}


std::map< ::DBus::String, ::DBus::String > 
ConfigurationManager::getSTUN(  )
{
    _debug("ConfigurationManager::getSTUN received\n");

}


void 
ConfigurationManager::setPlayTonesLocally( const ::DBus::Bool& flag )
{
    _debug("ConfigurationManager::setPlayTonesLocally received\n");

}


::DBus::Bool 
ConfigurationManager::getPlayTonesLocally(  )
{
    _debug("ConfigurationManager::getPlayTonesLocally received\n");

}


void 
ConfigurationManager::setTonePulseLenght( const ::DBus::Int32& milliseconds )
{
    _debug("ConfigurationManager::setTonePulseLenght received\n");

}


::DBus::Int32 
ConfigurationManager::getTonePulseLenght(  )
{
    _debug("ConfigurationManager::getTonePulseLenght received\n");

}


void 
ConfigurationManager::getToneLocaleList( const std::vector< ::DBus::String >& list )
{
    _debug("ConfigurationManager::getToneLocaleList received\n");

}


void 
ConfigurationManager::setToneLocale( const ::DBus::String& locale )
{
    _debug("ConfigurationManager::setToneLocale received\n");

}


::DBus::String 
ConfigurationManager::getToneLocale(  )
{
    _debug("ConfigurationManager::getToneLocale received\n");

}


::DBus::String 
ConfigurationManager::getVersion(  )
{
    _debug("ConfigurationManager::getVersion received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getRingtoneList(  )
{
    _debug("ConfigurationManager::getRingtoneList received\n");

}


void 
ConfigurationManager::setRingtone( const ::DBus::String& ringtone )
{
    _debug("ConfigurationManager::setRingtone received\n");

}


::DBus::String 
ConfigurationManager::getRingtone(  )
{
    _debug("ConfigurationManager::getRingtone received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getCodecList(  )
{
    _debug("ConfigurationManager::getCodecList received\n");

}


void 
ConfigurationManager::setCodecPreferedOrder( const std::vector< ::DBus::String >& ringtone )
{
    _debug("ConfigurationManager::setCodecPreferedOrder received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getCodecPreferedOrder(  )
{
    _debug("ConfigurationManager::getCodecPreferedOrder received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getPlaybackDeviceList(  )
{
    _debug("ConfigurationManager::getPlaybackDeviceList received\n");

}


void 
ConfigurationManager::setPlaybackDevice( const ::DBus::String& device )
{
    _debug("ConfigurationManager::setPlaybackDevice received\n");

}


::DBus::String 
ConfigurationManager::getPlaybackDevice(  )
{
    _debug("ConfigurationManager::getPlaybackDevice received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getRecordDeviceList(  )
{
    _debug("ConfigurationManager::getRecordDeviceList received\n");

}


void 
ConfigurationManager::setRecordDevice( const ::DBus::String& device )
{
    _debug("ConfigurationManager::setRecordDevice received\n");

}


::DBus::String 
ConfigurationManager::getRecordDevice(  )
{
    _debug("ConfigurationManager::getRecordDevice received\n");

}


std::vector< ::DBus::String > 
ConfigurationManager::getSampleRateList(  )
{
    _debug("ConfigurationManager::getSampleRateList received\n");

}


void 
ConfigurationManager::setSampleRate( const ::DBus::String& sampleRate )
{
    _debug("ConfigurationManager::setSampleRate received\n");

}


::DBus::String 
ConfigurationManager::getSampleRate(  )
{
    _debug("ConfigurationManager::getSampleRate received\n");

}


