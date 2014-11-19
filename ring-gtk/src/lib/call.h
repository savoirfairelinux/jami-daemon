/****************************************************************************
 *   Copyright (C) 2009-2014 by Savoir-Faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#ifndef CALL_H
#define CALL_H

#include "categorizedcompositenode.h"
#include <time.h>

//Qt
#include <QtCore/QDebug>
class QString;
class QTimer;

//SFLPhone
#include "sflphone_const.h"
#include "typedefs.h"
#include "historytimecategorymodel.h"
class Account;
class VideoRenderer;
class InstantMessagingModel;
class UserActionModel;
class PhoneNumber;
class TemporaryPhoneNumber;
class AbstractHistoryBackend;

class Call;

typedef  void (Call::*function)();

/**
 *  This class represents a call either actual (in the call list
 *  displayed in main window), either past (in the call history).
 *  A call is represented by an automate, with a list of states
 *  (enum call_state) and 2 lists of transition signals
 *  (call_action when the user performs an action on the UI and
 *  daemon_call_state when the daemon sends a stateChanged signal)
 *  When a transition signal is received, the automate calls a
 *  function then go to a new state according to the previous state
 *  of the call and the signal received.
 *  The functions to call and the new states to go to are placed in
 *  the maps actionPerformedStateMap, actionPerformedFunctionMap,
 *  stateChangedStateMap and stateChangedFunctionMap.
 *  Those maps are used by actionPerformed and stateChanged functions
 *  to handle the behavior of the automate.
 *  When an actual call goes to the state OVER, it becomes part of
 *  the call history.
 *
 *  It may be better to handle call list and call history separately,
 *  and to use the class Item to handle their display, or a model/view
 *  way. For this it needs to handle the becoming of a call to a past call
 *  keeping the information gathered by the call and needed by the history
 *  call (history state, start time...).
**/
class  LIB_EXPORT Call : public QObject
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:
   friend class CallModel;
   //Enum

   ///Model roles
   enum Role {
      Name          = 100,
      Number        = 101,
      Direction2    = 102,
      Date          = 103,
      Length        = 104,
      FormattedDate = 105,
      HasRecording  = 106,
      Historystate  = 107,
      Filter        = 108,
      FuzzyDate     = 109,
      IsBookmark    = 110,
      Security      = 111,
      Department    = 112,
      Email         = 113,
      Organisation  = 114,
      Object        = 117,
      PhotoPtr      = 118,
      CallState     = 119,
      Id            = 120,
      StartTime     = 121,
      StopTime      = 122,
      IsRecording   = 123,
      PhoneNu       = 124,
      IsPresent     = 125,
      SupportPresence=126,
      IsTracked     = 127,
      CategoryIcon  = 128,
      CallCount     = 129, /* The number of calls made with the same phone number */
      TotalSpentTime= 130, /* The total time spent speaking to with this phone number*/
      Missed        = 131,
      CallLifeCycleState= 132,
      DropState     = 300,
      DTMFAnimState = 400,
      LastDTMFidx   = 401,
      DropPosition  = 402,
   };

   enum DropAction {
      Conference = 100,
      Transfer   = 101,
   };

   ///Possible call states
   enum class State : unsigned int{
      INCOMING        = 0, /** Ringing incoming call */
      RINGING         = 1, /** Ringing outgoing call */
      CURRENT         = 2, /** Call to which the user can speak and hear */
      DIALING         = 3, /** Call which numbers are being added by the user */
      HOLD            = 4, /** Call is on hold */
      FAILURE         = 5, /** Call has failed */
      BUSY            = 6, /** Call is busy */
      TRANSFERRED     = 7, /** Call is being transferred.  During this state, the user can enter the new number. */
      TRANSF_HOLD     = 8, /** Call is on hold for transfer */
      OVER            = 9, /** Call is over and should not be used */
      ERROR           = 10,/** This state should never be reached */
      CONFERENCE      = 11,/** This call is the current conference*/
      CONFERENCE_HOLD = 12,/** This call is a conference on hold*/
      INITIALIZATION  = 13,/** The call have been placed, but the peer hasn't confirmed yet */
      __COUNT,
   };
   Q_ENUMS(State)

   /**
   * @enum Call::LegacyHistoryState
   * History items create before December 2013 will have a "state" field
   * mixing direction and missed. Newer items will have separated fields for that.
   *
   * SFLPhone-KDE will keep support for at least a year
   */
   enum class LegacyHistoryState : int //FIXME remove
   {
      INCOMING,
      OUTGOING,
      MISSED  ,
      NONE
   };

   ///@enum Direction If the user have been called or have called
   enum class Direction : int {
      INCOMING, /** Someone has called      */
      OUTGOING, /** The user called someone */
   };
   Q_ENUMS(Direction)

   ///@class HistoryStateName history map fields state names
   class HistoryStateName {
   public:
      constexpr static const char* MISSED         = "missed"  ;
      constexpr static const char* INCOMING       = "incoming";
      constexpr static const char* OUTGOING       = "outgoing";
   };

   ///@class ConferenceStateChange Possible values from "conferencechanged" signal
   class ConferenceStateChange {
   public:
      constexpr static const char* HOLD           = "HOLD"           ;
      constexpr static const char* ACTIVE         = "ACTIVE_ATTACHED";
   };

   class StateChange {
   public:
      constexpr static const char* HUNG_UP        = "HUNGUP" ;
      constexpr static const char* RINGING        = "RINGING";
      constexpr static const char* CURRENT        = "CURRENT";
      constexpr static const char* HOLD           = "HOLD"   ;
      constexpr static const char* BUSY           = "BUSY"   ;
      constexpr static const char* FAILURE        = "FAILURE";
      constexpr static const char* UNHOLD_CURRENT = "UNHOLD" ;
   };

   class DaemonStateInit {
   public:
      constexpr static const char* CURRENT  = "CURRENT"  ;
      constexpr static const char* HOLD     = "HOLD"     ;
      constexpr static const char* BUSY     = "BUSY"     ;
      constexpr static const char* INCOMING = "INCOMING" ;
      constexpr static const char* RINGING  = "RINGING"  ;
      constexpr static const char* INACTIVE = "INACTIVE" ;
   };

   ///"getHistory()" fields
   class HistoryMapFields {
   public:
      constexpr static const char* ACCOUNT_ID        = "accountid"      ;
      constexpr static const char* CALLID            = "callid"         ;
      constexpr static const char* DISPLAY_NAME      = "display_name"   ;
      constexpr static const char* PEER_NUMBER       = "peer_number"    ;
      constexpr static const char* RECORDING_PATH    = "recordfile"     ;
      constexpr static const char* STATE             = "state"          ;
      constexpr static const char* TIMESTAMP_START   = "timestamp_start";
      constexpr static const char* TIMESTAMP_STOP    = "timestamp_stop" ;
      constexpr static const char* MISSED            = "missed"         ;
      constexpr static const char* DIRECTION         = "direction"      ;
      constexpr static const char* CONTACT_USED      = "contact_used"   ;
      constexpr static const char* CONTACT_UID       = "contact_uid"    ;
      constexpr static const char* NUMBER_TYPE       = "number_type"    ;
   };

   ///"getCallDetails()" fields
   class DetailsMapFields {
   public:
      constexpr static const char* PEER_NAME         = "DISPLAY_NAME"   ;
      constexpr static const char* PEER_NUMBER       = "PEER_NUMBER"    ;
      constexpr static const char* ACCOUNT_ID        = "ACCOUNTID"      ;
      constexpr static const char* STATE             = "CALL_STATE"     ;
      constexpr static const char* TYPE              = "CALL_TYPE"      ;
      constexpr static const char* TIMESTAMP_START   = "TIMESTAMP_START";
      constexpr static const char* CONF_ID           = "CONF_ID"        ;
   };

   ///"getConferenceDetails()" fields
   class ConfDetailsMapFields {
   public:
      constexpr static const char* CONF_STATE        = "CONF_STATE"     ;
      constexpr static const char* CONFID            = "CONFID"         ;
   };

   ///If the call is incoming or outgoing
   class CallDirection {
   public:
      constexpr static const char* INCOMING = "0";
      constexpr static const char* OUTGOING = "1";
   };

   ///Is the call between one or more participants
   enum class Type {
      CALL      , /** A simple call                  */
      CONFERENCE, /** A composition of other calls   */
      HISTORY   , /** A call from a previous session */
   };

   /** @enum Call::DaemonState
   * This enum have all the states a call can take for the daemon.
   */
   enum class DaemonState : unsigned int
   {
      RINGING = 0, /** Ringing outgoing or incoming call */
      CURRENT = 1, /** Call to which the user can speak and hear */
      BUSY    = 2, /** Call is busy */
      HOLD    = 3, /** Call is on hold */
      HUNG_UP = 4, /** Call is over  */
      FAILURE = 5, /** Call has failed */
      __COUNT,
   };

   /** @enum Call::Action
   * This enum have all the actions you can make on a call.
   */
   enum class Action : unsigned int
   {
      ACCEPT   = 0, /** Accept, create or place call or place transfer */
      REFUSE   = 1, /** Red button, refuse or hang up */
      TRANSFER = 2, /** Put into or out of transfer mode*/
      HOLD     = 3, /** Hold or unhold the call */
      RECORD   = 4, /** Enable or disable recording */
      __COUNT,
   };

   /** @enum Call::LifeCycleState
    * This enum help track the call meta state
    * @todo Eventually add a meta state between progress and finished for
    *  calls that are still relevant enough to be in the main UI, such
    *  as BUSY OR FAILURE while also finished
    */
   enum class LifeCycleState {
      INITIALIZATION = 0, /** Anything before the media transfer start   */
      PROGRESS       = 1, /** The peers are in communication (or hold)   */
      FINISHED       = 2, /** Everything is over, there is no going back */
      __COUNT
   };

   //Read only properties
   Q_PROPERTY( Call::State        state            READ state             NOTIFY stateChanged     )
   Q_PROPERTY( QString            id               READ id                                        )
   Q_PROPERTY( Account*           account          READ account                                   )
   Q_PROPERTY( bool               isHistory        READ isHistory                                 )
   Q_PROPERTY( uint               stopTimeStamp    READ stopTimeStamp                             )
   Q_PROPERTY( uint               startTimeStamp   READ startTimeStamp                            )
   Q_PROPERTY( bool               isSecure         READ isSecure                                  )
   Q_PROPERTY( VideoRenderer*     videoRenderer    READ videoRenderer                             )
   Q_PROPERTY( QString            formattedName    READ formattedName                             )
   Q_PROPERTY( QString            length           READ length                                    )
   Q_PROPERTY( bool               hasRecording     READ hasRecording                              )
   Q_PROPERTY( bool               recording        READ isRecording                               )
   Q_PROPERTY( UserActionModel*   userActionModel  READ userActionModel   CONSTANT                )
   Q_PROPERTY( QString            toHumanStateName READ toHumanStateName                          )
   Q_PROPERTY( bool               missed           READ isMissed                                  )
   Q_PROPERTY( Direction          direction        READ direction                                 )
   Q_PROPERTY( bool               hasVideo         READ hasVideo                                  )
   Q_PROPERTY( Call::LegacyHistoryState historyState     READ historyState                        )

   //Read/write properties
   Q_PROPERTY( PhoneNumber*       peerPhoneNumber  READ peerPhoneNumber                           )
   Q_PROPERTY( QString            peerName         READ peerName          WRITE setPeerName       )
   Q_PROPERTY( QString            transferNumber   READ transferNumber    WRITE setTransferNumber )
   Q_PROPERTY( QString            recordingPath    READ recordingPath     WRITE setRecordingPath  )
   Q_PROPERTY( QString            dialNumber       READ dialNumber        WRITE setDialNumber      NOTIFY dialNumberChanged(QString))

   //Constructors & Destructors
   ~Call();
   static Call* buildDialingCall  (const QString& callId, const QString & peerName, Account* account = nullptr );
   static Call* buildIncomingCall (const QString& callId                                                       );
   static Call* buildRingingCall  (const QString& callId                                                       );
   static Call* buildHistoryCall  (const QMap<QString,QString>& hc                                             );
   static Call* buildExistingCall (const QString& callId                                                       );

   //Static getters
   static Call::LegacyHistoryState historyStateFromType    ( const QString& type                                           );
   static Call::State        startStateFromDaemonCallState ( const QString& daemonCallState, const QString& daemonCallType );
   static const QString      toHumanStateName              ( const Call::State                                             );

   //Getters
   Call::State              state            () const;
   const QString            id               () const;
   PhoneNumber*             peerPhoneNumber  () const;
   const QString            peerName         () const;
   Call::LegacyHistoryState historyState     () const;
   bool                     isRecording      () const;
   Account*                 account          () const;
   bool                     isHistory        () const;
   time_t                   stopTimeStamp    () const;
   time_t                   startTimeStamp   () const;
   bool                     isSecure         () const;
   const QString            transferNumber   () const;
   const QString            dialNumber       () const;
   const QString            recordingPath    () const;
   VideoRenderer*           videoRenderer    () const;
   const QString            formattedName    () const;
   bool                     hasRecording     () const;
   QString                  length           () const;
   QVariant                 roleData         (int role) const;
   UserActionModel*         userActionModel  () const;
   QString                  toHumanStateName () const;
   bool                     isHistory        ()      ;
   bool                     isMissed         () const;
   Call::Direction          direction        () const;
   AbstractHistoryBackend*  backend          () const;
   bool                     hasVideo         () const;
   Call::LifeCycleState     lifeCycleState   () const;
   Call::Type               type             () const;

   //Automated function
   Call::State stateChanged(const QString & newState);
   Call::State performAction(Call::Action action);

   //Setters
   void setTransferNumber ( const QString&     number     );
   void setDialNumber     ( const QString&     number     );
   void setDialNumber     ( const PhoneNumber* number     );
   void setRecordingPath  ( const QString&     path       );
   void setPeerName       ( const QString&     name       );
   void setAccount        ( Account*           account    );
   void setBackend        ( AbstractHistoryBackend* backend);

   //Mutators
   void appendText(const QString& str);
   void backspaceItemText();
   void reset();
   void sendTextMessage(const QString& message);

   //syntactic sugar
   Call* operator<<( Call::Action& c);

