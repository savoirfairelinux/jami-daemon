/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "SdesNegotiator.h"

#include "Pattern.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

using namespace sfl;

SdesNegotiator::SdesNegotiator (const std::vector<CryptoSuiteDefinition>& localCapabilites,
                                const std::vector<std::string>& remoteAttribute) :
        _remoteAttribute (remoteAttribute),
        _localCapabilities (localCapabilites)
{

}

std::vector<CryptoAttribute *> SdesNegotiator::parse (void)
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

        // used to match white space (which are used as separator) 
        generalSyntaxPattern = new Pattern ("[\x20\x09]+", "g");

        tagPattern = new Pattern ("^a=crypto:(?P<tag>[0-9]{1,9})");

        cryptoSuitePattern = new Pattern (
            "(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
            "AES_CM_128_HMAC_SHA1_32|" \
            "F8_128_HMAC_SHA1_80|" \
            "[A-Za-z0-9_]+)"); // srtp-crypto-suite-ext

	keyParamsPattern = new Pattern (
	    "(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
	    "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)"	 \
	    "(\\|2\\^(?P<lifetime>[0-9]+)\\|"		 \
	    "(?P<mkiValue>[0-9]+)\\:"			 \
	    "(?P<mkiLength>[0-9]{1,3})\\;?)?", "g");

        sessionParamPattern = new Pattern (
            "(?P<sessionParam>(kdr\\=[0-9]{1,2}|" \
            "UNENCRYPTED_SRTP|" \
            "UNENCRYPTED_SRTCP|" \
            "UNAUTHENTICATED_SRTP|" \
            "FEC_ORDER=(?P<fecOrder>FEC_SRTP|SRTP_FEC)|" \
            "FEC_KEY=(?P<fecKey>" + keyParamsPattern->getPattern() + ")|" \
            "WSH=(?P<wsh>[0-9]{1,2})|" \
            "(?<!\\-)[[:graph:]]+))*", "g"); // srtp-session-extension

    } catch (compile_error& exception) {
        throw parse_error ("A compile exception occured on a pattern.");
    }
      

    // Take each line from the vector
    // and parse its content

    
    std::vector<std::string>::iterator iter;
    std::vector<CryptoAttribute *> cryptoAttributeVector;
	
    for (iter = _remoteAttribute.begin(); iter != _remoteAttribute.end(); iter++) {

        // Split the line into its component
        // that we will analyze further down.
        std::vector<std::string> sdesLine;
	
	*generalSyntaxPattern << (*iter);
        
        try {
            sdesLine = generalSyntaxPattern->split();

            if (sdesLine.size() < 3) {
                throw parse_error ("Missing components in SDES line");
            }
        } catch (match_error& exception) {
            throw parse_error ("Error while analyzing the SDES line.");
        }
	

        // Check if the attribute starts with a=crypto
        // and get the tag for this line
        *tagPattern << sdesLine.at (0);
				
	std::string tag; 
	if (tagPattern->matches()) {
	try {
	    // std::cout << "Parsing the tag field";
	    tag = tagPattern->group ("tag");
	    // std::cout << ": " << tag << std::endl;
	} catch (match_error& exception) {
	    throw parse_error ("Error while parsing the tag field");
	}
	} else {
	    return cryptoAttributeVector;
	}

        // Check if the crypto suite is valid and retreive
        // its value.
        *cryptoSuitePattern << sdesLine.at (1);

	std::string cryptoSuite;
		
	if (cryptoSuitePattern->matches()) {
	    try {
	        // std::cout << "Parsing the crypto suite field";
	        cryptoSuite = cryptoSuitePattern->group ("cryptoSuite");
		// std::cout << ": " << cryptoSuite << std::endl;
	    } catch (match_error& exception) {
	        throw parse_error ("Error while parsing the crypto-suite field");
	    }
	} else {
	    return cryptoAttributeVector;
	}
	
        // Parse one or more key-params field.
        *keyParamsPattern << sdesLine.at (2);

	std::string srtpKeyInfo;
	std::string srtpKeyMethod;
	std::string lifetime;
	std::string mkiLength;
	std::string mkiValue;
	
        try {
            while(keyParamsPattern->matches()) {
                srtpKeyMethod = keyParamsPattern->group ("srtpKeyMethod");
                srtpKeyInfo = keyParamsPattern->group ("srtpKeyInfo");
                lifetime = keyParamsPattern->group ("lifetime");
                mkiValue = keyParamsPattern->group ("mkiValue");
                mkiLength = keyParamsPattern->group ("mkiLength");
            }
        } catch (match_error& exception) {
            throw parse_error ("Error while parsing the key-params field");
        }

        /**
         *  Parse the optional session-param fields
         * @TODO Implement this !
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
		
	// Add the new CryptoAttribute to the vector
	
	CryptoAttribute * cryptoAttribute = new CryptoAttribute(tag, cryptoSuite, srtpKeyMethod, srtpKeyInfo, lifetime, mkiValue, mkiLength);
	cryptoAttributeVector.push_back(cryptoAttribute);
    }

    return cryptoAttributeVector;
}

bool SdesNegotiator::negotiate (void)
{

    std::vector<CryptoAttribute *> cryptoAttributeVector = parse();
    std::vector<CryptoAttribute *>::iterator iter_offer = cryptoAttributeVector.begin();

    std::vector<CryptoSuiteDefinition>::iterator iter_local = _localCapabilities.begin();

    bool negotiationSuccess = false;

    try {
		
        while (!negotiationSuccess && (iter_offer != cryptoAttributeVector.end())) {

	    /*
	    std::cout << "Negotiate tag: " + (*iter_offer)->getTag() << std::endl;
	    std::cout << "Crypto Suite: " + (*iter_offer)->getCryptoSuite() << std::endl;
	    std::cout << "SRTP Key Method: " + (*iter_offer)->getSrtpKeyMethod() << std::endl;
	    std::cout << "SRTP Key Info: " + (*iter_offer)->getSrtpKeyInfo() << std::endl;
	    std::cout << "Lifetime: " + (*iter_offer)->getLifetime() << std::endl;
	    std::cout << "MKI Value: " + (*iter_offer)->getMkiValue() << std::endl;			
	    std::cout << "MKI Length: " + (*iter_offer)->getMkiLength() << std::endl;			
	    */

	    iter_local = _localCapabilities.begin();

	    while(!negotiationSuccess && (iter_local != _localCapabilities.end())) {  

	        if((*iter_offer)->getCryptoSuite().compare((*iter_local).name)){

		    negotiationSuccess = true;

		    _cryptoSuite = (*iter_offer)->getCryptoSuite();
		    _srtpKeyMethod = (*iter_offer)->getSrtpKeyMethod();
		    _srtpKeyInfo = (*iter_offer)->getSrtpKeyInfo();
		    _lifetime = (*iter_offer)->getLifetime();
		    _mkiValue = (*iter_offer)->getMkiValue();
		    _mkiLength = (*iter_offer)->getMkiLength();
		}

		iter_local++;
	    }

	    delete (*iter_offer);
	    
	    iter_offer++;
	}
	
    } catch (parse_error& exception) {
        return false;
    } catch (match_error& exception) {
        return false;
    }
	
    return negotiationSuccess;
}
