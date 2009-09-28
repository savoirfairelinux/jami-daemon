/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 */
 
#include "SdesNegotiator.h"

#include "util/Pattern.h"

#include <iostream> 
#include <sstream>
#include <algorithm>
#include <stdexcept> 

using namespace sfl::util;

struct CryptoAttribute {
	std::string tag;
	std::string cryptoSuite;
	std::string keyParams;
	std::string sessionParams;
};

SdesNegotiator::SdesNegotiator(const std::vector<CryptoSuiteDefinition>& localCapabilites, 
							   const std::vector<std::string>& remoteAttribute) :
_remoteAttribute(remoteAttribute),
_localCapabilities(localCapabilites)
{

}

void SdesNegotiator::parse(void)
{       
	// The patterns below try to follow
	// the ABNF grammar rules described in 
	// RFC4568 section 9.2 with the general
	// syntax :
	//a=crypto:tag 1*WSP crypto-suite 1*WSP key-params *(1*WSP session-param)

	Pattern
	* generalSyntaxPattern,
	* tagPattern, 
	* cryptoSuitePattern, 
	* keyParamsPattern, 
	* sessionParamPattern;
		
	try {
		generalSyntaxPattern = new Pattern("[\x20\x09]+", "g");
		
		tagPattern = new Pattern("^a=crypto:(?P<tag>[0-9]{1,9})");

		cryptoSuitePattern = new Pattern(
		"(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
		"AES_CM_128_HMAC_SHA1_32|" \
		"F8_128_HMAC_SHA1_80|" \
		"[A-Za-z0-9_]+)"); // srtp-crypto-suite-ext

		keyParamsPattern = new Pattern(
		"(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
		"(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)\\|" \
		"2\\^(?P<lifetime>[0-9]+)\\|" \
		"(?P<mkiValue>[0-9]+)\\:" \
		"(?P<mkiLength>[0-9]{1,3})\\;?", "g");

		sessionParamPattern = new Pattern(
		"(?P<sessionParam>(kdr\\=[0-9]{1,2}|" \
		"UNENCRYPTED_SRTP|" \
		"UNENCRYPTED_SRTCP|" \
		"UNAUTHENTICATED_SRTP|" \
		"FEC_ORDER=(?P<fecOrder>FEC_SRTP|SRTP_FEC)|" \
		"FEC_KEY=(?P<fecKey>" + keyParamsPattern.getPattern() + ")|" \
		"WSH=(?P<wsh>[0-9]{1,2})|" \
		"(?<!\\-)[[:graph:]]+))*", "g"); // srtp-session-extension
							  
	} catch(compile_error& exception) {
		throw parse_error("A compile exception occured on a pattern.");
		
	}

	// Take each line from the vector
	// and parse its content		
	
	std::vector<std::string>::iterator iter;    
	for (iter = _remoteAttribute.begin(); iter != _remoteAttribute.end(); iter++) {

		std::cout << (*iter) << std::endl;
		
		// Split the line into its component 
		// that we will analyze further down.
		
		generalSyntaxPattern << (*iter);
		std::vector<std::string> sdesLine;
		
		try {
			sdesLine = generalSyntaxPattern.split();
			if (sdesLine.size() < 3) {
				throw parse_error("Missing components in SDES line");
			}
		} catch (match_error& exception) {
			throw parse_error("Error while analyzing the SDES line.");
		}
		
		// Check if the attribute starts with a=crypto
		// and get the tag for this line
		tagPattern << sdesLine.at(0);
		try {
			std::string tag = tagPattern.group("tag");
			std::cout << "tag = " << tag << std::endl;
		} catch (match_error& exception) {
			throw parse_error("Error while parsing the tag field");
		}
		
		// Check if the crypto suite is valid and retreive
		// its value.
		cryptoSuitePattern << sdesLine.at(1);
		try {
			std::string cryptoSuite 
			cryptoSuite = cryptoSuitePattern.group("cryptoSuite");
			std::cout << "crypto-suite = " << cryptoSuite << std::endl;
		} catch (match_error& exception) {
			throw parse_error("Error while parsing the crypto-suite field");
		}
		
		// Parse one or more key-params field.
		keyParamsPattern << sdesLine.at(2);
		try {
			while (keyParamsPattern.matches()) {
				std::string srtpKeyMethod;
				srtpKeyMethod = keyParamsMatched.group("srtpKeyMethod");
				std::cout << "srtp-key-method = " << srtpKeyMethod << std::endl;
				
				std::string srtpKeyInfo;
				srtpKeyInfo = keyParamsPattern.group("srtpKeyInfo");
				std::cout << "srtp-key-info = " << srtpKeyInfo << std::endl;
				
				std::string lifetime;
				lifetime = keyParamsPattern.group("lifetime");
				std::cout << "lifetime = " << lifetime << std::endl;
				
				std::string mkiValue
				mkiValue = keyParamsPattern.group("mkiValue");
				std::cout << "mkiValue = " << mkiValue << std::endl;
				
				std::string mkiLength;
				mkiLength = keyParamsPattern.group("mkiLength");
				std::cout << "mkiLength = " << mkiLength << std::endl;                 
			}			
		} catch (match_error& exception) {
			throw parse_error("Error while parsing the key-params field");
		}
		
		/**
		 *  Parse the optional session-param fields
		 * @todo Implement this !
		 */
		/*
		if (sdesLine.size() == 3) continue;
		
		int i;
		for (i = 3; i < sdesLine.size(); i++) {
			sessionParamPattern << sdesLine.at(i); 
			while (sessionpParamPattern.matches()) {
					
				} catch (match_error& exception) {
					throw parse_error("Error while parsing the crypto-suite field");
				}                                  
			}
		} */
	}
   

}

bool SdesNegotiator::negotiate(void)
{
	parse();
}
