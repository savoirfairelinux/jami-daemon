/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/

//Parent
#include "Account.h"

//Qt
#include <QtCore/QDebug>
#include <QtCore/QString>

//SFLPhone
#include "sflphone_const.h"

//SFLPhone lib
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include "video_interface_singleton.h"
#include "AccountList.h"
#include "CredentialModel.h"
#include "AudioCodecModel.h"
#include "VideoCodecModel.h"

const account_function Account::stateMachineActionsOnState[6][7] = {
/*                 NOTHING              EDIT              RELOAD              SAVE               REMOVE             MODIFY             CANCEL            */
/*READY    */{ &Account::nothing, &Account::edit   , &Account::reload , &Account::nothing, &Account::remove , &Account::modify   , &Account::nothing },/**/
/*EDITING  */{ &Account::nothing, &Account::nothing, &Account::outdate, &Account::nothing, &Account::remove , &Account::modify   , &Account::cancel  },/**/
/*OUTDATED */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::remove , &Account::reloadMod, &Account::reload  },/**/
/*NEW      */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::save   , &Account::remove , &Account::nothing  , &Account::nothing },/**/
/*MODIFIED */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::save   , &Account::remove , &Account::nothing  , &Account::reload  },/**/
/*REMOVED  */{ &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing, &Account::nothing  , &Account::cancel  } /**/
/*                                                                                                                                                       */
};

///Match state name to user readable string
const QString& account_state_name(const QString& s)
{
   static const QString registered             = "Registered"               ;
   static const QString notRegistered          = "Not Registered"           ;
   static const QString trying                 = "Trying..."                ;
   static const QString error                  = "Error"                    ;
   static const QString authenticationFailed   = "Authentication Failed"    ;
   static const QString networkUnreachable     = "Network unreachable"      ;
   static const QString hostUnreachable        = "Host unreachable"         ;
   static const QString stunConfigurationError = "Stun configuration error" ;
   static const QString stunServerInvalid      = "Stun server invalid"      ;
   static const QString invalid                = "Invalid"                  ;
   
   if(s == QString(ACCOUNT_STATE_REGISTERED)       )
      return registered             ;
   if(s == QString(ACCOUNT_STATE_UNREGISTERED)     )
      return notRegistered          ;
   if(s == QString(ACCOUNT_STATE_TRYING)           )
      return trying                 ;
   if(s == QString(ACCOUNT_STATE_ERROR)            )
      return error                  ;
   if(s == QString(ACCOUNT_STATE_ERROR_AUTH)       )
      return authenticationFailed   ;
   if(s == QString(ACCOUNT_STATE_ERROR_NETWORK)    )
      return networkUnreachable     ;
   if(s == QString(ACCOUNT_STATE_ERROR_HOST)       )
      return hostUnreachable        ;
   if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN)  )
      return stunConfigurationError ;
   if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN) )
      return stunServerInvalid      ;
   return invalid                   ;
} //account_state_name

///Constructors
Account::Account():m_pAccountId(NULL),m_pAccountDetails(NULL),m_pCredentials(nullptr),m_pAudioCodecs(nullptr),m_CurrentState(READY),
m_pVideoCodecs(nullptr)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   connect(&callManager,SIGNAL(registrationStateChanged(QString,QString,int)),this,SLOT(accountChanged(QString,QString,int)));
}

///Build an account from it'id
Account* Account::buildExistingAccountFromId(const QString& _accountId)
{
   qDebug() << "Building an account from id: " << _accountId;
   Account* a = new Account();
   a->m_pAccountId = new QString(_accountId);

   a->performAction(AccountEditAction::RELOAD);

   return a;
} //buildExistingAccountFromId

///Build an account from it's name / alias
Account* Account::buildNewAccountFromAlias(const QString& alias)
{
   qDebug() << "Building an account from alias: " << alias;
   Account* a = new Account();
   a->m_pAccountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   return a;
}

