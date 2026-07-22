#include "cppunit/TestAssert.h"
#include "cppunit/TestFixture.h"
#include "cppunit/extensions/HelperMacros.h"

#include "media/audio/portaudio/audio_device_monitor.h"

#include "../../../test_runner.h"

namespace jami {
namespace test {

class AudioDeviceMonitorTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "audio_device_monitor"; }

private:
    void testNullPropertyStore();

    CPPUNIT_TEST_SUITE(AudioDeviceMonitorTest);
    CPPUNIT_TEST(testNullPropertyStore);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioDeviceMonitorTest, AudioDeviceMonitorTest::name());

void
AudioDeviceMonitorTest::testNullPropertyStore()
{
    CComPtr<IPropertyStore> props;
    CPPUNIT_ASSERT(GetFriendlyNameFromPropertyStore(props).empty());
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::AudioDeviceMonitorTest::name());
