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

#include "configurationtest.h"
#include "manager.h"
#include "config/yamlemitter.h"
#include "config/yamlparser.h"
#include "account.h"
#include "account_schema.h"
#include "logger.h"
#include "audio/alsa/alsalayer.h"
#include "audio/pulseaudio/pulselayer.h"
#include "sip/sipaccount.h"
#include "test_utils.h"

void ConfigurationTest::testYamlEmitter()
{
    using namespace Conf;
    MappingNode accountmap(NULL);
    MappingNode credentialmap(NULL);
    MappingNode srtpmap(NULL);
    MappingNode zrtpmap(NULL);
    MappingNode tlsmap(NULL);

    ScalarNode id("Account:1278432417");
    ScalarNode username("181");
    ScalarNode password("pass181");
    ScalarNode alias("sfl-181");
    ScalarNode hostname("192.168.50.3");
    ScalarNode enable(true);
    ScalarNode type("SIP");
    ScalarNode expire("3600");
    ScalarNode interface("default");
    ScalarNode port("5060");
    ScalarNode mailbox("97");
    ScalarNode publishAddr("192.168.50.182");
    ScalarNode publishPort("5060");
    ScalarNode sameasLocal(true);
    ScalarNode codecs("0/9/110/111/112/");
    ScalarNode stunServer("");
    ScalarNode stunEnabled(false);
    ScalarNode displayName("Alexandre Savard");
    ScalarNode dtmfType("sipinfo");

    ScalarNode count("0");

    ScalarNode srtpenabled(false);
    ScalarNode keyExchange("sdes");
    ScalarNode rtpFallback(false);

    ScalarNode displaySas(false);
    ScalarNode displaySasOnce(false);
    ScalarNode helloHashEnabled(false);
    ScalarNode notSuppWarning(false);

    ScalarNode tlsport("");
    ScalarNode certificate("");
    ScalarNode calist("");
    ScalarNode ciphers("");
    ScalarNode tlsenabled(false);
    ScalarNode tlsmethod("TLSV1");
    ScalarNode timeout("0");
    ScalarNode tlspassword("");
    ScalarNode privatekey("");
    ScalarNode requirecertif(true);
    ScalarNode server("");
    ScalarNode verifyclient(true);
    ScalarNode verifyserver(true);

    accountmap.setKeyValue(Account::ALIAS_KEY, &alias);
    accountmap.setKeyValue(Account::TYPE_KEY, &type);
    accountmap.setKeyValue(Account::ID_KEY, &id);
    accountmap.setKeyValue(Account::USERNAME_KEY, &username);
    accountmap.setKeyValue(Account::PASSWORD_KEY, &password);
    accountmap.setKeyValue(Account::HOSTNAME_KEY, &hostname);
    accountmap.setKeyValue(Account::ACCOUNT_ENABLE_KEY, &enable);
    accountmap.setKeyValue(Account::MAILBOX_KEY, &mailbox);
    accountmap.setKeyValue(Preferences::REGISTRATION_EXPIRE_KEY, &expire);
    accountmap.setKeyValue(INTERFACE_KEY, &interface);
    accountmap.setKeyValue(PORT_KEY, &port);
    accountmap.setKeyValue(PUBLISH_ADDR_KEY, &publishAddr);
    accountmap.setKeyValue(PUBLISH_PORT_KEY, &publishPort);
    accountmap.setKeyValue(SAME_AS_LOCAL_KEY, &sameasLocal);
    accountmap.setKeyValue(DTMF_TYPE_KEY, &dtmfType);
    accountmap.setKeyValue(Account::DISPLAY_NAME_KEY, &displayName);

    accountmap.setKeyValue(SRTP_KEY, &srtpmap);
    srtpmap.setKeyValue(SRTP_ENABLE_KEY, &srtpenabled);
    srtpmap.setKeyValue(KEY_EXCHANGE_KEY, &keyExchange);
    srtpmap.setKeyValue(RTP_FALLBACK_KEY, &rtpFallback);

    accountmap.setKeyValue(ZRTP_KEY, &zrtpmap);
    zrtpmap.setKeyValue(DISPLAY_SAS_KEY, &displaySas);
    zrtpmap.setKeyValue(DISPLAY_SAS_ONCE_KEY, &displaySasOnce);
    zrtpmap.setKeyValue(HELLO_HASH_ENABLED_KEY, &helloHashEnabled);
    zrtpmap.setKeyValue(NOT_SUPP_WARNING_KEY, &notSuppWarning);

    accountmap.setKeyValue(CRED_KEY, &credentialmap);
    SequenceNode credentialseq(NULL);
    accountmap.setKeyValue(CRED_KEY, &credentialseq);

    MappingNode credmap1(NULL);
    MappingNode credmap2(NULL);
    ScalarNode user1("user");
    ScalarNode pass1("pass");
    ScalarNode realm1("*");
    ScalarNode user2("john");
    ScalarNode pass2("doe");
    ScalarNode realm2("fbi");
    credmap1.setKeyValue(CONFIG_ACCOUNT_USERNAME, &user1);
    credmap1.setKeyValue(CONFIG_ACCOUNT_PASSWORD, &pass1);
    credmap1.setKeyValue(CONFIG_ACCOUNT_REALM, &realm1);
    credmap2.setKeyValue(CONFIG_ACCOUNT_USERNAME, &user2);
    credmap2.setKeyValue(CONFIG_ACCOUNT_PASSWORD, &pass2);
    credmap2.setKeyValue(CONFIG_ACCOUNT_REALM, &realm2);
    credentialseq.addNode(&credmap1);
    credentialseq.addNode(&credmap2);

    accountmap.setKeyValue(TLS_KEY, &tlsmap);
    tlsmap.setKeyValue(TLS_PORT_KEY, &tlsport);
    tlsmap.setKeyValue(CERTIFICATE_KEY, &certificate);
    tlsmap.setKeyValue(CALIST_KEY, &calist);
    tlsmap.setKeyValue(CIPHERS_KEY, &ciphers);
    tlsmap.setKeyValue(TLS_ENABLE_KEY, &tlsenabled);
    tlsmap.setKeyValue(METHOD_KEY, &tlsmethod);
    tlsmap.setKeyValue(TIMEOUT_KEY, &timeout);
    tlsmap.setKeyValue(TLS_PASSWORD_KEY, &tlspassword);
    tlsmap.setKeyValue(PRIVATE_KEY_KEY, &privatekey);
    tlsmap.setKeyValue(REQUIRE_CERTIF_KEY, &requirecertif);
    tlsmap.setKeyValue(SERVER_KEY, &server);
    tlsmap.setKeyValue(VERIFY_CLIENT_KEY, &verifyclient);
    tlsmap.setKeyValue(VERIFY_SERVER_KEY, &verifyserver);

    try {
        YamlEmitter emitter("/tmp/ymlEmiter.txt");

        emitter.serializeAccount(&accountmap);
        emitter.serializeAccount(&accountmap);
        emitter.serializeData();

    } catch (const YamlEmitterException &e) {
       ERROR("ConfigTree: %s", e.what());
    }
}

void ConfigurationTest::test_expand_path(void){
  const std::string pattern_1 = "~";
  const std::string pattern_2 = "~/x";
  const std::string pattern_3 = "~foo/x"; // deliberately broken,
                                          // tilde should not be expanded
  std::string home = fileutils::get_home_dir();

  CPPUNIT_ASSERT(fileutils::expand_path(pattern_1) == home);
  CPPUNIT_ASSERT(fileutils::expand_path(pattern_2) == home.append("/x"));
  CPPUNIT_ASSERT(fileutils::expand_path(pattern_3) == "~foo/x");
}
