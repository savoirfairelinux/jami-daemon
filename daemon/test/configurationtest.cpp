/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <stdio.h>
#include <sstream>

#include "configurationtest.h"
#include "constants.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"

using std::cout;
using std::endl;

void ConfigurationTest::testDefaultValueAudio()
{
    _debug("-------------------- ConfigurationTest::testDefaultValueAudio() --------------------\n");

    CPPUNIT_ASSERT(Manager::instance().audioPreference.getCardin() == 0);  // ALSA_DFT_CARD);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getCardout() == 0);  // ALSA_DFT_CARD);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getSmplrate() == 44100);  // DFT_SAMPLE_RATE);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getPlugin() == PCM_DEFAULT);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getVolumespkr() == 100);
    CPPUNIT_ASSERT(Manager::instance().audioPreference.getVolumemic() == 100);
}

void ConfigurationTest::testDefaultValuePreferences()
{
    _debug("-------------------- ConfigurationTest::testDefaultValuePreferences --------------------\n");

    CPPUNIT_ASSERT(Manager::instance().preferences.getZoneToneChoice() == Preferences::DFT_ZONE);
}

void ConfigurationTest::testDefaultValueSignalisation()
{
    _debug("-------------------- ConfigurationTest::testDefaultValueSignalisation --------------------\n");

    CPPUNIT_ASSERT(Manager::instance().voipPreferences.getSymmetricRtp() == true);
    CPPUNIT_ASSERT(Manager::instance().voipPreferences.getPlayDtmf() == true);
    CPPUNIT_ASSERT(Manager::instance().voipPreferences.getPlayTones() == true);
    CPPUNIT_ASSERT(Manager::instance().voipPreferences.getPulseLength() == 250);
}

void ConfigurationTest::testInitAudioDriver()
{
    _debug("-------------------- ConfigurationTest::testInitAudioDriver --------------------\n");

    // Load the audio driver
    Manager::instance().initAudioDriver();

    // Check the creation

    if (Manager::instance().getAudioDriver() == NULL)
        CPPUNIT_FAIL("Error while loading audio layer");

    // Check if it has been created with the right type
    if (Manager::instance().audioPreference.getAudioApi() == "alsa")
        CPPUNIT_ASSERT(!dynamic_cast<PulseLayer*>(Manager::instance().getAudioDriver()));
    else if (Manager::instance().audioPreference.getAudioApi() == "pulseaudio")
        CPPUNIT_ASSERT(!dynamic_cast<AlsaLayer*>(Manager::instance().getAudioDriver()));
    else
        CPPUNIT_FAIL("Wrong audio layer type");
}


void ConfigurationTest::testYamlParser()
{
    try {
        Conf::YamlParser *parser = new Conf::YamlParser("ymlParser.yml");
        parser->serializeEvents();
        parser->composeEvents();
        parser->constructNativeData();

        delete parser;
    } catch (Conf::YamlParserException &e) {
        _error("ConfigTree: %s", e.what());
    }
}

