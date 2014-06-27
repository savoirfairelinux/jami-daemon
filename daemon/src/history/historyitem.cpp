/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
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

#include "historyitem.h"
#include <unistd.h>
#include <cstdlib>
#include <istream>

namespace sfl {

const char * const HistoryItem::ACCOUNT_ID_KEY =        "accountid";
const char * const HistoryItem::CALLID_KEY =            "callid";
const char * const HistoryItem::CONFID_KEY =            "confid";
const char * const HistoryItem::DISPLAY_NAME_KEY =      "display_name";
const char * const HistoryItem::PEER_NUMBER_KEY =       "peer_number";
const char * const HistoryItem::RECORDING_PATH_KEY =    "recordfile";
// FIXME: Deprecated
const char * const HistoryItem::STATE_KEY =             "state";
// New version:
const char * const HistoryItem::MISSED_KEY =            "missed";
const char * const HistoryItem::DIRECTION_KEY =         "direction";
const char * const HistoryItem::TIMESTAMP_START_KEY =   "timestamp_start";
const char * const HistoryItem::TIMESTAMP_STOP_KEY =    "timestamp_stop";
const char * const HistoryItem::AUDIO_CODEC_KEY =       "audio_codec";
const char * const HistoryItem::VIDEO_CODEC_KEY =       "video_codec";

const char * const HistoryItem::MISSED_STRING =         "missed";
const char * const HistoryItem::INCOMING_STRING =       "incoming";
const char * const HistoryItem::OUTGOING_STRING =       "outgoing";

using std::map;
using std::string;

static bool
file_exists(const std::string &str)
{
    return access(str.c_str(), F_OK) != -1;
}

HistoryItem::HistoryItem(const map<string, string> &args) : entryMap_(args),
    timestampStart_(std::atol(entryMap_[TIMESTAMP_START_KEY].c_str()))
{}

HistoryItem::HistoryItem(std::istream &entry) : entryMap_(), timestampStart_(0)
{
    string tmp;
    while (std::getline(entry, tmp, '\n')) {
        size_t pos = tmp.find('=');
        if (pos == string::npos)
            break;
        else if (pos < tmp.length() - 1) {
            string key(tmp.substr(0, pos));
            string val(tmp.substr(pos + 1, tmp.length() - pos - 1));
            if (key == RECORDING_PATH_KEY and not file_exists(val))
                val = "";
            entryMap_[key] = val;
        }
    }
    timestampStart_ = std::atol(entryMap_[TIMESTAMP_START_KEY].c_str());
}

map<string, string> HistoryItem::toMap() const
{
    return entryMap_;
}

bool HistoryItem::youngerThan(unsigned long otherTime) const
{
    return timestampStart_ > otherTime;
}

bool HistoryItem::hasPeerNumber() const
{
    return entryMap_.find(PEER_NUMBER_KEY) != entryMap_.end();
}

void HistoryItem::print(std::ostream &o) const
{
    // every entry starts with "[" + random integer = "]"
    for (const auto &item : entryMap_) {
        // if the file does not exist anymore, we do not save it
        if (item.first == RECORDING_PATH_KEY and not file_exists(item.second))
            o << item.first << "=" << "" << std::endl;
        else
            o << item.first << "=" << item.second << std::endl;
    }
}

std::ostream& operator << (std::ostream& o, const HistoryItem& item)
{
    item.print(o);
    return o;
}

}
