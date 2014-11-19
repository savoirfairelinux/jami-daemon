/****************************************************************************
 *   Copyright (C) 2012-2014 by Savoir-Faire Linux                          *
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
#include "useractionmodel.h"

#include "call.h"

//Enabled actions
const TypedStateMachine< TypedStateMachine< bool , Call::State > , UserActionModel::Action > UserActionModel::availableActionMap = {{
            /* INCOMING  RINGING CURRENT DIALING  HOLD  FAILURE BUSY  TRANSFERRED TRANSF_HOLD  OVER  ERROR CONFERENCE CONFERENCE_HOLD:*/
 /*PICKUP   */ {{ true   , true ,  false,  false, false, false, false,   false,     false,    false, false,  false,      false    }},
 /*HOLD     */ {{ false  , false,  true ,  false, false, false, false,   true ,     false,    false, false,  true ,      false    }},
 /*UNHOLD   */ {{ false  , false,  false,  false, true , false, false,   false,     false,    false, false,  false,      false    }},
 /*MUTE     */ {{ false  , true ,  true ,  false, false, false, false,   false,     false,    false, false,  false,      false    }},
 /*TRANSFER */ {{ false  , false,  true ,  false, true , false, false,   false,     false,    false, false,  false,      false    }},
 /*RECORD   */ {{ false  , true ,  true ,  false, true , false, false,   true ,     true ,    false, false,  true ,      true     }},
 /*REFUSE   */ {{ true   , false,  false,  false, false, false, false,   false,     false,    false, false,  false,      false    }},
 /*ACCEPT   */ {{ false  , false,  false,  true , false, false, false,   false,     false,    false, false,  false,      false    }},
 /*HANGUP   */ {{ false  , true ,  true ,  true , true , true , true ,   true ,     true ,    false, true ,  true ,      true     }},
}};

UserActionModel::UserActionModel(Call* parent) : QObject(parent),m_pCall(parent)
{
   Q_ASSERT(parent != nullptr);
}

// QVariant UserActionModel::data(const QModelIndex& index, int role ) const
// {
//    Q_UNUSED( index )
//    Q_UNUSED( role  )
//    return QVariant();
// }
//
// int UserActionModel::rowCount(const QModelIndex& parent ) const
// {
//    Q_UNUSED( parent )
//    return 1;
// }
//
// int UserActionModel::columnCount(const QModelIndex& parent ) const
// {
//    Q_UNUSED( parent )
//    return 1;
// }
//
// ///For now, this model probably wont be used that way
// Qt::ItemFlags UserActionModel::flags(const QModelIndex& index ) const
// {
//    Q_UNUSED(index)
//    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
// }
//
// ///This model is read only
// bool UserActionModel::setData(const QModelIndex& index, const QVariant &value, int role)
// {
//    Q_UNUSED( index )
//    Q_UNUSED( value )
//    Q_UNUSED( role  )
//    return false;
// }

bool UserActionModel::isActionEnabled( UserActionModel::Action action ) const
{
   return availableActionMap[action][m_pCall->state()];
}

void UserActionModel::slotStateChanged()
{
   emit actionStateChanged();
}


uint UserActionModel::relativeIndex( UserActionModel::Action action ) const
{
   int i(0),ret(0);
   while (i != static_cast<int>(action) && i < enum_class_size<UserActionModel::Action>()) {
      ret += isActionEnabled(static_cast<UserActionModel::Action>(i))?1:0;
      i++;
   }
   return ret;
}

uint UserActionModel::enabledCount( ) const
{
   uint ret =0;
   for (int i=0;i< enum_class_size<UserActionModel::Action>(); i++)
      ret += isActionEnabled(static_cast<UserActionModel::Action>(i))?1:0;
   return ret;
}


/****************************************************************
 *                                                              *
 *                         ACTIONS                              *
 *                                                              *
 ***************************************************************/

bool UserActionModel::isPickupEnabled() const
{
   return isActionEnabled(UserActionModel::Action::PICKUP);
}

bool UserActionModel::isHoldEnabled() const
{
   return isActionEnabled(UserActionModel::Action::HOLD);
}

bool UserActionModel::isUnholdEnabled() const
{
   return isActionEnabled(UserActionModel::Action::UNHOLD);
}

bool UserActionModel::isHangupEnabled() const
{
   return isActionEnabled(UserActionModel::Action::HANGUP);
}

bool UserActionModel::isMuteEnabled() const
{
   return isActionEnabled(UserActionModel::Action::MUTE);
}

bool UserActionModel::isTransferEnabled() const
{
   return isActionEnabled(UserActionModel::Action::TRANSFER);
}

bool UserActionModel::isRecordEnabled() const
{
   return isActionEnabled(UserActionModel::Action::RECORD);
}

bool UserActionModel::isRefuseEnabled() const
{
   return isActionEnabled(UserActionModel::Action::REFUSE);
}

bool UserActionModel::isAcceptEnabled() const
{
   return isActionEnabled(UserActionModel::Action::ACCEPT);
}
