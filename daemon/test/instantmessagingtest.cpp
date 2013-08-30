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

#include <iostream>
#include <fstream>
#include <expat.h>
#include "test_utils.h"

#include "instantmessagingtest.h"
#include "im/instant_messaging.h"
#include "logger.h"

#define MAXIMUM_SIZE	10
#define DELIMITER_CHAR	"\n\n"

using namespace sfl::InstantMessaging;

void InstantMessagingTest::testSaveSingleMessage()
{
    TITLE();
    std::string callID = "testfile1.txt";
    std::string filename = "im:";

    // Open a file stream and try to write in it
    CPPUNIT_ASSERT(saveMessage("Bonjour, c'est un test d'archivage de message", "Manu", callID, std::ios::out));

    filename.append(callID);
    // Read it to check it has been successfully written
    std::ifstream testfile(filename.c_str(), std::ios::in);
    CPPUNIT_ASSERT(testfile.is_open());

    std::string input;
    while (!testfile.eof()) {
        std::string tmp;
        std::getline(testfile, tmp);
        input.append(tmp);
    }

    testfile.close();
    CPPUNIT_ASSERT(input == "[Manu] Bonjour, c'est un test d'archivage de message");
}

void InstantMessagingTest::testSaveMultipleMessage()
{
    TITLE();

    std::string callID = "testfile2.txt";
    std::string filename = "im:";

    // Open a file stream and try to write in it
    CPPUNIT_ASSERT(saveMessage("Bonjour, c'est un test d'archivage de message", "Manu", callID, std::ios::out));
    CPPUNIT_ASSERT(saveMessage("Cool", "Alex", callID, std::ios::out || std::ios::app));

    filename.append(callID);
    // Read it to check it has been successfully written
    std::ifstream testfile(filename.c_str(), std::ios::in);
    CPPUNIT_ASSERT(testfile.is_open());

    std::string input;
    while (!testfile.eof()) {
        std::string tmp;
        std::getline(testfile, tmp);
        input.append(tmp);
    }

    testfile.close();
    printf("%s\n", input.c_str());
    CPPUNIT_ASSERT(input == "[Manu] Bonjour, c'est un test d'archivage de message[Alex] Cool");
}

static inline char* duplicateString(char dst[], const char src[], size_t len)
{
    memcpy(dst, src, len);
    dst[len] = 0;
    return dst;
}

static void XMLCALL startElementCallback(void *userData, const char *name, const char **atts)
{

    std::cout << "startElement " << name << std::endl;

    int *nbEntry = (int *) userData;

    char attribute[50];
    char value[50];

    for (const char **att = atts; *att; att += 2) {

        const char **val = att+1;

        duplicateString(attribute, *att, strlen(*att));
        std::cout << "att: " << attribute << std::endl;

        duplicateString(value, *val, strlen(*val));
        std::cout << "val: " << value << std::endl;

        if (strcmp(attribute, "uri") == 0) {
            if ((strcmp(value, "sip:alex@example.com") == 0) ||
                    (strcmp(value, "sip:manu@example.com") == 0))
                CPPUNIT_ASSERT(true);
            else
                CPPUNIT_ASSERT(false);
        }
    }

    *nbEntry += 1;
}

static void XMLCALL
endElementCallback(void * /*userData*/, const char * /*name*/)
{}

void InstantMessagingTest::testGenerateXmlUriList()
{
    std::cout << std::endl;

    // Create a test list with two entries
    sfl::InstantMessaging::UriList list;

    sfl::InstantMessaging::UriEntry entry1;
    entry1[sfl::IM_XML_URI] = "\"sip:alex@example.com\"";

    sfl::InstantMessaging::UriEntry entry2;
    entry2[sfl::IM_XML_URI] = "\"sip:manu@example.com\"";

    list.push_front(entry1);
    list.push_front(entry2);

    std::string buffer = generateXmlUriList(list);
    CPPUNIT_ASSERT(buffer.size() != 0);

    std::cout << buffer << std::endl;

    // parse the resuling xml (further tests are performed in callbacks)
    XML_Parser parser = XML_ParserCreate(NULL);
    int nbEntry = 0;
    XML_SetUserData(parser, &nbEntry);
    XML_SetElementHandler(parser, startElementCallback, endElementCallback);

    if (XML_Parse(parser, buffer.c_str(), buffer.size(), 1) == XML_STATUS_ERROR) {
        ERROR("%s at line %d", XML_ErrorString(XML_GetErrorCode(parser)), XML_GetCurrentLineNumber(parser));
        CPPUNIT_ASSERT(false);
    }

    XML_ParserFree(parser);
    CPPUNIT_ASSERT(nbEntry == 4);
}

