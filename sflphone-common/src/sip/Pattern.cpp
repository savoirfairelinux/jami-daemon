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

#include "Pattern.h"
#include <sstream>
#include <cstdio>

namespace sfl
{

Pattern::Pattern (const std::string& pattern, const std::string& options) :
        _pattern (pattern),
	_re (NULL),
        _ovector (NULL),
        _ovectorSize (0),
        _count (0),
        _options (0)
{

    // printf("Pattern constructor called for %s!\n", pattern.c_str());
    // Set offsets
    _offset[0] = _offset[1] = 0;

    // Set options.
    _optionsDescription = options;

    for (unsigned int i = 0; i < options.length(); i++) {
        switch (options.at (i)) {

            case 'i':
                _options |= PCRE_CASELESS;
                break;

            case 'm':
                _options |= PCRE_MULTILINE;
                break;

            case 's':
                _options |= PCRE_DOTALL;
                break;

            case 'x':
                _options |= PCRE_EXTENDED;
                break;
        }
    }

    // Compile the pattern.
    compile();
}

Pattern::~Pattern()
{
    if (_re != NULL) {
        pcre_free (_re);
    }

    delete[] _ovector;
}

void Pattern::compile (void)
{
    // Compile the pattern
    int offset;
    const char * error;

    _re = pcre_compile (_pattern.c_str(), 0, &error, &offset, NULL);

    if (_re == NULL) {
        std::string offsetStr;
        std::stringstream ss;
        ss << offset;
        offsetStr = ss.str();

        std::string msg ("PCRE compiling failed at offset " + offsetStr);

        throw compile_error (msg);
    }

    // Allocate an appropriate amount
    // of memory for the output vector.
    int captureCount;

    pcre_fullinfo (_re, NULL, PCRE_INFO_CAPTURECOUNT, &captureCount);

    delete[] _ovector;

    _ovector = new int[ (captureCount + 1) *3];

    _ovectorSize = (captureCount + 1) * 3;
}

unsigned int Pattern::getCaptureGroupCount (void)
{
    int captureCount;
    pcre_fullinfo (_re, NULL, PCRE_INFO_CAPTURECOUNT, &captureCount);
    return captureCount;
}

std::vector<std::string> Pattern::groups (void)
{
    const char ** stringList;

    pcre_get_substring_list (_subject.c_str(),
                             _ovector,
                             _count,
                             &stringList);

    std::vector<std::string> matchedSubstrings;
    int i = 1;

    while (stringList[i] != NULL) {
        matchedSubstrings.push_back (stringList[i]);
        // printf ("Substr: <start>%s<end>", stringList[i]);
        i++;
    }

    pcre_free_substring_list (stringList);

    return matchedSubstrings;
}

std::string Pattern::group (int groupNumber)
{
    const char * stringPtr;

    int rc = pcre_get_substring (
                 _subject.substr (_offset[0]).c_str(),
                 _ovector,
                 _count,
                 groupNumber,
                 &stringPtr);

    if (rc < 0) {
        switch (rc) {

            case PCRE_ERROR_NOSUBSTRING:
                throw std::out_of_range ("Invalid group reference.");

            case PCRE_ERROR_NOMEMORY:
                throw match_error ("Memory exhausted.");

            default:
                throw match_error ("Failed to get named substring.");
        }
    }

    std::string matchedStr (stringPtr);

    pcre_free_substring (stringPtr);

    return matchedStr;
}

std::string Pattern::group (const std::string& groupName)
{
    const char * stringPtr = NULL;

    int rc = pcre_get_named_substring (
                 _re,
                 _subject.substr (_offset[0]).c_str(),
                 _ovector,
                 _count,
                 groupName.c_str(),
                 &stringPtr);

    if (rc < 0) {
        switch (rc) {

            case PCRE_ERROR_NOSUBSTRING:
	        
		break;

            case PCRE_ERROR_NOMEMORY:
                throw match_error ("Memory exhausted.");

            default:
                throw match_error ("Failed to get named substring.");
        }
    }

    std::string matchedStr;

    if(stringPtr) {

	matchedStr = stringPtr;
	pcre_free_substring (stringPtr);
    }
    else {

         matchedStr = "";
    }

    return matchedStr;

}

void Pattern::start (const std::string& groupName) const
{
    int index = pcre_get_stringnumber (_re, groupName.c_str());
    start (index);
}

size_t Pattern::start (unsigned int groupNumber) const
{
  if (groupNumber <= (unsigned int)_count) {
        return _ovector[ (groupNumber + 1) * 2];
    } else {
        throw std::out_of_range ("Invalid group reference.");
    }
}

size_t Pattern::start (void) const
{
    return _ovector[0] + _offset[0];
}

void Pattern::end (const std::string& groupName) const
{
    int index = pcre_get_stringnumber (_re, groupName.c_str());
    end (index);
}

size_t Pattern::end (unsigned int groupNumber) const
{
  if (groupNumber <= (unsigned int)_count) {
        return _ovector[ ( (groupNumber + 1) * 2) + 1 ] - 1;
    } else {
        throw std::out_of_range ("Invalid group reference.");
    }
}

size_t Pattern::end (void) const
{
    return (_ovector[1] - 1) + _offset[0];
}

bool Pattern::matches (void) throw (match_error)
{
    return matches (_subject);
}

bool Pattern::matches (const std::string& subject) throw (match_error)
{

    // Try to find a match for this pattern
    int rc = pcre_exec (
                 _re,
                 NULL,
                 subject.substr (_offset[1]).c_str(),
                 subject.length() - _offset[1],
                 0,
                 _options,
                 _ovector,
                 _ovectorSize);

  

    // Matching failed.
    if (rc < 0) {
        _offset[0] = _offset[1] = 0;
        // printf("  Matching failed with %d\n", rc);
        return false;
    }

    // Handle the case if matching should be done globally
    if (_optionsDescription.find ("g") != std::string::npos) {
        _offset[0] = _offset[1];
        // New offset is old offset + end of relative offset
        _offset[1] =  _ovector[1] + _offset[0];
    }

    // Matching succeded but not enough space.
    if (rc == 0) {
        throw match_error ("No space to store all substrings.");
        // @TODO figure out something more clever to do in that case.
    }

    // Matching succeeded. Keep the number of substrings for
    // subsequent calls to group().
      _count = rc;

    return true;
}

std::vector<std::string> Pattern::split (void)
{
    size_t tokenEnd = -1;
    size_t tokenStart = 0;

    std::vector<std::string> substringSplitted;

    while (matches()) {
        tokenStart = start();
        substringSplitted.push_back (_subject.substr (tokenEnd + 1,
                                     tokenStart - tokenEnd - 1));
	// printf("split: %s\n", _subject.substr (tokenEnd + 1,
	// 					 tokenStart - tokenEnd - 1).c_str());
        tokenEnd = end();
    }

    substringSplitted.push_back (_subject.substr (tokenEnd + 1, tokenStart - tokenEnd - 1));

    return substringSplitted;
}
}