///Destructor
Account::~Account()
{
   disconnect();
   delete m_pAccountId;
   if (m_pCredentials)    delete m_pCredentials;
   if (m_pAccountDetails) delete m_pAccountDetails;
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///Callback when the account state change
void Account::accountChanged(QString accountId,QString state,int)
{
   if (m_pAccountId && accountId == *m_pAccountId) {
      if (Account::updateState())
         emit stateChanged(getStateName(state));
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///IS this account new
bool Account::isNew() const
{
   return (m_pAccountId == NULL);
}

///Get this account ID
const QString& Account::getAccountId() const
{
   if (isNew())
      qDebug() << "Error : getting AccountId of a new account.";
   if (!m_pAccountId) {
      qDebug() << "Account not configured";
      return EMPTY_STRING; //WARNING May explode
   }
   
   return *m_pAccountId;
}

///Get this account details
const MapStringString& Account::getAccountDetails() const
{
   return *m_pAccountDetails;
}

///Get current state
const QString& Account::getStateName(const QString& state) const
{
   return (const QString&)account_state_name(state);
}

///Get an account detail
const QString& Account::getAccountDetail(const QString& param) const
{
   if (!m_pAccountDetails) {
      qDebug() << "The account list is not set";
      return EMPTY_STRING; //May crash, but better than crashing now
   }
   if (m_pAccountDetails->find(param) != m_pAccountDetails->end()) {
      return (*m_pAccountDetails)[param];
   }
   else if (m_pAccountDetails->count() > 0) {
      qDebug() << "Account paramater \"" << param << "\" not found";
      return EMPTY_STRING;
   }
   else {
      qDebug() << "Account details not found, there is " << m_pAccountDetails->count() << " details available";
      return EMPTY_STRING;
   }
} //getAccountDetail

///Get the alias
const QString& Account::getAlias() const
{
   return getAccountDetail(ACCOUNT_ALIAS);
}

///Is this account enabled
bool Account::isEnabled() const
{
   return (getAccountDetail(ACCOUNT_ENABLED) == REGISTRATION_ENABLED_TRUE);
}

///Is this account registered
bool Account::isRegistered() const
{
   return (getAccountDetail(ACCOUNT_REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED);
}

///Return the model index of this item
QModelIndex Account::getIndex()
{
   for (int i=0;i < AccountList::getInstance()->m_pAccounts->size();i++) {
      if (this == (*AccountList::getInstance()->m_pAccounts)[i]) {
         return AccountList::getInstance()->index(i,0);
      }
   }
   return QModelIndex();
}

///Return status color name
QString Account::getStateColorName() const
{
   if(getAccountRegistrationStatus() == ACCOUNT_STATE_UNREGISTERED)
            return "black";
   if(getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED || getAccountRegistrationStatus() == ACCOUNT_STATE_READY)
            return "darkGreen";
   return "red";
}

///Return status Qt color, QColor is not part of QtCore, use using the global variant
Qt::GlobalColor Account::getStateColor() const
{
   if(getAccountRegistrationStatus() == ACCOUNT_STATE_UNREGISTERED)
            return Qt::darkGray;
   if(getAccountRegistrationStatus() == ACCOUNT_STATE_REGISTERED || getAccountRegistrationStatus() == ACCOUNT_STATE_READY)
            return Qt::darkGreen;
   if(getAccountRegistrationStatus() == ACCOUNT_STATE_TRYING)
            return Qt::darkYellow;
   return Qt::darkRed;
}

///Create and return the credential model
CredentialModel* Account::getCredentialsModel()
{
   if (!m_pCredentials) {
      reloadCredentials();
   }
   return m_pCredentials;
}

///Create and return the audio codec model
AudioCodecModel* Account::getAudioCodecModel()
{
   if (!m_pAudioCodecs) {
      reloadAudioCodecs();
   }
   return m_pAudioCodecs;
}

///Create and return the video codec model
VideoCodecModel* Account::getVideoCodecModel()
{
   if (!m_pVideoCodecs) {
      m_pVideoCodecs = new VideoCodecModel(this);
   }
   return m_pVideoCodecs;
}

/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set account details
void Account::setAccountDetails(const MapStringString& m)
{
   if (m_pAccountDetails)
      delete m_pAccountDetails;
   *m_pAccountDetails = m;
}

///Set a specific detail
bool Account::setAccountDetail(const QString& param, const QString& val)
{
   QString buf = (*m_pAccountDetails)[param];
   if (param == ACCOUNT_REGISTRATION_STATUS) {
      (*m_pAccountDetails)[param] = val;
      emit detailChanged(this,param,val,buf);
   }
   else {
      performAction(AccountEditAction::MODIFY);
      if (m_CurrentState == MODIFIED || m_CurrentState == NEW) {
         (*m_pAccountDetails)[param] = val;
         emit detailChanged(this,param,val,buf);
      }
   }
   return m_CurrentState == MODIFIED || m_CurrentState == NEW;
}

///Set the account id
void Account::setAccountId(const QString& id)
{
   qDebug() << "Setting accountId = " << m_pAccountId;
   if (! isNew())
      qDebug() << "Error : setting AccountId of an existing account.";
   m_pAccountId = new QString(id);
}

///Set account enabled
void Account::setEnabled(bool checked)
{
   setAccountEnabled(checked);
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

/**Update the account
 * @return if the state changed
 */
bool Account::updateState()
{
   if(! isNew()) {
      ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      MapStringString details = configurationManager.getAccountDetails(getAccountId()).value();
      QString status = details[ACCOUNT_REGISTRATION_STATUS];
      QString currentStatus = getAccountRegistrationStatus();
      setAccountDetail(ACCOUNT_REGISTRATION_STATUS, status); //Update -internal- object state
      return status == currentStatus;
   }
   return true;
}

///Save the current account to the daemon
void Account::save()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   if (isNew()) {
      MapStringString details = getAccountDetails();
      QString currentId = configurationManager.addAccount(details);
      setAccountId(currentId);
      qDebug() << "NEW ID" << currentId;
   }
   else {
      configurationManager.setAccountDetails(getAccountId(), getAccountDetails());
   }

   //QString id = configurationManager.getAccountDetail(getAccountId());
   if (!getAccountId().isEmpty()) {
      Account* acc =  AccountList::getInstance()->getAccountById(getAccountId());
      qDebug() << "Adding the new account to the account list (" << getAccountId() << ")";
      if (acc != this) {
         (*AccountList::getInstance()->m_pAccounts) << this;
      }
      
      performAction(AccountEditAction::RELOAD);
      updateState();
      m_CurrentState = READY;
   }
   m_pVideoCodecs->save();
   saveAudioCodecs();
}

///Synchronise with the daemon, this need to be done manually to prevent reloading the account while it is being edited
void Account::reload()
{
   qDebug() << "Reloading" << getAccountId();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QMap<QString,QString> aDetails = configurationManager.getAccountDetails(getAccountId());

   if (!aDetails.count()) {
      qDebug() << "Account not found";
   }
   else {
      if (m_pAccountDetails) {
         delete m_pAccountDetails;
         m_pAccountDetails = nullptr;
      }
      m_pAccountDetails = new MapStringString(aDetails);
   }
   m_CurrentState = READY;
}

void Account::reloadCredentials()
{
   if (!m_pCredentials) {
      m_pCredentials = new CredentialModel(this);
   }
   m_pCredentials->clear();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   VectorMapStringString credentials = configurationManager.getCredentials(getAccountId());
   for (int i=0; i < credentials.size(); i++) {
      QModelIndex idx = m_pCredentials->addCredentials();
      m_pCredentials->setData(idx,credentials[i][ CONFIG_ACCOUNT_USERNAME  ],CredentialModel::NAME_ROLE    );
      m_pCredentials->setData(idx,credentials[i][ CONFIG_ACCOUNT_PASSWORD  ],CredentialModel::PASSWORD_ROLE);
      m_pCredentials->setData(idx,credentials[i][ CONFIG_ACCOUNT_REALM     ],CredentialModel::REALM_ROLE   );
   }
}

void Account::saveCredentials() {
   if (m_pCredentials) {
      ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      VectorMapStringString toReturn;
      for (int i=0; i < m_pCredentials->rowCount();i++) {
         QModelIndex idx = m_pCredentials->index(i,0);
         MapStringString credentialData;
         QString username = m_pCredentials->data(idx,CredentialModel::NAME_ROLE     ).toString();
         username = (username.isEmpty())?getAccountUsername():username;
         credentialData[ CONFIG_ACCOUNT_USERNAME] = m_pCredentials->data(idx,CredentialModel::NAME_ROLE     ).toString();
         credentialData[ CONFIG_ACCOUNT_PASSWORD] = m_pCredentials->data(idx,CredentialModel::PASSWORD_ROLE ).toString();
         credentialData[ CONFIG_ACCOUNT_REALM   ] = m_pCredentials->data(idx,CredentialModel::REALM_ROLE    ).toString();
         toReturn << credentialData;
      }
      configurationManager.setCredentials(getAccountId(),toReturn);
   }
}

void Account::reloadAudioCodecs()
{
   if (!m_pAudioCodecs) {
      m_pAudioCodecs = new AudioCodecModel(this);
   }
   m_pAudioCodecs->clear();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QVector<int> codecIdList = configurationManager.getAudioCodecList();
   QVector<int> activeCodecList = configurationManager.getActiveAudioCodecList(getAccountId());
   QStringList tmpNameList;

   foreach (int aCodec, activeCodecList) {
      QStringList codec = configurationManager.getAudioCodecDetails(aCodec);
      QModelIndex idx = m_pAudioCodecs->addAudioCodec();
      m_pAudioCodecs->setData(idx,codec[0]     ,AudioCodecModel::NAME_ROLE       );
      m_pAudioCodecs->setData(idx,codec[1]     ,AudioCodecModel::SAMPLERATE_ROLE );
      m_pAudioCodecs->setData(idx,codec[2]     ,AudioCodecModel::BITRATE_ROLE    );
      m_pAudioCodecs->setData(idx,aCodec       ,AudioCodecModel::ID_ROLE         );
      m_pAudioCodecs->setData(idx, Qt::Checked ,Qt::CheckStateRole               );
      if (codecIdList.indexOf(aCodec)!=-1)
         codecIdList.remove(codecIdList.indexOf(aCodec));
   }

   foreach (int aCodec, codecIdList) {
      QStringList codec = configurationManager.getAudioCodecDetails(aCodec);
      QModelIndex idx = m_pAudioCodecs->addAudioCodec();
      m_pAudioCodecs->setData(idx,codec[0],AudioCodecModel::NAME_ROLE       );
      m_pAudioCodecs->setData(idx,codec[1],AudioCodecModel::SAMPLERATE_ROLE );
      m_pAudioCodecs->setData(idx,codec[2],AudioCodecModel::BITRATE_ROLE    );
      m_pAudioCodecs->setData(idx,aCodec  ,AudioCodecModel::ID_ROLE         );
      
      m_pAudioCodecs->setData(idx, Qt::Unchecked ,Qt::CheckStateRole);
   }
}

void Account::saveAudioCodecs() {
   QStringList _codecList;
   for (int i=0; i < m_pAudioCodecs->rowCount();i++) {
      QModelIndex idx = m_pAudioCodecs->index(i,0);
      if (m_pAudioCodecs->data(idx,Qt::CheckStateRole) == Qt::Checked) {
         _codecList << m_pAudioCodecs->data(idx,AudioCodecModel::ID_ROLE).toString();
      }
   }

   ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   configurationManager.setActiveAudioCodecList(_codecList, getAccountId());
   qDebug() << "Account codec have been saved" << _codecList << getAccountId();
}

/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Are both account the same
bool Account::operator==(const Account& a)const
{
   return *m_pAccountId == *a.m_pAccountId;
}

/*****************************************************************************
 *                                                                           *
 *                                   Video                                   *
 *                                                                           *
 ****************************************************************************/
#ifdef ENABLE_VIDEO
void Account::setActiveVideoCodecList(QList<VideoCodec*> codecs)
{
   QStringList codecs;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   foreach(VideoCodec* codec,codecs) {
      codecs << codecs->getName();
   }
   interface.setActiveCodecList(codecs,m_pAccountId);
}

QList<VideoCodec*> Account::getActiveVideoCodecList()
{
   QList<VideoCodec*> codecs;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   foreach (QString codec, interface.getActiveCodecList(m_pAccountId)) {
      codecs << VideoCodec::getCodec(codec);
   }
}

#endif