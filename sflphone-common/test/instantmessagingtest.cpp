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
#include <iostream>
#include <fstream>

#include "instantmessagingtest.h"

#include "expat.h"
#include <stdio.h>

#define MAXIMUM_SIZE	10
#define DELIMITER_CHAR	"\n\n"

using std::cout;
using std::endl;


void InstantMessagingTest::setUp()
{
    _im = new sfl::InstantMessaging ();
    _im->init ();
}

void InstantMessagingTest::testSaveSingleMessage ()
{
    _debug ("-------------------- InstantMessagingTest::testSaveSingleMessage --------------------\n");

    std::string input, tmp;
    std::string callID = "testfile1.txt";
    std::string filename = "im:";

    // Open a file stream and try to write in it
    CPPUNIT_ASSERT (_im->saveMessage ("Bonjour, c'est un test d'archivage de message", "Manu", callID, std::ios::out)  == true);

    filename.append(callID);
    // Read it to check it has been successfully written
    std::ifstream testfile (filename.c_str (), std::ios::in);
    CPPUNIT_ASSERT (testfile.is_open () == true);

    while (!testfile.eof ()) {
        std::getline (testfile, tmp);
        input.append (tmp);
    }

    testfile.close ();
    CPPUNIT_ASSERT (input == "[Manu] Bonjour, c'est un test d'archivage de message");
}

void InstantMessagingTest::testSaveMultipleMessage ()
{
    _debug ("-------------------- InstantMessagingTest::testSaveMultipleMessage --------------------\n");

    std::string input, tmp;
    std::string callID = "testfile2.txt";
    std::string filename = "im:";

    // Open a file stream and try to write in it
    CPPUNIT_ASSERT (_im->saveMessage ("Bonjour, c'est un test d'archivage de message", "Manu", callID, std::ios::out)  == true);
    CPPUNIT_ASSERT (_im->saveMessage ("Cool", "Alex", callID, std::ios::out || std::ios::app)  == true);

    filename.append(callID);
    // Read it to check it has been successfully written
    std::ifstream testfile (filename.c_str (), std::ios::in);
    CPPUNIT_ASSERT (testfile.is_open () == true);

    while (!testfile.eof ()) {
        std::getline (testfile, tmp);
        input.append (tmp);
    }

    testfile.close ();
    printf ("%s\n", input.c_str());
    CPPUNIT_ASSERT (input == "[Manu] Bonjour, c'est un test d'archivage de message[Alex] Cool");
}

void InstantMessagingTest::testSplitMessage ()
{

    _im->setMessageMaximumSize(10);
    unsigned int maxSize = _im->getMessageMaximumSize();

    /* A message that does not need to be split */
    std::string short_message = "Salut";
    std::vector<std::string> messages = _im->split_message (short_message);
    CPPUNIT_ASSERT (messages.size() == short_message.length() / maxSize + 1);
    CPPUNIT_ASSERT (messages[0] == short_message);

    /* A message that needs to be split into two messages */
    std::string long_message = "A message too long";
    messages = _im->split_message (long_message);
    int size = messages.size ();
    int i = 0;
    CPPUNIT_ASSERT (size == (int) (long_message.length() / maxSize + 1));

    /* If only one element, do not enter the loop */
    for (i = 0; i < size - 1; i++) {
        CPPUNIT_ASSERT (messages[i] == long_message.substr ( (maxSize * i), maxSize) + DELIMITER_CHAR);
    } 

    /* Works for the last element, or for the only element */
    CPPUNIT_ASSERT (messages[size- 1] == long_message.substr (maxSize * (size-1)));

    /* A message that needs to be split into four messages */
    std::string very_long_message = "A message that needs to be split into many messages";
    messages = _im->split_message (very_long_message);
    size = messages.size ();

    /* If only one element, do not enter the loop */
    for (i = 0; i < size - 1; i++) {
        CPPUNIT_ASSERT (messages[i] ==very_long_message.substr ( (maxSize * i), maxSize) + DELIMITER_CHAR);
    }

    /* Works for the last element, or for the only element */
    CPPUNIT_ASSERT (messages[size- 1] == very_long_message.substr (maxSize * (size-1)));
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

    int *nbEntry = (int *)userData;

    char attribute[50];
    char value[50];

    const char **att;
    for (att = atts; *att; att += 2) {

	const char **val = att+1;

	duplicateString(attribute, *att, strlen(*att));
	std::cout << "att: " << attribute << std::endl;
	
	duplicateString(value, *val, strlen(*val));
	std::cout << "val: " << value << std::endl;

	if (strcmp(attribute, "uri") == 0) {
	    if((strcmp(value, "sip:alex@example.com") == 0) ||
	       (strcmp(value, "sip:manu@example.com") == 0))
		CPPUNIT_ASSERT(true);
	    else
		CPPUNIT_ASSERT(false);
	}
    }

    *nbEntry += 1;

}

static void XMLCALL endElementCallback(void *userData, const char *name)
{
    // std::cout << "endElement " << name << std::endl;    
}

