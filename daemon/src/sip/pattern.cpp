/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include "pattern.h"
#include <sstream>
#include <cstdio>

namespace sfl {

Pattern::Pattern(const std::string& pattern, bool matchGlobally) :
    pattern_(pattern),
    subject_(),
    re_(NULL),
    ovector_(),
    offset_{0, 0},
    count_(0),
    matchGlobally_(matchGlobally)
{
    // Compile the pattern
    int offset;
    const char * error;

    re_ = pcre_compile(pattern_.c_str(), 0, &error, &offset, NULL);

    if (re_ == NULL) {
        std::string offsetStr;
        std::stringstream ss;
        ss << offset;
        offsetStr = ss.str();

        std::string msg("PCRE compiling failed at offset " + offsetStr);

        throw CompileError(msg);
    }

    // Allocate an appropriate amount
    // of memory for the output vector.
    int captureCount;
    pcre_fullinfo(re_, NULL, PCRE_INFO_CAPTURECOUNT, &captureCount);

    ovector_.clear();
    ovector_.resize((captureCount + 1) * 3);
}

Pattern::~Pattern()
{
    if (re_ != NULL)
        pcre_free(re_);
}


std::string Pattern::group(const char *groupName)
{
    const char * stringPtr = NULL;
    int rc = pcre_get_named_substring(re_, subject_.substr(offset_[0]).c_str(),
                                      &ovector_[0], count_, groupName,
                                      &stringPtr);

    if (rc < 0) {
        switch (rc) {
            case PCRE_ERROR_NOSUBSTRING:
                break;

            case PCRE_ERROR_NOMEMORY:
                throw MatchError("Memory exhausted.");

            default:
                throw MatchError("Failed to get named substring.");
        }
    }
    std::string matchedStr;
    if (stringPtr) {
        matchedStr = stringPtr;
        pcre_free_substring(stringPtr);
    }
    return matchedStr;
}

size_t Pattern::start() const
{
    return ovector_[0] + offset_[0];
}

size_t Pattern::end() const
{
    return (ovector_[1] - 1) + offset_[0];
}

bool Pattern::matches()
{
    // Try to find a match for this pattern
    int rc = pcre_exec(re_, NULL, subject_.substr(offset_[1]).c_str(),
                       subject_.length() - offset_[1], 0, 0, &ovector_[0],
                       ovector_.size());

    // Matching failed.
    if (rc < 0) {
        offset_[0] = offset_[1] = 0;
        return false;
    }

    // Handle the case if matching should be done globally
    if (matchGlobally_) {
        offset_[0] = offset_[1];
        // New offset is old offset + end of relative offset
        offset_[1] =  ovector_[1] + offset_[0];
    }

    // Matching succeded but not enough space.
    // @TODO figure out something more clever to do in this case.
    if (rc == 0)
        throw MatchError("No space to store all substrings.");

    // Matching succeeded. Keep the number of substrings for
    // subsequent calls to group().
    count_ = rc;
    return true;
}

std::vector<std::string> Pattern::split()
{
    size_t tokenEnd = -1;
    size_t tokenStart = 0;
    std::vector<std::string> substringSplitted;
    while (matches()) {
        tokenStart = start();
        substringSplitted.push_back(subject_.substr(tokenEnd + 1,
                                    tokenStart - tokenEnd - 1));
        tokenEnd = end();
    }

    substringSplitted.push_back(subject_.substr(tokenEnd + 1,
                                                tokenStart - tokenEnd - 1));
    return substringSplitted;
}
}
