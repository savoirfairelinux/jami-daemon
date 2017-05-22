/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include <cstdlib>

#include "historytest.h"
#include "history/history.h"
#include "logger.h"
#include "constants.h"

namespace {
void restore()
{
    if (system("mv " HISTORY_SAMPLE_BAK " " HISTORY_SAMPLE) < 0)
        ERROR("Restoration of %s failed", HISTORY_SAMPLE);
}

void backup()
{
    if (system("cp " HISTORY_SAMPLE " " HISTORY_SAMPLE_BAK) < 0)
        ERROR("Backup of %s failed", HISTORY_SAMPLE);
}
}

namespace sfl {

void HistoryTest::setUp()
{
    backup();
    history_ = new History;
    history_->setPath(HISTORY_SAMPLE);
}


void HistoryTest::test_create_path()
{
    DEBUG("-------------------- HistoryTest::test_set_path --------------------\n");

    std::string path(HISTORY_SAMPLE);
    CPPUNIT_ASSERT(history_->path_ == path);
}

void HistoryTest::test_load_from_file()
{
    DEBUG("-------------------- HistoryTest::test_load_from_file --------------------\n");

    bool res = history_->load(HISTORY_LIMIT);
    CPPUNIT_ASSERT(res);
}

void HistoryTest::test_load_items()
{
    DEBUG("-------------------- HistoryTest::test_load_items --------------------\n");
    bool res = history_->load(HISTORY_LIMIT);
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT(history_->numberOfItems() == HISTORY_SAMPLE_SIZE);
}

void HistoryTest::test_save_to_file()
{
    DEBUG("-------------------- HistoryTest::test_save_to_file --------------------\n");
    CPPUNIT_ASSERT(history_->save());
}

void HistoryTest::test_get_serialized()
{
    DEBUG("-------------------- HistoryTest::test_get_serialized --------------------\n");
    bool res = history_->load(HISTORY_LIMIT);
    CPPUNIT_ASSERT(res);
    CPPUNIT_ASSERT(history_->getSerialized().size() == HISTORY_SAMPLE_SIZE);
}

void HistoryTest::tearDown()
{
    delete history_;
    restore();
}

}