void InstantMessagingTest::testGenerateXmlUriList ()
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

    std::string buffer = _im->generateXmlUriList(list);
    CPPUNIT_ASSERT(buffer.size() != 0);

    std::cout << buffer << std::endl;
	
    // parse the resuling xml (further tests are performed in callbacks)
    XML_Parser parser = XML_ParserCreate(NULL);
    int nbEntry = 0;
    XML_SetUserData(parser, &nbEntry);
    XML_SetElementHandler(parser, startElementCallback, endElementCallback);
    if (XML_Parse(parser, buffer.c_str(), buffer.size(), 1) == XML_STATUS_ERROR) {
	std::cout << "Error: " << XML_ErrorString(XML_GetErrorCode(parser)) 
                  << " at line " << XML_GetCurrentLineNumber(parser) << std::endl;
        CPPUNIT_ASSERT(false);
    }
    XML_ParserFree(parser);

    CPPUNIT_ASSERT(nbEntry == 4);

    CPPUNIT_ASSERT(true);
}

void InstantMessagingTest::testXmlUriListParsing ()
{
    std::string xmlbuffer = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    xmlbuffer.append ("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    xmlbuffer.append ("<list>");
    xmlbuffer.append ("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    xmlbuffer.append ("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    xmlbuffer.append ("</list>");
    xmlbuffer.append ("</resource-lists>");


    sfl::InstantMessaging::UriList list = _im->parseXmlUriList(xmlbuffer);
    CPPUNIT_ASSERT(list.size() == 2);

    // An iterator over xml attribute
    sfl::InstantMessaging::UriEntry::iterator iterAttr;

    // An iterator over list entries
    sfl::InstantMessaging::UriList::iterator iterEntry = list.begin();

    
    while (iterEntry != list.end()) {
        sfl::InstantMessaging::UriEntry entry = static_cast<sfl::InstantMessaging::UriEntry> (*iterEntry);
        iterAttr = entry.find (sfl::IM_XML_URI);
		
        if((iterAttr->second == std::string("sip:alex@example.com")) ||
           (iterAttr->second == std::string("sip:manu@example.com")))
	    CPPUNIT_ASSERT(true);
	else
	    CPPUNIT_ASSERT(false);
        iterEntry++;
    }
}

void InstantMessagingTest::testGetTextArea ()
{

    std::string formatedText = "--boundary Content-Type: text/plain";
    formatedText.append ("Here is the text area");

    formatedText.append ("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append ("Content-Disposition: recipient-list");
    formatedText.append ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append ("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append ("<list>");
    formatedText.append ("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("</list>");
    formatedText.append ("</resource-lists>");
    formatedText.append ("--boundary--");

    std::string message = _im->findTextMessage(formatedText);

    std::cout << "message " << message << std::endl;

    CPPUNIT_ASSERT(message == "Here is the text area");
}


void InstantMessagingTest::testGetUriListArea ()
{
    std::string formatedText = "--boundary Content-Type: text/plain";
    formatedText.append ("Here is the text area");

    formatedText.append ("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append ("Content-Disposition: recipient-list");
    formatedText.append ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append ("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append ("<list>");
    formatedText.append ("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("</list>");
    formatedText.append ("</resource-lists>");
    formatedText.append ("--boundary--");

    std::string urilist = _im->findTextUriList(formatedText);

    CPPUNIT_ASSERT(urilist.compare("<?xml version=\"1.0\" encoding=\"UTF-8\"?><resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\"><list><entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" /><entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" /></list></resource-lists>") == 0);

    std::cout << "urilist: " << urilist << std::endl;

    sfl::InstantMessaging::UriList list = _im->parseXmlUriList(urilist);
    CPPUNIT_ASSERT(list.size() == 2);

    // order may be important, for example to identify message sender
    sfl::InstantMessaging::UriEntry entry = list.front();
    CPPUNIT_ASSERT(entry.size() == 2);

    sfl::InstantMessaging::UriEntry::iterator iterAttr = entry.find (sfl::IM_XML_URI);

    if(iterAttr == entry.end()) {
	std::cout << "Error, did not found attribute" << std::endl;
	CPPUNIT_ASSERT(false);
    }

    std::string from = iterAttr->second;
    CPPUNIT_ASSERT(from == "sip:alex@example.com");
}


void InstantMessagingTest::testIllFormatedMessage ()
{
    bool exceptionCaught = false;

    // SHOULD BE: Content-Type: text/plain
    std::string formatedText = "--boundary Content-Ty";
    formatedText.append ("Here is the text area");

    formatedText.append ("--boundary Content-Type: application/resource-lists+xml");
    formatedText.append ("Content-Disposition: recipient-list");
    formatedText.append ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    formatedText.append ("<resource-lists xmlns=\"urn:ietf:params:xml:ns:resource-lists\" xmlns:cp=\"urn:ietf:params:xml:ns:copycontrol\">");
    formatedText.append ("<list>");
    formatedText.append ("<entry uri=\"sip:alex@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("<entry uri=\"sip:manu@example.com\" cp:copyControl=\"to\" />");
    formatedText.append ("</list>");
    formatedText.append ("</resource-lists>");
    formatedText.append ("--boundary--");

    try {
	std::string message = _im->findTextMessage(formatedText);
    } catch (sfl::InstantMessageException &e) {
	exceptionCaught = true;	
    }

    if(exceptionCaught)
	CPPUNIT_ASSERT(true);
    else
	CPPUNIT_ASSERT(false);

}


void InstantMessagingTest::tearDown()
{
    delete _im;
    _im = 0;
}
