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
#ifndef __SFL_SDES_NEGOTIATOR_H__
#define __SFL_SDES_NEGOTIATOR_H__

#include <stdexcept>
#include <string> 
#include <vector>

namespace sfl {

    /** 
     * General exception object that is thrown when
     * an error occured with a regular expression
     * operation.
     */
    class parse_error : public std::invalid_argument 
    {
        public:     
        explicit parse_error(const std::string& error) :  
        std::invalid_argument(error) {}
    };
    
    enum CipherMode {
        AESCounterMode,
        AESF8Mode
    };
    
    enum MACMode {
        HMACSHA1
    };
    
    enum KeyMethod {
        Inline
        // url, maybe at some point
    };
    
    struct CryptoSuiteDefinition {
        char * name;
        int masterKeyLength;
        int masterSaltLength;
        int srtpLifetime;
        int srtcpLifetime;
        CipherMode cipher;
        int encryptionKeyLength;
        MACMode mac;
        int srtpAuthTagLength;
        int srtcpAuthTagLength;
        int srtpAuthKeyLength;
        int srtcpAuthKeyLen;
    };
    
    /** 
    * List of accepted Crypto-Suites
    * as defined in RFC4568 (6.2)
    */
    const CryptoSuiteDefinition CryptoSuites[3] = { 
          {"AES_CM_128_HMAC_SHA1_80", 128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 80, 80, 160, 160 },
          {"AES_CM_128_HMAC_SHA1_32", 128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 32, 80, 160, 160 },
          {"F8_128_HMAC_SHA1_80", 128, 112, 48, 31, AESF8Mode, 128, HMACSHA1, 80, 80, 160, 160 } };   
        
    /** 
     * Internal structure 
     * used during parsing.
     */  
    struct CryptoAttribute;                    

    class SdesNegotiator 
    {
        /**
         * Constructor for an SDES crypto attributes
         * negotiator. 
         *
         * @param attribute 
         *       A vector of crypto attributes as defined in 
         *       RFC4568. This string will be parsed
         *       and a crypto context will be created
         *       from it. 
         */
         
        public:
            SdesNegotiator(const std::vector<CryptoSuiteDefinition>& localCapabilites, const std::vector<std::string>& remoteAttribute);
            ~SdesNegotiator() { };
            
        public:
            bool negotiate(void);

        private:
            /**
             * A vector list containing the remote attributes.
             * Multiple crypto lines can be sent, and the
             * prefered method is then chosen from that list.
             */
            std::vector<std::string> _remoteAttribute;
            std::vector<CryptoSuiteDefinition> _localCapabilities;
            


        private:
            void parse(void);
            CryptoAttribute * tokenize(const std::string& attributeLine);
    };
}
#endif
