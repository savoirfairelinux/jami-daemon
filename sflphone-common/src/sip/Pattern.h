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
#ifndef __SFL_PATTERN_H__
#define __SFL_PATTERN_H__

#include <stdexcept>
#include <string>
#include <vector>
#include <pcre.h>

namespace sfl  {

    /** 
     * Exception object that is thrown when
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
     * Exception object that is thrown when
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
    
    class Pattern {
    
        public:
        
        /**
	 * Constructor for a regular expression
	 * pattern evaluator/matcher. 
	 *
	 * @param pattern 
	 *      The regular expression to 
	 *      be used for this instance.
	 */
             
         Pattern(const std::string& pattern, 
		 const std::string& options = "");
            
	 /**
	  * Destructor. Pcre pattern gets freed
	  * here.
	  */
	 ~Pattern();
            
	 /**
	  * Assignment operator overloading.
	  * Set the regular expression 
	  * to be used on subject strings
	  * and compile the regular expression 
	  * from that string. 
	  * 
	  * @param pattern The new pattern
	  */
	 void operator=(const std::string& pattern) {
	     _pattern = pattern; 
	     compile();            
	 }
            
	 void operator=(const char * pattern) {
	     _pattern = pattern; 
	     compile();            
	 }            
              
	 /**
	  * Compile the regular expression
	  * from the pattern that was set for 
	  * this object.
	  */
	 void compile(void);
             
	 /**
	  * Get the currently set regular expression 
	  * that is used on subject strings
	  * 
	  * @return The currently set pattern
	  */ 
	 inline std::string getPattern(void) { return _pattern; }
             
	 /**
	  * << operator overload. Sets the the subject
	  * for latter use on the >> operator. 
	  * 
	  * @param subject 
	  *      The expression to be evaluated
	  *      by the pattern.
	  *
	  */
	 void operator<<(const std::string& subject) {
	     _subject = subject;
	 }
			
	 /**
	  * Get the start position of the overall match.
	  * 
	  * @return the start position of the overall match.  
	  */
	 size_t start(void) const;
			
	 /**
	  * Get the start position of the specified match.
	  * 
	  * @param groupNumber The capturing group number.
	  * 
	  * @return the start position of the specified match.  
	  */
	 size_t start(unsigned int groupNumber) const;
			
	 /**
	  * Get the start position of the specified match.
	  * 
	  * @param groupName The capturing group name.
	  */
	 void start(const std::string& groupName) const;
			
	 /**
	  * Get the end position of the overall match.
	  * 
	  * @return the end position of the overall match.  
	  */			
	 size_t end(void) const;
			
	 /**
	  * Get the end position of the specified match.
	  * 
	  * @param groupNumber The capturing group number.
	  * 
	  * @return the end position of the specified match.  
	  */			
	 size_t end(unsigned int groupNumber) const;
			
	 /**
	  * Get the end position of the specified match.
	  * 
	  * @param groupName The capturing group name.
	  * 
	  * @return the end position of the specified match.  
	  */			
	 void end(const std::string& groupName) const;
			
	 /**
	  * Get the number of capturing groups in the 
	  * compiled regex. 
	  * 
	  * @return The number of capture groups.
	  * 
	  * @pre The regular expression should have been 
	  * 	    compiled prior to the execution of this method.
	  */
	 unsigned int getCaptureGroupCount(void);
			
	 /**
	  * Get the substring matched in a capturing 
	  * group (named or unnamed).
	  *  
	  * This methods only performs a basic lookup
	  * inside its internal substring table. Thus,
	  * matches() should have been called prior to 
	  * this method in order to obtain the desired 
	  * output. 
	  *
	  * @param groupName The name of the group  
	  * 
	  * @return the substring matched by the 
	  *         regular expression designated
	  *         the group name.
	  */
	 std::string group(const std::string& groupName);
         
	 /**
	  * Get the substring matched in a named group.
	  * 
	  * This methods only performs a basic lookup
	  * inside its internal substring table. Thus,
	  * matches() should have been called prior to 
	  * this method in order to obtain the desired 
	  * output. 
	  * 
	  * @param groupNumber The number of the group. 
	  * 
	  * @return the substring matched by the 
	  *         regular expression designated
	  *         the group number.
	  */
	 std::string group(int groupNumber);
	 
	 /**
	  * Similar to python's MatchObject.groups. Get all 
	  * the substrings matched by the capture groups defined
	  * in the pattern. The complete (implicit) capture group
	  * is not returned : ie only groups from 1 up to the number
	  * of groups in the pattern are returned.
	  * 
	  * @return A vector of stings that were matched by some
	  * 		   capturing group in the pattern.
	  * 
	  * @pre The regular expression should have been 
	  * 	    compiled prior to the execution of this method.
	  */
	 std::vector<std::string> groups(void);
	 
	 /**
	  * Try to match the compiled pattern with a 
	  * subject. 
	  * 
	  * @param subject Subject to be matched
	  * 		          by the pattern. 
	  * 
	  * @return true If the subject matches the pattern, 
	  *         false otherwise. 
	  * 
	  * @pre The regular expression should have been 
	  * 	    compiled prior to the execution of this method.
	  * 
	  * @post The internal substring table will be updated
	  *       with the new matches. Therefore, subsequent
	  * 		 calls to group may return different results.
	  */
	 bool matches(const std::string& subject) throw(match_error);
	 
	 /**
	  * Try to match the compiled pattern with the implicit 
	  * subject. 
	  * 
	  * @return true If the subject matches the pattern, 
	  *         false otherwise. 
	  * 
	  * @pre The regular expression should have been 
	  * 	    compiled prior to the execution of this method.
	  * 
	  * @post The internal substring table will be updated
	  *       with the new matches. Therefore, subsequent
	  * 		 calls to group may return different results.
	  */
	 bool matches(void) throw(match_error); 	
	 
	 /**
	  *  Split the subject into a list of substrings.
	  * 
	  * @return A vector of substrings. 
	  * 
	  * @pre The regular expression should have been 
	  * 	    compiled prior to the execution of this method.
	  * 
	  * @post The internal subject won't be affected by this 
	  * 	     by this operation. In other words: subject_before =
	  * 		 subject_after.
	  */
	 std::vector<std::string> split(void); // throw(match_error);		

    private:  
	 /**
	  * The regular expression that represents that pattern.
	  */
	 std::string _pattern;
	 
	 /** 
	  * The optional subject string.
	  */
	 std::string _subject;
	 
	 /**
	  * PCRE struct that
	  * contains the compiled regular
	  * expression
            */
	 pcre * _re;
	 
	 /**
	  * The internal output vector used by PCRE. 
	  */
	 int * _ovector;
	 
	 /**
	  * The size of the _ovector
	  */
	 int _ovectorSize;
			
	 /**
	  * Current offset in the _ovector;
	  */
	 
	 int _offset[2];
	 
	 /**
	  * The number of substrings matched after calling
	  * pcre_exec.
	  */ 
	 int _count;
	 
	 /**
	  * PCRE options for this pattern.
	  */
	 int _options;
	 
	 /**
	  * String representation of the options.
	  */
	 std::string _optionsDescription;
    };
}


#endif
