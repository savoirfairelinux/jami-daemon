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
#include "hookmanager.h"

#include <QtCore/QCoreApplication>
#include "dbus/configurationmanager.h"

HookManager* HookManager::m_spInstance = nullptr;

HookManager::HookManager() : QObject(QCoreApplication::instance())
{
   ConfigurationManagerInterface & configurationManager = DBus::ConfigurationManager::instance();
   QMap<QString,QString> hooks = configurationManager.getHookSettings();
   m_AddPrefix          = hooks[HookManager::Names::PHONE_NUMBER_HOOK_ADD_PREFIX];
   m_SipFeild           = hooks[HookManager::Names::URLHOOK_SIP_FIELD           ];
   m_Command            = hooks[HookManager::Names::URLHOOK_COMMAND             ];
   m_Iax2Enabled        = hooks[HookManager::Names::URLHOOK_IAX2_ENABLED        ]=="true"?true:false;
   m_SipEnabled         = hooks[HookManager::Names::URLHOOK_SIP_ENABLED         ]=="true"?true:false;
   m_PhoneNumberEnabled = hooks[HookManager::Names::PHONE_NUMBER_HOOK_ENABLED   ]=="true"?true:false;

}

HookManager::~HookManager()
{
}

void HookManager::save()
{
   ConfigurationManagerInterface & configurationManager = DBus::ConfigurationManager::instance();
   QMap<QString,QString> hooks;

   hooks[HookManager::Names::PHONE_NUMBER_HOOK_ADD_PREFIX] = m_AddPrefix;
   hooks[HookManager::Names::URLHOOK_SIP_FIELD           ] = m_SipFeild;
   hooks[HookManager::Names::URLHOOK_COMMAND             ] = m_Command;
   hooks[HookManager::Names::URLHOOK_IAX2_ENABLED        ] = m_Iax2Enabled?"true":"false";
   hooks[HookManager::Names::URLHOOK_SIP_ENABLED         ] = m_SipEnabled?"true":"false";
   hooks[HookManager::Names::PHONE_NUMBER_HOOK_ENABLED   ] = m_PhoneNumberEnabled?"true":"false";
   configurationManager.setHookSettings(hooks);
}

HookManager* HookManager::instance()
{
   if (!m_spInstance)
      m_spInstance = new HookManager();
   return m_spInstance;
}

QString HookManager::prefix() const
{
   return m_AddPrefix;
}

QString HookManager::sipFeild() const
{
   return m_SipFeild;
}

QString HookManager::command() const
{
   return m_Command;
}

bool HookManager::isIax2Enabled() const
{
   return m_Iax2Enabled;
}

bool HookManager::isSipEnabled() const
{
   return m_SipEnabled;
}

bool HookManager::isPhoneNumberEnabled() const
{
   return m_PhoneNumberEnabled;
}

void HookManager::setPrefix(const QString& prefix)
{
   m_AddPrefix = prefix;
   save();
}

void HookManager::setSipFeild(const QString& field)
{
   m_SipFeild = field;
   save();
}

void HookManager::setCommand(const QString& command)
{
   m_Command = command;
   save();
}

void HookManager::setIax2Enabled(bool enabled)
{
   m_Iax2Enabled = enabled;
   save();
}

void HookManager::setSipEnabled(bool enabled)
{
   m_SipEnabled = enabled;
   save();
}

void HookManager::setPhoneNumberEnabled(bool enabled)
{
   m_PhoneNumberEnabled = enabled;
   save();
}