void InstantMessagingTest::testXmlUriListParsing()
{
    std::string xmlbuffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xmlbuffer.append("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    xmlbuffer.append("<list>");
    xmlbuffer.append("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    xmlbuffer.append("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    xmlbuffer.append("</list>");
    xmlbuffer.append("</resource-lists>");


    sfl::InstantMessaging::UriList list = parseXmlUriList(xmlbuffer);
    CPPUNIT_ASSERT(list.size() == 2);

    // An iterator over xml attribute
    sfl::InstantMessaging::UriEntry::iterator iterAttr;

    // An iterator over list entries
    for (auto &entry : list) {
        iterAttr = entry.find(sfl::IM_XML_URI);

        CPPUNIT_ASSERT((iterAttr->second == std::string("sip:alex@example.com")) or
                (iterAttr->second == std::string("sip:manu@example.com")));
    }
}

void InstantMessagingTest::testGetTextArea()
{

    std::string formatedText = "--boundary Content-Type: text/plain";
    formatedText.append("Here is the text area");

    formatedText.append("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append("Content-Disposition: recipient-list");
    formatedText.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append("<list>");
    formatedText.append("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("</list>");
    formatedText.append("</resource-lists>");
    formatedText.append("--boundary--");

    std::string message(findTextMessage(formatedText));
    DEBUG("Message %s", message.c_str());

    CPPUNIT_ASSERT(message == "Here is the text area");
}

void InstantMessagingTest::testGetUriListArea()
{
    std::string formatedText = "--boundary Content-Type: text/plain";
    formatedText.append("Here is the text area");

    formatedText.append("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append("Content-Disposition: recipient-list");
    formatedText.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append("<list>");
    formatedText.append("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("</list>");
    formatedText.append("</resource-lists>");
    formatedText.append("--boundary--");

    std::string urilist = findTextUriList(formatedText);

    CPPUNIT_ASSERT(urilist.compare("<?xml version=\"1.0\" encoding=\"UTF-8\"?><resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\"><list><entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" /><entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" /></list></resource-lists>") == 0);

    std::cout << "urilist: " << urilist << std::endl;

    sfl::InstantMessaging::UriList list = parseXmlUriList(urilist);
    CPPUNIT_ASSERT(list.size() == 2);

    // order may be important, for example to identify message sender
    sfl::InstantMessaging::UriEntry entry = list.front();
    CPPUNIT_ASSERT(entry.size() == 2);

    sfl::InstantMessaging::UriEntry::iterator iterAttr = entry.find(sfl::IM_XML_URI);

    if (iterAttr == entry.end()) {
        ERROR("Did not find attribute");
        CPPUNIT_ASSERT(false);
    }

    std::string from = iterAttr->second;
    CPPUNIT_ASSERT(from == "sip:alex@example.com");
}

void InstantMessagingTest::testIllFormatedMessage()
{
    bool exceptionCaught = false;

    // SHOULD BE: Content-Type: text/plain
    std::string formatedText = "--boundary Content-Ty";
    formatedText.append("Here is the text area");

    formatedText.append("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append("Content-Disposition: recipient-list");
    formatedText.append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append("<list>");
    formatedText.append("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append("</list>");
    formatedText.append("</resource-lists>");
    formatedText.append("--boundary--");

    try {
        std::string message = findTextMessage(formatedText);
    } catch (const sfl::InstantMessageException &e) {
        exceptionCaught = true;
    }

    CPPUNIT_ASSERT(exceptionCaught);
}
