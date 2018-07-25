/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
 */

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "dring.h"
#include "libav_deps.h"
#include "media_decoder.h"
#include "media_device.h"
#include "media_io_handle.h"

#include "../../test_runner.h"

namespace ring { namespace test {

class MediaDecoderTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_decoder"; }

    void setUp();
    void tearDown();

private:
    void testFileDecode();

    CPPUNIT_TEST_SUITE(MediaDecoderTest);
    CPPUNIT_TEST(testFileDecode);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaDecoder> decoder_;
    std::unique_ptr<MediaIOHandle> ioHandle_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaDecoderTest, MediaDecoderTest::name());

void
MediaDecoderTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
    decoder_.reset(new MediaDecoder);
}

void
MediaDecoderTest::tearDown()
{
    DRing::fini();
}

void
MediaDecoderTest::testFileDecode()
{
    DeviceParams dev;
    dev.input = "/home/pgorley/default.wav";
    CPPUNIT_ASSERT(decoder_->openInput(dev) >= 0);
    CPPUNIT_ASSERT(decoder_->setupStream(AVMEDIA_TYPE_AUDIO) >= 0);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaDecoderTest::name());
