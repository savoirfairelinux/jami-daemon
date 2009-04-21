/*
 *  Copyright (C) 2004-2005-2006 Savoir-Faire Linux inc.
 *  Author : Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com> 
 *
 * 	Portions Copyright (c) 2000 Billy Biggs <bbiggs@div8.net>
 *  Portions Copyright (c) 2004 Wirlab <kphone@wirlab.net>
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

#ifndef __DTMF_H_
#define __DTMF_H_

#include "../global.h"
#include "dtmfgenerator.h"

/**
 * @file dtmf.h
 * @brief DMTF library to generate a dtmf sample
 */
class DTMF {
  public:
    /**
     * Create a new DTMF.
     * @param sampleRate frequency of the sample (ex: 8000 hz)
     */
    DTMF (unsigned int sampleRate);
    
   /**
    * Destructor
    */ 
    ~DTMF (void);

    /**
     * Start the done for th given dtmf
     * @param code  The DTMF code
     */
    void startTone(char code);
    
    /**
     * Copy the sound inside the sampling* buffer 
     * @param buffer : a SFLDataFormat* buffer
     * @param n      : The size to generate
     */
    bool generateDTMF	(SFLDataFormat* buffer, size_t n);

    char currentTone;
    char newTone;

    DTMFGenerator dtmfgenerator;
};

#endif // __KEY_DTMF_H_
