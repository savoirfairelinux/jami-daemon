/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 * Author: Yan Morin <yan.morin@savoirfairelinux.com>
 * Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
 *
 * Portions (c) 2003 iptel.org
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 *
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

#ifndef DTMFGENERATOR_H
#define DTMFGENERATOR_H

#include <exception>
#include <string.h>

#include "tone.h"

#define NUM_TONES 16

/*
 * @file dtmfgenerator.h
 * @brief DMTF Generator Exception
 */
class DTMFException : public std::exception
{
    private:

        /** Message */
        const char* reason;
    public:
        /**
         * Constructor
         * @param _reason An error message
         */
        DTMFException (const char* _reason) throw();

        /**
         * Destructor
         */
        virtual ~DTMFException() throw();
        /*
            // Copy Constructor
            DTMFException(const DTMFException& rh) throw();

            // Assignment Operator
            DTMFException& operator=( const DTMFException& rh) throw();
        */
        /**
         * @return const char* The error
         */
        virtual const char* what() const throw();
};

/*
 * @file dtmfgenerator.h
 * @brief DTMF Tone Generator
 */
class DTMFGenerator
{
    private:
        /** Struct to handle a DTMF */
        struct DTMFTone {
            unsigned char code; /** Code of the tone */
            int lower;          /** Lower frequency */
            int higher;         /** Higher frequency */
        };

        /** State of the DTMF generator */
        struct DTMFState {
            unsigned int offset;   /** Offset in the sample currently being played */
            SFLDataFormat* sample;         /** Currently generated code */
        };

        /** State of the DTMF generator */
        DTMFState state;

        /** The different kind of tones */
        static const DTMFTone tones[NUM_TONES];

        /** Generated samples */
        SFLDataFormat* samples[NUM_TONES];

        /** Sampling rate of generated dtmf */
        int _sampleRate;

        /** A tone object */
        Tone tone;

    public:
        /**
         * DTMF Generator contains frequency of each keys
         * and can build one DTMF.
         * @param sampleRate frequency of the sample (ex: 8000 hz)
         */
        DTMFGenerator (unsigned int sampleRate);

        /**
         * Destructor
         */
        ~DTMFGenerator();


        // Copy Constructor
        DTMFGenerator (const DTMFGenerator& rh);

        // Assignment Operator
        DTMFGenerator& operator= (const DTMFGenerator& rh);

        /*
         * Get n samples of the signal of code code
         * @param buffer a SFLDataFormat pointer to an allocated buffer
         * @param n      number of sampling to get, should be lower or equal to buffer size
         * @param code   dtmf code to get sound
         */
        void getSamples (SFLDataFormat* buffer, size_t n, unsigned char code) throw (DTMFException);

        /*
         * Get next n samples (continues where previous call to
         * genSample or genNextSamples stopped
         * @param buffer a SFLDataFormat pointer to an allocated buffer
         * @param n      number of sampling to get, should be lower or equal to buffer size
         */
        void getNextSamples (SFLDataFormat* buffer, size_t n) throw (DTMFException);

    private:

        /**
         * Generate samples for a specific dtmf code
         * @param code The code
         * @return SFLDataFormat* The generated data
         */
        SFLDataFormat* generateSample (unsigned char code) throw (DTMFException);
};

#endif // DTMFGENERATOR_H