void ConfigurationTest::testYamlEmitter()
{
    Conf::MappingNode accountmap(NULL);
    Conf::MappingNode credentialmap(NULL);
    Conf::MappingNode srtpmap(NULL);
    Conf::MappingNode zrtpmap(NULL);
    Conf::MappingNode tlsmap(NULL);


    Conf::ScalarNode id("Account:1278432417");
    Conf::ScalarNode username("181");
    Conf::ScalarNode password("pass181");
    Conf::ScalarNode alias("sfl-181");
    Conf::ScalarNode hostname("192.168.50.3");
    Conf::ScalarNode enable(true);
    Conf::ScalarNode type("SIP");
    Conf::ScalarNode expire("3600");
    Conf::ScalarNode interface("default");
    Conf::ScalarNode port("5060");
    Conf::ScalarNode mailbox("97");
    Conf::ScalarNode publishAddr("192.168.50.182");
    Conf::ScalarNode publishPort("5060");
    Conf::ScalarNode sameasLocal(true);
    Conf::ScalarNode codecs("0/9/110/111/112/");
    Conf::ScalarNode stunServer("stun.sflphone.org");
    Conf::ScalarNode stunEnabled(false);
    Conf::ScalarNode displayName("Alexandre Savard");
    Conf::ScalarNode dtmfType("sipinfo");

    Conf::ScalarNode count("0");

    Conf::ScalarNode srtpenabled(false);
    Conf::ScalarNode keyExchange("sdes");
    Conf::ScalarNode rtpFallback(false);

    Conf::ScalarNode displaySas(false);
    Conf::ScalarNode displaySasOnce(false);
    Conf::ScalarNode helloHashEnabled(false);
    Conf::ScalarNode notSuppWarning(false);

    Conf::ScalarNode tlsport("");
    Conf::ScalarNode certificate("");
    Conf::ScalarNode calist("");
    Conf::ScalarNode ciphers("");
    Conf::ScalarNode tlsenabled(false);
    Conf::ScalarNode tlsmethod("TLSV1");
    Conf::ScalarNode timeout("0");
    Conf::ScalarNode tlspassword("");
    Conf::ScalarNode privatekey("");
    Conf::ScalarNode requirecertif(true);
    Conf::ScalarNode server("");
    Conf::ScalarNode verifyclient(true);
    Conf::ScalarNode verifyserver(true);

    accountmap.setKeyValue(aliasKey, &alias);
    accountmap.setKeyValue(typeKey, &type);
    accountmap.setKeyValue(idKey, &id);
    accountmap.setKeyValue(usernameKey, &username);
    accountmap.setKeyValue(passwordKey, &password);
    accountmap.setKeyValue(hostnameKey, &hostname);
    accountmap.setKeyValue(accountEnableKey, &enable);
    accountmap.setKeyValue(mailboxKey, &mailbox);
    accountmap.setKeyValue(expireKey, &expire);
    accountmap.setKeyValue(interfaceKey, &interface);
    accountmap.setKeyValue(portKey, &port);
    accountmap.setKeyValue(publishAddrKey, &publishAddr);
    accountmap.setKeyValue(publishPortKey, &publishPort);
    accountmap.setKeyValue(sameasLocalKey, &sameasLocal);
    accountmap.setKeyValue(dtmfTypeKey, &dtmfType);
    accountmap.setKeyValue(displayNameKey, &displayName);

    accountmap.setKeyValue(srtpKey, &srtpmap);
    srtpmap.setKeyValue(srtpEnableKey, &srtpenabled);
    srtpmap.setKeyValue(keyExchangeKey, &keyExchange);
    srtpmap.setKeyValue(rtpFallbackKey, &rtpFallback);

    accountmap.setKeyValue(zrtpKey, &zrtpmap);
    zrtpmap.setKeyValue(displaySasKey, &displaySas);
    zrtpmap.setKeyValue(displaySasOnceKey, &displaySasOnce);
    zrtpmap.setKeyValue(helloHashEnabledKey, &helloHashEnabled);
    zrtpmap.setKeyValue(notSuppWarningKey, &notSuppWarning);

    accountmap.setKeyValue(credKey, &credentialmap);
    Conf::SequenceNode credentialseq(NULL);
    accountmap.setKeyValue(credKey, &credentialseq);

    Conf::MappingNode credmap1(NULL);
    Conf::MappingNode credmap2(NULL);
    Conf::ScalarNode user1("user");
    Conf::ScalarNode pass1("pass");
    Conf::ScalarNode realm1("*");
    Conf::ScalarNode user2("john");
    Conf::ScalarNode pass2("doe");
    Conf::ScalarNode realm2("fbi");
    credmap1.setKeyValue(USERNAME, &user1);
    credmap1.setKeyValue(PASSWORD, &pass1);
    credmap1.setKeyValue(REALM, &realm1);
    credmap2.setKeyValue(USERNAME, &user2);
    credmap2.setKeyValue(PASSWORD, &pass2);
    credmap2.setKeyValue(REALM, &realm2);
    credentialseq.addNode(&credmap1);
    credentialseq.addNode(&credmap2);

    accountmap.setKeyValue(tlsKey, &tlsmap);
    tlsmap.setKeyValue(tlsPortKey, &tlsport);
    tlsmap.setKeyValue(certificateKey, &certificate);
    tlsmap.setKeyValue(calistKey, &calist);
    tlsmap.setKeyValue(ciphersKey, &ciphers);
    tlsmap.setKeyValue(tlsEnableKey, &tlsenabled);
    tlsmap.setKeyValue(methodKey, &tlsmethod);
    tlsmap.setKeyValue(timeoutKey, &timeout);
    tlsmap.setKeyValue(tlsPasswordKey, &tlspassword);
    tlsmap.setKeyValue(privateKeyKey, &privatekey);
    tlsmap.setKeyValue(requireCertifKey, &requirecertif);
    tlsmap.setKeyValue(serverKey, &server);
    tlsmap.setKeyValue(verifyClientKey, &verifyclient);
    tlsmap.setKeyValue(verifyServerKey, &verifyserver);

    try {
        Conf::YamlEmitter *emitter = new Conf::YamlEmitter("/tmp/ymlEmiter.txt");

        emitter->serializeAccount(&accountmap);
        emitter->serializeAccount(&accountmap);
        emitter->serializeData();

        delete emitter;
    } catch (Conf::YamlEmitterException &e) {
        _error("ConfigTree: %s", e.what());
    }
}
