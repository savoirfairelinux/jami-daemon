/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

using std::cout;
using std::endl;

void ConfigurationTest::testDefaultValueAudio() {
	_debug ("-------------------- ConfigurationTest::testDefaultValueAudio() --------------------\n");

	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, ALSA_CARD_ID_IN) == ALSA_DFT_CARD);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, ALSA_CARD_ID_OUT) == ALSA_DFT_CARD);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, AUDIO_SAMPLE_RATE) == DFT_SAMPLE_RATE);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, ALSA_FRAME_SIZE) == DFT_FRAME_SIZE);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, ALSA_PLUGIN) == PCM_DEFAULT);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, VOLUME_SPKR) == DFT_VOL_SPKR_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (AUDIO, VOLUME_MICRO) == DFT_VOL_MICRO_STR);
}

void ConfigurationTest::testDefaultValuePreferences() {
	_debug ("-------------------- ConfigurationTest::testDefaultValuePreferences --------------------\n");

	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, ZONE_TONE) == DFT_ZONE);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_DIALPAD) == NO_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_RINGTONE) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_SEARCHBAR) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_START) == NO_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_POPUP) == NO_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_NOTIFY) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_MAIL_NOTIFY) == NO_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_VOLUME) == NO_STR);
	//CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, REGISTRATION_EXPIRE) == DFT_EXPIRE_VALUE);
	//CPPUNIT_ASSERT (Manager::instance().getConfigString (PREFERENCES, CONFIG_AUDIO) == DFT_AUDIO_MANAGER);

}

void ConfigurationTest::testDefaultValueSignalisation() {
	_debug ("-------------------- ConfigurationTest::testDefaultValueSignalisation --------------------\n");

	CPPUNIT_ASSERT (Manager::instance().getConfigString (SIGNALISATION , SYMMETRIC) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (SIGNALISATION , PLAY_DTMF) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (SIGNALISATION , PLAY_TONES) == YES_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (SIGNALISATION , PULSE_LENGTH) == DFT_PULSE_LENGTH_STR);
	CPPUNIT_ASSERT (Manager::instance().getConfigString (SIGNALISATION , SEND_DTMF_AS) == SIP_INFO_STR);
}

void ConfigurationTest::testLoadSIPAccount() {
	_debug ("-------------------- ConfigurationTest::testLoadSIPAccount --------------------\n");

	AccountMap accounts;
	Account *current;
	std::ostringstream ss;
	int nb_account; // Must be 1

	// Load the account from the user file
	nb_account = Manager::instance().loadAccountMap();
	CPPUNIT_ASSERT_EQUAL (1, nb_account);
	// Save the account information
	accounts = Manager::instance()._accountMap;

	AccountMap::iterator iter = accounts.begin();
	CPPUNIT_ASSERT (Manager::instance().accountExists (iter->first) == true);

	while (iter != accounts.end()) {
		current = iter->second;
		CPPUNIT_ASSERT (iter->first == current->getAccountID());
		CPPUNIT_ASSERT (0 == current->getVoIPLink());
		iter++;
	}
}

void ConfigurationTest::testUnloadSIPAccount() {
	_debug ("-------------------- ConfigurationTest::testUnloadSIPAccount --------------------\n");

	AccountMap accounts;

	// Load the accounts from the user file
	Manager::instance().loadAccountMap();
	// Unload the accounts
	Manager::instance().unloadAccountMap();
	// Save the account information
	accounts = Manager::instance()._accountMap;

	AccountMap::iterator iter = accounts.begin();
	CPPUNIT_ASSERT (Manager::instance().accountExists (iter->first) == false);

	if (iter != accounts.end()) {
		CPPUNIT_FAIL ("Unload account map failed\n");
	}
}

void ConfigurationTest::testInitVolume() {
	_debug ("-------------------- ConfigurationTest::testInitVolume --------------------\n");

	Manager::instance().initVolume();

	CPPUNIT_ASSERT (Manager::instance().getConfigInt (AUDIO, VOLUME_SPKR) == Manager::instance().getSpkrVolume());
	CPPUNIT_ASSERT (Manager::instance().getConfigInt (AUDIO, VOLUME_MICRO) == Manager::instance().getMicVolume());
}

