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
#ifndef __SFL_FMTP_H__
#define __SFL_FMTP_H__

#include "logger.h"

#include <cc++/tokenizer.h>
#include <string>
#include <map>

namespace sfl {

/**
 * A utility class for holding SDP parameters.
 */
class SdpParameter: public std::pair<std::string, std::string> {
public:
	SdpParameter(const std::string& name, const std::string& value) :
		std::pair<std::string, std::string>(name, value) {
	}
	;
	inline ~SdpParameter() {
	}
	;
	std::string getName() const {
		return first;
	}
	std::string getValue() const {
		return second;
	}
};

/**
 *  Class for holding the attributes of a a=fmtp SDP line.
 */
class Fmtp: public std::map<std::string, std::string> {
public:
	Fmtp(const std::string& payloadType) {
		this->payloadType = payloadType;
		params = "";
	}

	Fmtp() {
		payloadType = "96";
		params = "";
	}

	/**
	 * Split the params of a a=fmtp line into its individual parameter, separated by ";" tokens.
	 * Note that RFC4566 indicates no assumption about how this piece of data should be formatted.
	 *
	 * @param payloadType The static or dynamic payload type corresponding to some a=rtpmap line.
	 * @params params Codec specific parameters.
	 */
	Fmtp(const std::string& payloadType, const std::string& paramsUnparsed) {
		this->payloadType = payloadType;
		params = paramsUnparsed;

		ost::StringTokenizer paramsTokenizer(paramsUnparsed.c_str(), ";",
				false, true);
		ost::StringTokenizer::iterator it;

		for (it = paramsTokenizer.begin(); it != paramsTokenizer.end(); ++it) {
			std::string param(*it);
			size_t pos = param.find("=");
			std::string value = param.substr(pos + 1); // FIXME Too naive !
			std::string property = param.substr(0, pos); // FIXME Too naive !

			insert(std::pair<std::string, std::string>(property, value));
		}
	}

	/**
	 * Given the list of parameters kept in this object,
	 * create a formatted string that represents this list.
	 * @return The formatted list of parameters.
	 */
	std::string getParametersFormatted() const {
		std::string output = "";
    	int numberParamsAppended = 0;

		const_iterator it;
		for (it = begin(); it != end(); it++) {
			std::string name = (*it).first;
			std::string value = (*it).second;
			if (numberParamsAppended != 0) {
				output.append("; ");
			}

			output.append(name + "=" + value);

			numberParamsAppended += 1;
		}

		return output;
	}

	/**
	 * @return An iterator to the element, if the specified key value is found,
	 * or Fmtp::end if the specified is not found in the parameter list.
	 */
	iterator getParameter(const std::string& paramName) {
		return find(paramName);
	}

	/**
	 * @return An iterator to the element, if the specified key value is found,
	 * or Fmtp::end if the specified is not found in the parameter list.
	 */
	const_iterator getParameter(const std::string& paramName) const {
		return find(paramName);
	}

	/**
	 * @return The static or dynamic payload type corresponding to some a=rtpmap line.
	 */
	std::string getPayloadType() const {
		return payloadType;
	}

private:
	std::string payloadType;
	std::string params;
};

}

#endif
