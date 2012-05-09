/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

#ifndef CALL_MODEL_H
#define CALL_MODEL_H

#include <QObject>
#include <QVector>
#include <QMap>
#include "typedefs.h"

//Qt
class QDragEnterEvent;
class QDebug;
class QModelIndex;

//SFLPhone
class Call;
class AccountList;
class Account;
class ContactBackend;

typedef QMap<QString, Call*>  CallMap;
typedef QList<Call*>          CallList;

///@class CallModelBase Base class for the central model/frontend          
///This class need to exist because template classes can't have signals and
///slots because Qt MOC generator can't guess the type at precompilation   
class LIB_EXPORT CallModelBase : public QObject
{
   Q_OBJECT
public:
   CallModelBase(QObject* parent = 0);
   virtual bool changeConference  ( const QString &confId, const QString &state ) = 0;
   virtual void removeConference  ( const QString &confId                       ) = 0;
   virtual Call* addConference    ( const QString &confID                       ) = 0;
   virtual Call* findCallByCallId ( const QString& callId                       ) = 0;
   virtual Call* addRingingCall   ( const QString& callId                       ) = 0;
   virtual Call* addIncomingCall  ( const QString& callId                       ) = 0;
   virtual void  addToHistory     ( Call* call                                  ) = 0;
   virtual Call* addCall          ( Call* call           , Call* parent =0      );
   virtual Call* getCall          ( const QString& callId                       ) const = 0;
public slots:
   void on1_callStateChanged   ( const QString& callID    , const QString &state   );
   void on1_incomingCall       ( const QString& accountID , const QString & callID );
   void on1_incomingConference ( const QString& confID                             );
   void on1_changingConference ( const QString& confID    , const QString &state   );
   void on1_conferenceRemoved  ( const QString& confId                             );
   void on1_voiceMailNotify    ( const QString& accountID , int count              );
   void on1_volumeChanged      ( const QString& device    , double value           );
private:
   static bool dbusInit;
signals:
   void callStateChanged        ( Call* call                              );
   void incomingCall            ( Call* call                              );
   void conferenceCreated       ( Call* conf                              );
   void conferenceChanged       ( Call* conf                              );
   void conferenceRemoved       ( const QString& confId                   );
   void aboutToRemoveConference ( Call* conf                              );
   void voiceMailNotify         ( const QString& accountID , int    count );
   void volumeChanged           ( const QString& device    , double value );
   void callAdded               ( Call* call               , Call* parent );
   void historyChanged          (                                         );
};

/**
 * Note from the author: It was previously done by a QAbstractModel + QTreeView, but the sip-call use case is incompatible  
 *  with the MVC model. The MVC never got to a point were it was bug-free and the code was getting dirty. The Mirror model  
 *  solution may be less "clean" than MVC, but is 3 time smaller and easier to improve (in fact, possible to improve).      
 */
///@class CallModel Central model/frontend to deal with sflphoned
template  <typename CallWidget, typename Index>
class LIB_EXPORT CallModel : public CallModelBase {
   public:
      enum ModelType {
         ActiveCall,
         History,
         Address
      };

      //Constructors, initializer and destructors
      CallModel                ( ModelType type     );
      virtual ~CallModel       (                    ) {}
      virtual bool initCall    (                    );
      virtual bool initHistory (                    );
      virtual void initContact ( ContactBackend* be );

      //Call related
      virtual Call*  addCall          ( Call* call                , Call* parent =0    );
      Call*          addDialingCall   ( const QString& peerName="", QString account="" );
      Call*          addIncomingCall  ( const QString& callId                          );
      Call*          addRingingCall   ( const QString& callId                          );
      static QString generateCallId   (                                                );
      void           removeCall       ( Call* call                                     );
      void           attendedTransfer ( Call* toTransfer           , Call* target      );
      void           transfer         ( Call* toTransfer           , QString target    );
      void           addToHistory     ( Call* call                                     );
      
      virtual bool selectItem(Call* item) { Q_UNUSED(item); return false;}