private:

   //Attributes
   Account*                 m_Account         ;
   QString                  m_CallId          ;
   PhoneNumber*             m_pPeerPhoneNumber;
   QString                  m_PeerName        ;
   QString                  m_RecordingPath   ;
   Call::LegacyHistoryState m_HistoryState    ;
   time_t                   m_pStartTimeStamp ;
   time_t                   m_pStopTimeStamp  ;
   TemporaryPhoneNumber*    m_pTransferNumber ;
   TemporaryPhoneNumber*    m_pDialNumber     ;
   Call::State              m_CurrentState    ;
   bool                     m_Recording       ;
   InstantMessagingModel*   m_pImModel        ;
   QTimer*                  m_pTimer          ;
   UserActionModel*         m_pUserActionModel;
   bool                     m_History         ;
   bool                     m_Missed          ;
   Call::Direction          m_Direction       ;
   Call::Type               m_Type            ;
   AbstractHistoryBackend*  m_pBackend        ;

   //Cache
   HistoryTimeCategoryModel::HistoryConst m_HistoryConst;

   //State machine
   /**
    *  actionPerformedStateMap[orig_state][action]
    *  Map of the states to go to when the action action is
    *  performed on a call in state orig_state.
   **/
   static const TypedStateMachine< TypedStateMachine< Call::State , Call::Action > , Call::State > actionPerformedStateMap;

   /**
    *  actionPerformedFunctionMap[orig_state][action]
    *  Map of the functions to call when the action action is
    *  performed on a call in state orig_state.
   **/
   static const TypedStateMachine< TypedStateMachine< function , Call::Action > , Call::State > actionPerformedFunctionMap;

   /**
    *  stateChangedStateMap[orig_state][daemon_new_state]
    *  Map of the states to go to when the daemon sends the signal
    *  callStateChanged with arg daemon_new_state
    *  on a call in state orig_state.
   **/
   static const TypedStateMachine< TypedStateMachine< Call::State , Call::DaemonState > , Call::State > stateChangedStateMap;

   /**
    *  stateChangedFunctionMap[orig_state][daemon_new_state]
    *  Map of the functions to call when the daemon sends the signal
    *  callStateChanged with arg daemon_new_state
    *  on a call in state orig_state.
   **/
   static const TypedStateMachine< TypedStateMachine< function , Call::DaemonState > , Call::State > stateChangedFunctionMap;

   /**
    * metaStateTransitionValidationMap help validate if a state transition violate the lifecycle logic.
    * it should technically never happen, but this is an easy additional safety to implement
    * and prevent human (developer) errors.
    */
   static const TypedStateMachine< TypedStateMachine< bool , Call::LifeCycleState > , Call::State > metaStateTransitionValidationMap;

   /**
    * Convert the call state into its meta state (life cycle state). The meta state is a flat,
    * forward only progression from creating to archiving of a call.
    */
   static const TypedStateMachine< Call::LifeCycleState , Call::State > metaStateMap;

   explicit Call(const QString& confId, const QString& account);
   Call(Call::State startState, const QString& callId, const QString& peerName = QString(), PhoneNumber* number = nullptr, Account* account = nullptr);
   static Call::DaemonState toDaemonCallState   (const QString& stateName);
   static Call::State       confStatetoCallState(const QString& stateName);

   //Automate functions
   // See actionPerformedFunctionMap and stateChangedFunctionMap
   // to know when it is called.
   void nothing      () __attribute__ ((const));
   void error        () __attribute__ ((noreturn));
   void failure      ();
   void accept       ();
   void refuse       ();
   void acceptTransf ();
   void acceptHold   ();
   void hangUp       ();
   void cancel       ();
   void hold         ();
   void call         ();
   void transfer     ();
   void unhold       ();
   void switchRecord ();
   void setRecord    ();
   void start        ();
   void startStop    ();
   void stop         ();
   void startWeird   ();
   void warning      ();
   void remove       ();

   //Helpers
   void changeCurrentState(Call::State newState);
   void setStartTimeStamp(time_t stamp);
   void initTimer();

public Q_SLOTS:
   void playRecording();
   void stopRecording();
   void seekRecording(double position);
   void playDTMF(const QString& str);

private Q_SLOTS:
   void stopPlayback(const QString& filePath);
   void updatePlayback(const QString& path,int position,int size);
   void updated();

Q_SIGNALS:
   ///Emitted when a call change (state or details)
   void changed();
   void changed(Call* self);
   ///Emitted when the call is over
   void isOver(Call*);
   void playbackPositionChanged(int,int);
   void playbackStopped();
   void playbackStarted();
   ///Notify that a DTMF have been played
   void dtmfPlayed(const QString& str);
   ///Notify of state change
   void stateChanged();
   void startTimeStampChanged(time_t newTimeStamp);
   void dialNumberChanged(const QString& number);
};

Q_DECLARE_METATYPE(Call*)

QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::State& c       );
QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::DaemonState& c );
QDebug LIB_EXPORT operator<<(QDebug dbg, const Call::Action& c      );

#endif
