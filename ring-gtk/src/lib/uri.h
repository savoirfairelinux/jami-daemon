/****************************************************************************
 *   Copyright (C) 2014 by Savoir-Faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#ifndef URI_H
#define URI_H

#include "typedefs.h"

#include <QStringList>

/**
    * @class URI A specialised string with multiple attributes
    *
    * Most of SFLPhone-KDE handle uri as strings, but more
    * advanced algorithms need to access the various sections.
    *
    * Here is some example of common numbers/URIs:
    *  * 123
    *  * 123@192.168.123.123
    *  * 123@asterisk-server
    *  * <sip:123@192.168.123.123>
    *  * <sips:123@192.168.123.123>
    *  * <sips:888@192.168.48.213;transport=TLS>
    *  * <sip:c8oqz84zk7z@privacy.org>;tag=hyh8
    *  * 1 800 123-4567
    *  * 18001234567
    *  * iax:example.com/alice
    *  * iax:johnQ@example.com/12022561414
    *  * iax:example.com:4570/alice?friends
    *
    * @ref http://tools.ietf.org/html/rfc5456#page-8
    * @ref http://tools.ietf.org/html/rfc3986
    * @ref http://tools.ietf.org/html/rfc3261
    * @ref http://tools.ietf.org/html/rfc5630
    *
    * From the RFC:
    *    foo://example.com:8042/over/there?name=ferret#nose
    *    \_/   \______________/\_________/ \_________/ \__/
    *     |           |            |            |        |
    *  scheme     authority       path        query   fragment
    *     |   _____________________|__
    *    / \ /                        \
    *    urn:example:animal:ferret:nose
    *
    *    authority   = [ userinfo "@" ] host [ ":" port ]
    *
    *    "For example, the semicolon (";") and equals ("=") reserved characters are
    *    often used to delimit parameters and parameter values applicable to
    *    that segment.  The comma (",") reserved character is often used for
    *    similar purposes.  For example, one URI producer might use a segment
    *    such as "name;v=1.1" to indicate a reference to version 1.1 of
    *    "name", whereas another might use a segment such as "name,1.1" to
    *    indicate the same. "
    */
class LIB_EXPORT URI : public QString {
public:

   /**
    * Default constructor
    * @param other an URI string
    */
   URI(const QString& other);
   URI(const URI&     other);

   ///@enum SchemeType The very first part of the URI followed by a ':'
   enum class SchemeType {
      NONE , //Implicit SIP or IAX, use account type as reference
      SIP  ,
      SIPS ,
      IAX  ,
   };

   ///Strings associated with SchemeType
   constexpr static const char* schemeNames[] = {
      /*NONE = */ ""     ,
      /*SIP  = */ "sip:" ,
      /*SIPS = */ "sips:",
      /*IAX  = */ "iax:" ,
   };

   /**
    * @enum Transport each known valid transport types
    * Defined at http://tools.ietf.org/html/rfc3261#page-222
    */
   enum class Transport {
      NOT_SET, /** The transport have not been set directly in the URI  */
      TLS    , /**                                                      */
      tls    , /**                                                      */
      TCP    , /**                                                      */
      tcp    , /**                                                      */
      UDP    , /**                                                      */
      udp    , /**                                                      */
      SCTP   , /**                                                      */
      sctp   , /**                                                      */
   };

   QString hostname   () const;
   QString fullUri    () const;
   QString userinfo   () const;
   bool    hasHostname() const;

private:
   QString     m_Hostname    ;
   QString     m_Userinfo    ;
   QStringList m_lAttributes ;
   QString     m_Stripped    ;
   SchemeType  m_HeaderType  ;
   bool        m_hasChevrons ;
   bool        m_Parsed      ;

   //Helper
   static QString strip(const QString& uri, SchemeType& sheme);
   void parse();
};
// Q_DECLARE_METATYPE(URI*)

#endif //URI_H