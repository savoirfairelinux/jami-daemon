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
#include "uri.h"

constexpr const char* URI::schemeNames[];

URI::URI(const QString& other):QString()
   ,m_Parsed(false),m_HeaderType(SchemeType::NONE)
{
   m_Stripped                     = strip(other,m_HeaderType);
   (*static_cast<QString*>(this)) = m_Stripped               ;
}

URI::URI(const URI& o):QString(),m_Parsed(o.m_Parsed),m_Hostname(o.m_Hostname),
   m_HeaderType(o.m_HeaderType),m_Userinfo(o.m_Userinfo),m_Stripped(o.m_Stripped)
{
   (*static_cast<QString*>(this)) = o.m_Stripped;
}

///Strip out <sip:****> from the URI
QString URI::strip(const QString& uri, SchemeType& sheme)
{
   if (uri.isEmpty())
      return QString();
   int start(0),end(uri.size()-1); //Other type of comparisons were too slow
   if (end > 5 && uri[0] == '<' ) {
      if (uri[4] == ':') {
         sheme = uri[1] == 's'?SchemeType::SIP:SchemeType::IAX;
         start = 5;
      }
      else if (uri[5] == ':') {
         sheme = SchemeType::SIPS;
         start = 6;
      }
   }
   if (end && uri[end] == '>')
      end--;
   else if (start) {
      //TODO there may be a ';' section with arguments, check
   }
   return uri.mid(start,end-start+1);
}

///Return the domaine of an URI (<sip:12345@example.com>)
QString URI::hostname() const
{
   if (!m_Parsed)
      const_cast<URI*>(this)->parse();
   return m_Hostname;
}

bool URI::hasHostname() const
{
   if (!m_Parsed)
      const_cast<URI*>(this)->parse();
   return !m_Hostname.isEmpty();
}

///Keep a cache of the values to avoid re-parsing them
void URI::parse()
{
   if (indexOf('@') != -1) {
      const QStringList splitted = split('@');
      m_Hostname = splitted[1];//splitted[1].left(splitted[1].size())
      m_Userinfo = splitted[0];
      m_Parsed = true;
   }
}

QString URI::userinfo() const
{
   if (!m_Parsed)
      const_cast<URI*>(this)->parse();
   return m_Userinfo;
}

/**
 * Some feature, like SIP presence, require a properly formatted USI
 */
QString URI::fullUri() const
{
   return QString("<%1%2>")
      .arg(schemeNames[static_cast<int>(m_HeaderType == SchemeType::NONE?SchemeType::SIP:m_HeaderType)])
      .arg(*this);
}