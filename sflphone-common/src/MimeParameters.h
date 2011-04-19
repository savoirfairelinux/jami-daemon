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

#ifndef __SFL_MIME_PARAMETERS_H__
#define __SFL_MIME_PARAMETERS_H__

/**
 * Start a new payload format definition.
 */
#define MIME_PAYLOAD_FORMAT_DEFINITION( mime, subtype, payloadType, clock ) \
		private: \
			uint8 payload; \
		public: \
		inline virtual ~MimeParameters##subtype() {}; \
        std::string getMimeType() const { \
            return std::string( mime ); \
        } \
        std::string getMimeSubtype() const { \
            return std::string( #subtype ); \
        } \
        uint8 getPayloadType() const { \
            return payload; \
        } \
        void setPayloadType(uint8 pt) { \
			payload = pt; \
		} \
        uint32 getClockRate() const { \
            return clock; \
        } \
	    MimeParameters##subtype() : payload(payloadType) {

/**
 * An alias for MIME_PARAMETER_OPTIONAL
 */
#define MIME_PARAMETER(name, handler)	\
	addOptionalParameter( name ); \
	addHandler( name, handler );

/**
 * Defines an optional parameter.
 */
#define MIME_PARAMETER_OPTIONAL(name, handler) \
	addOptionalParameter( name ); \
	addHandler( name, handler );

/**
 * Defines a required parameter. The value of this parameter
 * should be obtained when sending the initial SDP offer.
 */
#define MIME_PARAMETER_REQUIRED(name, handler) \
	addRequiredParameter( name ); \
	addHandler( name, handler );

/**
 * End a payload format definition.
 */
#define MIME_PAYLOAD_FORMAT_DEFINITION_END() \
        }

#define MIME_PARAMETER_KEEP_IF_EQUAL MimeParameters::addParameterIfEqual

#define MIME_PARAMETER_KEEP_MINIMUM MimeParameters::addParameterMinimum

#define MIME_PARAMETER_KEEP_REMOTE MimeParameters::addParameterRemote

#include "sip/Fmtp.h"

#include <ccrtp/rtp.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <map>
#include <errno.h>

namespace sfl
{

/**
 * This exception is thrown in cases where a given format cannot
 * be parsed, or required pieces of information are missing.
 */
class SdpFormatNegotiationException: public std::runtime_error
{
	public:
	SdpFormatNegotiationException (const std::string& msg) : std::runtime_error (msg) {}
};

/**
 * Interface for exposing MIME parameters in SDP offer/answer model.
 */
class MimeParameters
{
    public:
        /**
         * @return The mimesubtype for this codec. Eg. : "video"
         */
        virtual std::string getMimeType() const = 0;

        /**
         * @return The mimesubtype for this codec. Eg. : "theora"
         */
        virtual std::string getMimeSubtype() const = 0;

        /**
         * @return payload type numeric identifier.
         */
        virtual uint8 getPayloadType() const = 0;

        /**
         * @param payload The new payload to set
         */
        virtual void setPayloadType(uint8 payload) = 0;

        /**
         * @return RTP clock rate in Hz.
         */
        virtual uint32 getClockRate() const = 0;

        /**
         * @param name The name that identifies the MIME parameter.
         * @param value The value this parameter should have.
         */
        virtual void setParameter (const std::string& name, const std::string& value) = 0;

        /**
         * @param name The name that identifies the MIME parameter.
         * @return The value that is set for this parameter.
         */
        virtual std::string getParameter (const std::string& name) = 0;

        /**
         * @return A string containing the codec specific parameters, formatted by default as :
         * "PARAM_LIST : PARAM_NAME = VALUE SEMI_COLON PARAM_LIST | PARAM_END
         *  PARAM_END : empty"
         */
        virtual std::string getParametersFormatted() {
        	// TODO Instead of putting everything into the same vector,
        	// enforce the required vs optional aspect. Unfilled required params. should
        	// result in exception throwing.
        	std::vector<std::string> paramList = requiredParameters;
        	std::copy(optionalParameters.begin(), optionalParameters.end(), std::back_inserter(paramList));

        	std::string output("");
        	std::vector<std::string>::iterator it;
        	int numberParamsAppended = 0;
        	for (it = paramList.begin(); it != paramList.end(); it++) {

        		std::string name = *it;
        		std::string value = getParameter(name);
        		if (value != "") {
        	   		if (numberParamsAppended != 0) {
        	        	output.append("; ");
        	        }

        	   		output.append(name + "=" + value);

        	   		numberParamsAppended += 1;
        		}
        	}

        	return output;
        }

        /**
         * Calls #setParameter() in a loop for every parameters of the format object.
         * @param format The format object containing the parameters to set.
         */
        void setParameters(const sfl::Fmtp& format) {
        	sfl::Fmtp::const_iterator it;
        	for (it = format.begin(); it != format.end(); it++) {
        		_info("****************** Setting parameters");
        		setParameter((*it).first, (*it).second);
        	}
        }

        void setNegotiatedParameters(const sfl::Fmtp& format) {
        	negotiatedFormat = format;
        }

        void applyNegotiatedParameters() {
        	setParameters(negotiatedFormat);
        }

    protected:
        sfl::Fmtp negotiatedFormat;

