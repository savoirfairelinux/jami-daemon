/****************************************************************************
 *   Copyright (C) 2013-2014 by Savoir-Faire Linux                          *
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
#ifndef PRESENCESTATUSMODEL_H
#define PRESENCESTATUSMODEL_H
#include "typedefs.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QAbstractTableModel>

class PresenceSerializationVisitor;
class AbstractItemBackendBase;

///CredentialModel: A model for account credentials
class LIB_EXPORT PresenceStatusModel : public QAbstractTableModel {
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop
public:

   //Internal representation
   struct StatusData {
      QString  name         ;
      QString  message      ;
      QVariant color        ;
      bool     status       ;
      bool     defaultStatus;
   };

   //Table columns
   enum class Columns {
      Name    = 0,
      Message = 1,
      Color   = 2,
      Status  = 3,
      Default = 4,
   };

   //Methods
   void addStatus(StatusData* status);
   void setPresenceVisitor(PresenceSerializationVisitor* visitor);

   //Properties
   Q_PROPERTY( QString     customMessage     READ customMessage    WRITE  setCustomMessage              NOTIFY customMessageChanged(QString)     )
   Q_PROPERTY( bool        useCustomStatus   READ useCustomStatus  WRITE  setUseCustomStatus            NOTIFY useCustomStatusChanged(bool)      )
   Q_PROPERTY( bool        customStatus      READ customStatus     WRITE  setCustomStatus               NOTIFY customStatusChanged(bool)         )
   Q_PROPERTY( bool        currentStatus     READ currentStatus    NOTIFY currentStatusChanged(bool)                                             )
   Q_PROPERTY( QString     currentMessage    READ currentMessage   NOTIFY currentMessageChanged(QString)                                         )
   Q_PROPERTY( QModelIndex defaultStatus     READ defaultStatus    WRITE  setDefaultStatus              NOTIFY defaultStatusChanged(QModelIndex) )
   Q_PROPERTY( QString     currentName       READ currentName      NOTIFY currentNameChanged(QString)                                            )

   //Constructor
   explicit PresenceStatusModel(QObject* parent = nullptr);
   virtual ~PresenceStatusModel();

   //Abstract model members
   virtual QVariant      data       (const QModelIndex& index, int role = Qt::DisplayRole                 ) const;
   virtual int           rowCount   (const QModelIndex& parent = QModelIndex()                            ) const;
   virtual int           columnCount(const QModelIndex& parent = QModelIndex()                            ) const;
   virtual Qt::ItemFlags flags      (const QModelIndex& index                                             ) const;
   virtual bool          setData    (const QModelIndex& index, const QVariant &value, int role            )      ;
   virtual QVariant      headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;

   //Singleton
   static PresenceStatusModel* instance();

   //Setters
   void setDefaultStatus( const QModelIndex& idx );
   void setAutoTracked(AbstractItemBackendBase* backend, bool tracked) const;

   //Getters
   QString     customMessage   () const;
   bool        useCustomStatus () const;
   bool        customStatus    () const;
   bool        currentStatus   () const;
   QString     currentMessage  () const;
   QString     currentName     () const;
   QModelIndex defaultStatus   () const;
   bool        isAutoTracked(AbstractItemBackendBase* backend) const;

private:

   //Attributes
   QVector<StatusData*> m_lStatuses        ;
   QString              m_CustomMessage    ;
   bool                 m_UseCustomStatus  ;
   bool                 m_CustomStatus     ;
   StatusData*          m_pCurrentStatus   ;
   StatusData*          m_pDefaultStatus   ;
   PresenceSerializationVisitor* m_pVisitor;

   //Singleton
   static PresenceStatusModel* m_spInstance;

public Q_SLOTS:
   void addRow            (                          );
   void removeRow         ( const QModelIndex& index );
   void save              (                          );
   void moveUp            ( const QModelIndex& index );
   void moveDown          ( const QModelIndex& index );
   void setUseCustomStatus( bool useCustom           );
   void setCustomStatus   ( bool status              );
   void setCurrentIndex   ( const QModelIndex& index );
   void setCustomMessage  ( const QString& message   );

Q_SIGNALS:
   void currentIndexChanged   ( const QModelIndex& current   );
   void currentNameChanged    ( const QString&     name      );
   void useCustomStatusChanged( bool               useCustom );
   void customMessageChanged  ( const QString&     message   );
   void customStatusChanged   ( bool               status    );
   void defaultStatusChanged  ( const QModelIndex& def       );
   void currentMessageChanged ( const QString&     message   );
   void currentStatusChanged  ( bool               status    );

};

Q_DECLARE_METATYPE( PresenceStatusModel* )

#endif
