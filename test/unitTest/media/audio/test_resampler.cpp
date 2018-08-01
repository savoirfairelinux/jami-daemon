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
#include "audio/resampler.h"

#include "../test_runner.h"

namespace ring { namespace test {

class ResamplerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "resampler"; }

    void setUp();
    void tearDown();

private:
    void testResample();

    CPPUNIT_TEST_SUITE(ResamplerTest);
    CPPUNIT_TEST(testResample);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<Resampler> resampler_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ResamplerTest, ResamplerTest::name());

void
ResamplerTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
}

void
ResamplerTest::tearDown()
{
    DRing::fini();
}

void
ResamplerTest::testResample()
{
    const constexpr AudioFormat none(0, 0);
    const constexpr AudioFormat infmt(44100, 1);
    const constexpr AudioFormat outfmt(48000, 2);

    resampler_.reset(new Resampler);

    AudioBuffer inbuf(1024, infmt);
    AudioBuffer outbuf(0, outfmt);

    resampler_->resample(inbuf, outbuf);
    CPPUNIT_ASSERT(outbuf.getFormat().sample_rate == 48000);
    CPPUNIT_ASSERT(outbuf.getFormat().nb_channels == 2);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::ResamplerTest::name());
