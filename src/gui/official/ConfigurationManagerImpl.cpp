/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
              <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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

#include "ConfigurationManagerImpl.hpp"
#include "DebugOutput.hpp"

void 
ConfigurationManagerImpl::setCurrentSpeakerVolume(unsigned int )
{
}

void
ConfigurationManagerImpl::setCurrentMicrophoneVolume(unsigned int )
{
}

void
ConfigurationManagerImpl::add(const ConfigEntry &entry)
{
  mEntries[entry.section][entry.name] = entry;
}

void
ConfigurationManagerImpl::add(const AudioDevice &entry)
{
  mAudioDevices.push_back(entry);
}

void
ConfigurationManagerImpl::set(const QString &section,
			      const QString &name,
			      const QString &value)
{
  SectionMap::iterator pos = mEntries.find(section);
  if(pos != mEntries.end()) {
    VariableMap::iterator vpos = pos->second.find(name);
    if(vpos != pos->second.end()) {
      vpos->second.value = value;
    }
  }
}

