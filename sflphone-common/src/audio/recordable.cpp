/*
 *  Copyright (C) 2004-2008 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "recordable.h"
#include "manager.h"

Recordable::Recordable()
{

    FILE_TYPE fileType = FILE_WAV;
    SOUND_FORMAT soundFormat = INT16;

    recAudio.setRecordingOption (fileType, soundFormat, 44100, Manager::instance().getConfigString (AUDIO, RECORD_PATH));
}


Recordable::~Recordable()
{
    if (recAudio.isOpenFile()) {
        recAudio.closeFile();
    }
}


void Recordable::initRecFileName()
{
    _debug("XXXXXXXXXXXXXXXXX getRecFileId() %s XXXXXXXXXXXXXXXXXXX\n", getRecFileId().c_str());

    recAudio.initFileName (getRecFileId());
}