void ConfigurationTest::testInitAudioDriver() {
	_debug ("-------------------- ConfigurationTest::testInitAudioDriver --------------------\n");

	// Load the audio driver
	Manager::instance().initAudioDriver();

	// Check the creation

	if (Manager::instance().getAudioDriver() == NULL)
		CPPUNIT_FAIL ("Error while loading audio layer");

	// Check if it has been created with the right type
	if (Manager::instance().getConfigInt(PREFERENCES, CONFIG_AUDIO) == ALSA)
		CPPUNIT_ASSERT_EQUAL (Manager::instance().getAudioDriver()->getLayerType(), ALSA);
	else if (Manager::instance().getConfigInt(PREFERENCES, CONFIG_AUDIO)
			== PULSEAUDIO)
		CPPUNIT_ASSERT_EQUAL (Manager::instance().getAudioDriver()->getLayerType(), PULSEAUDIO);
	else
		CPPUNIT_FAIL ("Wrong audio layer type");
}


void ConfigurationTest::testYamlParser() 
{

  Conf::YamlParser *parser;
  try {

    parser = new Conf::YamlParser("sequence2.yml");
  
    parser->serializeEvents();

    parser->composeEvents();

    parser->constructNativeData();
  
    delete parser;
    parser = NULL;

  }
  catch (Conf::YamlParserException &e) {
    _error("ConfigTree: %s", e.what());
  }
  
}

void ConfigurationTest::testYamlEmitter()
{
  Conf::YamlEmitter *emitter;

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
  Conf::ScalarNode enable("true");
  Conf::ScalarNode type("SIP");
  Conf::ScalarNode expire("3600");
  Conf::ScalarNode interface("default");
  Conf::ScalarNode port("5060");
  Conf::ScalarNode mailbox("97");
  Conf::ScalarNode publishAddr("192.168.50.182");
  Conf::ScalarNode publishPort("5060");
  Conf::ScalarNode sameasLocal("true");
  Conf::ScalarNode resolveOnce("false");      
  Conf::ScalarNode codecs("0/9/110/111/112/");
  Conf::ScalarNode stunServer("stun.sflphone.org");
  Conf::ScalarNode stunEnabled("false");
  Conf::ScalarNode displayName("Alexandre Savard");
  Conf::ScalarNode dtmfType("sipinfo");

  Conf::ScalarNode count("0");
  
  Conf::ScalarNode srtpenabled("false");
  Conf::ScalarNode keyExchange("sdes");
  Conf::ScalarNode rtpFallback("false");

  Conf::ScalarNode displaySas("false");
  Conf::ScalarNode displaySasOnce("false");
  Conf::ScalarNode helloHashEnabled("false");
  Conf::ScalarNode notSuppWarning("false");

  Conf::ScalarNode tlsport("");
  Conf::ScalarNode certificate("");
  Conf::ScalarNode calist("");
  Conf::ScalarNode ciphers("");
  Conf::ScalarNode tlsenabled("false");
  Conf::ScalarNode tlsmethod("TLSV1");
  Conf::ScalarNode timeout("0");
  Conf::ScalarNode tlspassword("");
  Conf::ScalarNode privatekey("");
  Conf::ScalarNode requirecertif("true");
  Conf::ScalarNode server("");
  Conf::ScalarNode verifyclient("true");
  Conf::ScalarNode verifyserver("true");

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
  accountmap.setKeyValue(resolveOnceKey, &resolveOnce);
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
  credentialmap.setKeyValue(credentialCountKey, &count);

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

  try{
    emitter = new Conf::YamlEmitter("/tmp/sequenceEmiter.txt");

    emitter->writeAccount(&accountmap);
    emitter->writeAccount(&accountmap);
    emitter->serializeData();

    delete emitter;
  }
  catch (Conf::YamlEmitterException &e) {
    _error("ConfigTree: %s", e.what());
  }

}
