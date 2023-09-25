/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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
 */

#pragma once

#include "dtmfgenerator.h"

/**
 * @file dtmf.h
 * @brief DMTF library to generate a dtmf sample
 */
namespace jami {

class DTMF
{
public:
    /**
     * Create a new DTMF.
     * @param sampleRate frequency of the sample (ex: 8000 hz)
     */
    DTMF(unsigned int sampleRate, AVSampleFormat sampleFormat);

    /**
     * Start the done for th given dtmf
     * @param code  The DTMF code
     */
    void startTone(char code);

    /**
     * Copy the sound inside the sampling* buffer
     */
    bool generateDTMF(AVFrame* frame);

private:
    char currentTone_;
    char newTone_;

    DTMFGenerator dtmfgenerator_;
};

} // namespace jami
