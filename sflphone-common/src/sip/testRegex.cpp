#include "Regex.h"
#include <iostream>
#include <algorithm>

std::string regularExpression = "^a=crypto:([0-9]{1,9})" \
                                "[\x20\x09](AES_CM_128_HMAC_SHA1_80|AES_CM_128_HMAC_SHA1_32|F8_128_HMAC_SHA1_80|[A-Za-z0-9_]+)" \
                                "[\x20\x09](inline|[A-Za-z0-9_]+)\\:([A-Za-z0-9\x2B\x2F\x3D]+)\\|2\\^([0-9]+)\\|([0-9]+)\\:([0-9]{1,3})\\;?" \
                                "[\x20\x09]?(kdr\\=[0-9]{1,2}|UNENCRYPTED_SRTP|UNENCRYPTED_SRTCP|UNAUTHENTICATED_SRTP|(FEC_ORDER)=(FEC_SRTP|SRTP_FEC)" \
                                "|(FEC_KEY)=|(WSH)=([0-9]{1,2})|(?<!\\-)[[:graph:]]+)*";

std::string subject = "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32 kdr=12";

void printSubstring (const std::string& substring)
{
    std::cout << substring << std::endl;
}

void testFindMethods (void)
{
    // Test the find methods
    //
    std::cout << "Testing pattern 1" << std::endl;

    sfl::Regex pattern (regularExpression);

    // Test the findall method
    std::vector<std::string> substring = pattern.findall (subject);

std:
    for_each (substring.begin(), substring.end(), printSubstring);

    // Test the finditer method
    sfl::range range = pattern.finditer (subject);
    std::for_each (range.first, range.second, printSubstring);

    // Instanciate a new Regex object
    // but set the pattern only after
    // the constructor was called.
    std::cout  << std::endl << "Testing pattern 2" << std::endl;
}

void testOperators (void)
{
    sfl::Regex pattern2;

    pattern2.setPattern (regularExpression);
    pattern2.compile();

    sfl::range range = pattern2.finditer (subject);
    std::for_each (range.first, range.second, printSubstring);

    // Instanciate a new Regex object
    // but set the pattern only after
    // the constructor was called.
    // Use the = operator to set the
    // regular expression.
    std::cout  << std::endl << "Testing pattern 3" << std::endl;

    sfl::Regex pattern3;

    pattern3 = regularExpression;

    range = pattern3.finditer (subject);
    std::for_each (range.first, range.second, printSubstring);

    // Test the << and >> operators
    std::cout  << std::endl << "Testing pattern 4" << std::endl;
    sfl::Regex pattern4;

    pattern4 = regularExpression;

    pattern4 << subject;

    std::vector<std::string> outputVector;
    pattern4 >> outputVector;
    std::for_each (outputVector.begin(), outputVector.end(), printSubstring);
}

void testGroup (void)
{
    std::cout  << std::endl << "Testing group feature" << std::endl;

    sfl::Regex pattern;

    pattern = "^a=crypto:(?P<tag>[0-9]{1,9})";

    pattern << subject;

    std::string substring = pattern.group ("tag");

    std::cout << "Substring: " << substring << std::endl;
}

int main (void)
{
    testFindMethods();
    testOperators();
    testGroup();

    return 0;
}

