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

#ifndef RECORDABLE_H
#define RECORDABLE_H

#include "../plug-in/audiorecorder/audiorecord.h"

class Recordable {

    public:

        Recordable();

	~Recordable();

	bool isRecording(){ return recAudio.isRecording(); }

	bool setRecording(){ return recAudio.setRecording(); }

	void stopRecording(){ recAudio.stopRecording(); }

	void initRecFileName();

	void setRecordingSmplRate(int smplRate);

	virtual std::string getRecFileId() = 0;

	// virtual std::string getFileName() = 0;

	// std::string getFileName() { return _filename; }

	/**
	 * An instance of audio recorder
	 */
         AudioRecord recAudio;


    private:

	/** File name for his call : time YY-MM-DD */
        // std::string _filename;

        

};

#endif
