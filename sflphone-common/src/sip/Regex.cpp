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
 *  MEstatusHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 
#include "Regex.h"

#include <sstream> 

namespace sfl {

    const int MAX_SUBSTRINGS = 30;
    
    Regex::Regex(const std::string& pattern) :
    _pattern(pattern)
    ,_re(NULL)
    ,_pcreOutputVector(NULL)
    {   
        // Compile the pattern
        int offset;
        const char * error;
        
        _re = pcre_compile(_pattern.c_str(), 0, &error, &offset, NULL);
    
        if (_re == NULL) {
            std::string offsetStr;
            std::stringstream ss;
            ss << offset;
            offsetStr = ss.str();
            
            std::string msg("PCRE compiling failed at offset ");

            throw compile_error(msg);
        }
        
        // Allocate space for 
        // the output vector    
        _pcreOutputVector = new int[MAX_SUBSTRINGS];
    }

    Regex::~Regex() 
    {
        pcre_free(_re);
        delete[] _pcreOutputVector;
    }
    
    const std::vector<std::string>& Regex::findall(const std::string& subject)
    {
        // Execute the PCRE regex
        int status;
                
        status = pcre_exec(_re, NULL, subject.c_str(), subject.length(), 
                        0, 0, _pcreOutputVector, MAX_SUBSTRINGS);

        // Handle error cases
        if (status < 0) {
            
            // Free the regular expression 
            pcre_free(_re);
            
            // Throw and exception
            switch(status) {
                case PCRE_ERROR_NOMATCH: 
                    throw match_error("No match");
                    break;
                default:
                    std::string statusStr;
                    std::stringstream ss;
                    ss << status - 1;
                    statusStr = ss.str();  
                    
                    throw match_error(std::string("Matching error number ") + 
                                      statusStr + std::string(" occured"));
                    break;
            }

        }
        // Output_vector  isn't big enough
        if (status == 0) {
        
            status = MAX_SUBSTRINGS/3;
            
            std::string statusStr;
            std::stringstream ss;
            ss << status - 1;
            statusStr = ss.str(); 
                     
            throw std::length_error(std::string("Output vector is not big enough. Has room for") 
                  +  statusStr + std::string("captured substrings\n"));    
        }            
        
        // Copy the contents to the std::vector that will be 
        // handed to the user
        int count = status;      
        const char **stringlist;     
        
        status = pcre_get_substring_list(subject.c_str(), _pcreOutputVector, count, &stringlist);
        if (status < 0) {
            fprintf(stderr, "Get substring list failed");
        } else {
            _outputVector.clear();
            
            int i;
            for (i = 0; i < count; i++) {
                _outputVector.push_back(stringlist[i]);
            }
                        
            pcre_free_substring_list(stringlist);

        }        
        
        return _outputVector;
    }
    
    std::vector<std::string>::iterator Regex::finditer(const std::string& subject)
    {
        findall(subject);   
        std::vector<std::string>::iterator iter = _outputVector.begin();
        return iter;
    }

}
