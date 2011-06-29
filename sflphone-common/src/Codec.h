/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#ifndef __SFL_CODEC_H__
#define __SFL_CODEC_H__

#include "MimeParameters.h" // TODO Move to some higher directory

#include <cc++/digest.h>

/**
 * Interface for both audio codecs as well as video codecs.
 */
namespace sfl {
class Codec : public virtual MimeParameters
{
    public:
        Codec() {};
        virtual ~Codec() {}

        /**
         * @return The bitrate for which this codec is configured // TODO deal with VBR case.
         */
        virtual double getBitRate() const = 0;

        /**
         * @return The expected bandwidth used by this codec.
         */
        virtual double getBandwidth() const = 0;

        /**
         * @return Additional information (description) about this codec. This is meant to be shown to the user.
         */
        virtual std::string getDescription() const = 0;

        /**
         * @return A copy of the current codec.
         */
        //virtual Codec* clone() const = 0;

        /**
         * Build a unique hash code for identifying the codec uniquely.
         * Note that if multiple implementations of codec are provided,
         * one should override this function in order to avoid conflicting
         * hash codes.
         *
         * Hash code is a CRC32 digest made from :
         * MIME "." MIME SUBTYPE "." PAYLOAD TYPE "." PAYLOAD CLOCKRATE
         *
         * @return a unique hash code identifying this codec.
         */
        virtual std::string hashCode() const {
            ost::CRC32Digest digest;

            std::ostringstream os;
            os << getMimeType()
            << "." << getMimeSubtype()
            << "." << getPayloadType()
            << "." << getClockRate();

            std::string concat = os.str();

            digest.putDigest ( (const unsigned char*) concat.c_str(), concat.length());

            std::ostringstream buffer;
            buffer << digest;

            return buffer.str();
        }
};
}

typedef sfl::Codec* create_t();

typedef void destroy_t (sfl::Codec*);

#endif
