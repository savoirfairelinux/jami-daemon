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
#ifndef __SFL_REGEX_H__
#define __SFL_REGEX_H__

#include <stdexcept> 
#include <vector>

#include <pcre.h>

namespace sfl {
    
    /**
     * While waiting for C++0x to come out
     * Let's say that we have something like
     * std::range
     *
     * Defines a pair of iterator over a vector of
     * strings. The fist element corresponds to the
     * begining of the vector, while the second is
     * set to the end.
     */
     typedef std::pair<std::vector<std::string>::iterator, std::vector<std::string>::iterator> range;
     
    /** 
     * Exception object that is throw when
     * an error occured while compiling the
     * regular expression.
     */
    class compile_error : public std::invalid_argument 
    {
        public:     
        explicit compile_error(const std::string& error) :  
        std::invalid_argument(error) {}
    };
    
    /** 
     * Exception object that is throw when
     * an error occured while mathing a
     * pattern to an expression.
     */
    class match_error : public std::invalid_argument      
    {
        public:     
        match_error(const std::string& error) :
        std::invalid_argument(error) {}
    };
     
    /**
     * This class implements in its way
     * some of the libpcre library.
     */
    
    class Regex {
    
        public:
        
            /**
             * Constructor for a regular expression
             * pattern evaluator/matcher. 
             *
             * @param pattern 
             *      The regular expression to 
             *      be used for this instance.
             */
            Regex(const std::string& pattern);
            
            ~Regex();
            
            /** 
             * Match the given expression against
             * this pattern and returns a vector of
             * the substrings that were matched.
             *
             * @param subject 
             *      The expression to be evaluated
             *      by the pattern.
             *
             * @return a vector containing the substrings
             *       in the order that the parentheses were
             *       defined. Throws a match_error if the 
             *       expression cannot be matched.
             */ 
            const std::vector<std::string>& findall(const std::string& subject);

            /** 
             * Match the given expression against
             * this pattern and returns an iterator
             * to the substrings.
             *
             * @param subject 
             *      The expression to be evaluated
             *      by the pattern.
             *
             * @return an iterator to the output vector
             *         containing the substrings that 
             *         were matched.
             */             
            range finditer(const std::string& subject);
            
        private:
            
            /**
            * The regular expression that represents that pattern
            */

            std::string _pattern;

            /**
            * The pcre regular expression structure
            */
            pcre * _re;

            /**
            * The output vector used to contain
            * substrings that were matched by pcre.
            */
            int * _pcreOutputVector;

            /**
            * The output std::vector used to contain 
            * substrings that were matched by pcre.
            */

            std::vector<std::string> _outputVector;
    };
    
}

#endif