        /**
         * @param name The name for the required parameter to add.
         */
        void addRequiredParameter(const std::string& name) {
        	requiredParameters.push_back(name);
        }
        std::vector<std::string> requiredParameters;

        /**
         * @param name The name for the optional parameter to add.
         */
        void addOptionalParameter(const std::string& name) {
        	optionalParameters.push_back(name);
        }
        std::vector<std::string> optionalParameters;

        /**
         * Negotiation handler type.
         * @param localParam The local parameter.
         * @param remoteParam The remote parameter.
         * @param offerer The specified format for the offerer.
         * @param answerer The specified format for the answerer.
         * @param negotiatedFmtp The format in which to make changes, if any.
         * @throw SdpFormatNegotiationException if the format can't be processed because of missing options,
         * or wrong formatting.
         */
        typedef void (*ParameterNegotiationHandler)(
        		const sfl::SdpParameter& localParam,
        		const sfl::SdpParameter& remoteParam,
        		const sfl::Fmtp& offerer,
        		const sfl::Fmtp& answerer,
        		sfl::Fmtp& negotiatedFmtp);

        std::map<std::string, ParameterNegotiationHandler> parameterHandlerTable;

        typedef std::pair<std::string, ParameterNegotiationHandler> ParameterHandlerEntry;

        typedef std::map<std::string, ParameterNegotiationHandler>::iterator ParameterHandlerIterator;

        /**
         * Add a negotiation handler for the given property.
         * @param name The parameter name (eg: "profile-level-id")
         * @param handler A pointer to the negotiation handler function;
         */
        void addHandler(const std::string& name, ParameterNegotiationHandler handler) {
        	parameterHandlerTable.insert(ParameterHandlerEntry(name, handler));
        }

    public:

        /**
         * The default behavior if this method is not overriden is to return the format from
         * the answerer.
         * @param offerer The specified format for the offerer.
         * @param answerer The specified format for the answerer.
         * @return A format object containing the negotiated format.
         * @throw SdpFormatNegotiationException if the format can't be processed because of missing options,
         * or wrong formatting.
         */
        virtual sfl::Fmtp negotiate(const sfl::Fmtp& offerer, const sfl::Fmtp& answerer) throw(SdpFormatNegotiationException) {
        	sfl::Fmtp::iterator itAnswerer;

        	// This object will be built iteratively
			sfl::Fmtp negotiatedFmtp(offerer.getPayloadType());

			sfl::Fmtp::const_iterator it;
			// Iterate over all of the properties in the answer
    		for (it = answerer.begin(); it != answerer.end(); it++) {
    			std::string propertyName = (*it).first;
    			std::string valueAnswerer = (*it).second;

    			// Find the corresponding property in the offer
    			std::string valueOfferer;
    			sfl::Fmtp::const_iterator itOfferer = offerer.getParameter(propertyName);
    			if (it == offerer.end()) {
    				valueOfferer = "";
    			} else {
    				valueOfferer = (*itOfferer).second;
    			}

    			// Find a suitable property handler
        		ParameterHandlerIterator itHandler = parameterHandlerTable.find(propertyName);
        		if (itHandler != parameterHandlerTable.end()) {
        			((*itHandler).second)(sfl::SdpParameter(propertyName, valueOfferer),
        					sfl::SdpParameter(propertyName, valueAnswerer),
        					offerer, answerer, negotiatedFmtp);
        		} else {
        			_error("Could not find any handler for property \"%s\"", propertyName.c_str());
        		}
    		}

    		return negotiatedFmtp;
    	}

    protected:
        /**
         * This handler makes
         */
        static void addParameterIfEqual(
        		const sfl::SdpParameter& localParam,
        		const sfl::SdpParameter& remoteParam,
        		const sfl::Fmtp& offerer,
        		const sfl::Fmtp& answerer,
        		sfl::Fmtp& negotiatedFmtp)
        {
        	if (localParam.getValue() == remoteParam.getValue()) {
        		negotiatedFmtp[localParam.getName()] = localParam.getValue();
        	}
        }

        static void addParameterMinimum(
        		const sfl::SdpParameter& localParam,
        		const sfl::SdpParameter& remoteParam,
        		const sfl::Fmtp& offerer,
        		const sfl::Fmtp& answerer,
        		sfl::Fmtp& negotiatedFmtp)
        {
    		long int localValue = strtol(localParam.getValue().c_str(), NULL, 16);
    		if (localValue == 0 && errno == EINVAL) {
    			// Throw
    		}

    		long int remoteValue = strtol(remoteParam.getValue().c_str(), NULL, 16);
    		if (remoteValue == 0 && errno == EINVAL) {
    			// Throw
    		}

    		long int minimumValue = std::min(localValue, remoteValue);
    		negotiatedFmtp[localParam.getName()] = minimumValue;
        }

        static void addParameterRemote(
        		const sfl::SdpParameter& localParam,
        		const sfl::SdpParameter& remoteParam,
        		const sfl::Fmtp& offerer,
        		const sfl::Fmtp& answerer,
        		sfl::Fmtp& negotiatedFmtp)
        {
        	negotiatedFmtp[remoteParam.getName()] = remoteParam.getValue();
        }
};

}
#endif
