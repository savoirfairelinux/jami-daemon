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

#include "regex.h"

#include <iostream> 
#include <sstream>
#include <algorithm>
#include <stdexcept> 

namespace sfl {

    struct CryptoAttribute {
        std::string tag;
        std::string cryptoSuite;
        std::string keyParams;
        std::string sessionParams;
    };
    
    SdesNegotiator::SdesNegotiator(const std::vector<CryptoSuiteDefinition>& localCapabilites, const std::vector<std::string>& remoteAttribute) :
    _remoteAttribute(remoteAttribute),
    _localCapabilities(localCapabilites)
    {
    
    }

    CryptoAttribute * SdesNegotiator::tokenize(const std::string& attributeLine) 
    {
        // Split the line into at most
        // 4 components corresponding to
        // a=crypto:<tag> <crypto-suite> <key-params> [<session-params>]
        size_t pos;
        const char WSP = ' ';        
        std::string line(attributeLine);
        
        pos = line.rfind(WSP);
        
        std::string token;
        std::vector<std::string> lineSplitted;
            
        while (pos != std::string::npos && lineSplitted.size() != 4) {

            token = line.substr(pos+1);
            lineSplitted.push_back(token);

            token = line.substr(0, pos);
            line = token;
                        
            pos = line.rfind(WSP);

        }

        lineSplitted.push_back(line);

        CryptoAttribute * cryptoLine;
        // Build up the new CryptoAttribute
        try {
            cryptoLine = new CryptoAttribute();        
        } catch (std::bad_alloc&) {
            std::cerr << "Failed create new CryptoLine" << std::endl;    
            throw;	
        }       
        
        std::reverse(lineSplitted.begin(), lineSplitted.end());
        
        cryptoLine->tag = lineSplitted.at(0);
        cryptoLine->cryptoSuite = lineSplitted.at(1);
        cryptoLine->keyParams = lineSplitted.at(2);
        if (lineSplitted.size() == 4) {
            cryptoLine->sessionParams = lineSplitted.at(3);
        }
        
        return cryptoLine;
    }
   
    bool SdesNegotiator::parse(void)
    {
       std::vector<std::string>::iterator iter; 
       
       for (iter = _remoteAttribute.begin(); iter != _remoteAttribute.end(); iter++) {
       
            // Split the line into components
            // and build up a CryptoAttribute
            // structure. 
            CryptoAttribute * cryptoLine;
            try {
                cryptoLine = tokenize((*iter));
            } catch (...) {
                std::cerr << "An exception occured" << std::endl;
            }      

            // Check if we have the right kind of attribute
            if (cryptoLine->tag.find("a=crypto:") != 0) {
            std::cout << cryptoLine->tag << std::endl;
            throw std::runtime_error("Bad syntax");
            }

            // Find index
            size_t tagPos;
            tagPos = cryptoLine->tag.find(":");
            if (tagPos == std::string::npos) {
            throw std::runtime_error("Bad syntax");
            }

            std::string index;
            index = cryptoLine->tag.substr(tagPos+1);

            std::cout << "index:" << index << std::endl;
            // Make sure its less than 9 digit long
            
            if (index.length() > 9) {
                throw std::runtime_error("Index too long.");            
            }
            
            int tagIndex;
            std::istringstream ss(index);
            ss >> tagIndex;
            if (ss.fail()) {
                throw std::runtime_error("Bad conversion");
            }
            
            // Check if the given crypto-suite is valid
            // by looking in our list. 
            // Extension: 1*(ALPHA / DIGIT / "_")
            int i;
            for (i = 0; i < 3; i++) {
                if (cryptoLine->cryptoSuite.compare(CryptoSuites[i].name) == 0) {
                    break;
                }
            }

            if (i == 3) {
                std::cout << "This is an unhandled extension\n" << std::endl;
            }
            
            // Parse the key-params
            // Check it starts with a valid key-method
            if (cryptoLine->keyParams.find("inline:") != 0) {
                throw std::runtime_error("Unsupported key-method\n");
            }
            
            KeyMethod method = Inline;
            
            // Find concatenated key and salt values
            
            
       }
       
    
    }
    
    bool SdesNegotiator::negotiate(void)
    {
        parse();
    }
}
