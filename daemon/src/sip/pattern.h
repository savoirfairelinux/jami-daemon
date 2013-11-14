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
#ifndef __PATTERN_H__
#define __PATTERN_H__

#include <stdexcept>
#include <string>
#include <vector>
#include <pcre.h>
#include "noncopyable.h"

namespace sfl {

/**
 * Exception object that is thrown when
 * an error occured while compiling the
 * regular expression.
 */
class CompileError : public std::invalid_argument {
    public:
        explicit CompileError(const std::string& error) :
            std::invalid_argument(error) {}
};

/**
 * Exception object that is thrown when
 * an error occured while mathing a
 * pattern to an expression.
 */
class MatchError : public std::invalid_argument {
    public:
        MatchError(const std::string& error) :
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
                bool matchGlobally);

        /**
         * Destructor. Pcre pattern gets freed
         * here.
         */
        ~Pattern();

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
        std::string group(const char *groupName);

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
        bool matches();

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
        std::vector<std::string> split();

        void updateSubject(const std::string& subject) {
            subject_ = subject;
        }

    private:
        /**
         * Get the start position of the overall match.
         *
         * @return the start position of the overall match.
         */
        size_t start() const;

        /**
         * Get the end position of the overall match.
         *
         * @return the end position of the overall match.
         */
        size_t end() const;

        NON_COPYABLE(Pattern);
         // The regular expression that represents that pattern.
        std::string pattern_;

        // The optional subject string.
        std::string subject_;

        /**
         * PCRE struct that
         * contains the compiled regular
         * expression
               */
        pcre * re_;

        // The internal output vector used by PCRE.
        std::vector<int> ovector_;

        // Current offset in the ovector_;
        int offset_[2];

        // The number of substrings matched after calling pcre_exec.
        int count_;

        bool matchGlobally_;
};
}

#endif // __PATTERN_H__

