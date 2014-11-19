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
#ifndef HOOKMANAGER_H
#define HOOKMANAGER_H

#include "typedefs.h"

/**
 * This class allow to get and set the different hooks
 */
class LIB_EXPORT HookManager : public QObject
{
   Q_OBJECT

public:
   static HookManager* instance();

   //Properties
   Q_PROPERTY(QString prefix              READ prefix               WRITE setPrefix             )
   Q_PROPERTY(QString sipFeild            READ sipFeild             WRITE setSipFeild           )
   Q_PROPERTY(QString command             READ command              WRITE setCommand            )
   Q_PROPERTY(bool    iax2Enabled         READ isIax2Enabled        WRITE setIax2Enabled        )
   Q_PROPERTY(bool    sipEnabled          READ isSipEnabled         WRITE setSipEnabled         )
   Q_PROPERTY(bool    phoneNumberEnabled  READ isPhoneNumberEnabled WRITE setPhoneNumberEnabled )

   //Getters
   QString prefix           () const;
   QString sipFeild         () const;
   QString command          () const;
   bool isIax2Enabled       () const;
   bool isSipEnabled        () const;
   bool isPhoneNumberEnabled() const;

   //Setters
   void setPrefix             (const QString& prefix );
   void setSipFeild           (const QString& field  );
   void setCommand            (const QString& command);
   void setIax2Enabled        (bool enabled          );
   void setSipEnabled         (bool enabled          );
   void setPhoneNumberEnabled (bool enabled          );

private:
   explicit HookManager();
   virtual ~HookManager();
   void save();

   class Names {
   public:
      constexpr static const char* PHONE_NUMBER_HOOK_ADD_PREFIX = "PHONE_NUMBER_HOOK_ADD_PREFIX";
      constexpr static const char* URLHOOK_SIP_FIELD            = "URLHOOK_SIP_FIELD"           ;
      constexpr static const char* URLHOOK_COMMAND              = "URLHOOK_COMMAND"             ;
      constexpr static const char* URLHOOK_IAX2_ENABLED         = "URLHOOK_IAX2_ENABLED"        ;
      constexpr static const char* URLHOOK_SIP_ENABLED          = "URLHOOK_SIP_ENABLED"         ;
      constexpr static const char* PHONE_NUMBER_HOOK_ENABLED    = "PHONE_NUMBER_HOOK_ENABLED"   ;
   };

   //Attributes
   QString m_AddPrefix      ;
   QString m_SipFeild       ;
   QString m_Command        ;
   bool m_Iax2Enabled       ;
   bool m_SipEnabled        ;
   bool m_PhoneNumberEnabled;

   static HookManager* m_spInstance;
};

#endif