      //Comference related
      bool createConferenceFromCall  ( Call* call1, Call* call2                    );
      bool mergeConferences          ( Call* conf1, Call* conf2                    );
      bool addParticipant            ( Call* call2, Call* conference               );
      bool detachParticipant         ( Call* call                                  );
      virtual bool changeConference  ( const QString &confId, const QString &state );
      virtual void removeConference  ( const QString &confId                       );
      virtual Call* addConference    ( const QString &confID                       );
      void removeConference          ( Call* call                                  );

      //Getters
      int size                                                     ();
      CallList                              getCallList            ();
      static const CallMap&                getHistory             ();
      static const QStringList              getNumbersByPopularity ();
      static const QStringList getHistoryCallId                    ();

      //Account related
      static Account* getCurrentAccount  (                     );
      static QString getCurrentAccountId (                     );
      static AccountList* getAccountList (                     );
      static QString getPriorAccoundId   (                     );
      static void setPriorAccountId      (const QString& value );

      //Connection related
      static bool init();
      
      //Magic dispatcher
      Call* findCallByCallId( const QString& callId   );
      CallList getCalls     (                         );
      CallList getCalls     ( const CallWidget widget ) const;
      CallList getCalls     ( const QString& callId   ) const;
      CallList getCalls     ( const Call* call        ) const;
      CallList getCalls     ( const Index idx         ) const;
      
      bool isConference     ( const Call* call        ) const;
      bool isConference     ( const QString& callId   ) const;
      bool isConference     ( const Index idx         ) const;
      bool isConference     ( const CallWidget widget ) const;
      
      Call* getCall         ( const QString& callId   ) const;
      Call* getCall         ( const Index idx         ) const;
      Call* getCall         ( const Call* call        ) const;
      Call* getCall         ( const CallWidget widget ) const;
      
      Index getIndex        ( const Call* call        ) const;
      Index getIndex        ( const Index idx         ) const;
      Index getIndex        ( const CallWidget widget ) const;
      Index getIndex        ( const QString& callId   ) const;
      
      CallWidget getWidget  ( const Call* call        ) const;
      CallWidget getWidget  ( const Index idx         ) const;
      CallWidget getWidget  ( const CallWidget widget ) const;
      CallWidget getWidget  ( const QString& getWidget) const;
      
      bool updateIndex      ( Call* call, Index value      );
      bool updateWidget     ( Call* call, CallWidget value );
      
      
   protected:
      //Struct
      struct InternalStruct;
      typedef QList<InternalStruct*> InternalCallList;
      struct InternalStruct {
         CallWidget       call       ;
         Call*            call_real  ;
         Index            index      ;
         InternalCallList children   ;
         bool             conference ;
      };
      typedef QHash< Call*      , InternalStruct* > InternalCall  ;
      typedef QHash< QString    , InternalStruct* > InternalCallId;
      typedef QHash< CallWidget , InternalStruct* > InternalWidget;
      typedef QHash< Index      , InternalStruct* > InternalIndex ;

      //Static attributes
      static CallMap m_sActiveCalls ;
      static CallMap m_sHistoryCalls;
      
      static InternalCall   m_sPrivateCallList_call  ;
      static InternalCallId m_sPrivateCallList_callId;
      static InternalWidget m_sPrivateCallList_widget;
      static InternalIndex  m_sPrivateCallList_index ;
      
      static QString      m_sPriorAccountId;
      static AccountList* m_spAccountList  ;
      static bool         m_sCallInit      ;
      static bool         m_sHistoryInit   ;

   private:
      static bool m_sInstanceInit;

      //Helpers
      Call* addCallCommon(Call* call);
      bool  updateCommon (Call* call);
};

class CallModelConvenience : public CallModel<QWidget*,QModelIndex*>
{
   public:
      CallModelConvenience(ModelType type) : CallModel<QWidget*,QModelIndex*>(type) {}
};

#include "CallModel.hpp"

#endif
