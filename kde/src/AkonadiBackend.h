#ifndef AKONADI_BACKEND_H
#define AKONADI_BACKEND_H

#include <QObject>
#include <akonadi/session.h>
#include <akonadi/collectionmodel.h>
#include <kabc/addressee.h>
#include <kabc/addresseelist.h>
#include <lib/ContactBackend.h>

class Contact;

namespace KABC {
   class Addressee;
}

typedef QList<Contact*> ContactList;

class AkonadiBackend : public ContactBackend {
   Q_OBJECT
public:
   static   ContactBackend* getInstance();
   Contact* getContactByPhone ( QString phoneNumber );
   Contact* getContactByUid   ( QString uid         );
   void     editContact       ( Contact* contact    );
   void     addNewContact     ( Contact* contact    );
private:
   AkonadiBackend(QObject* parent);
   virtual ~AkonadiBackend();
   
   //Attributes
   static AkonadiBackend*         m_pInstance       ;
   Akonadi::Session*              m_pSession        ;
   Akonadi::Collection            m_pCollection     ;
   QHash<QString,KABC::Addressee> m_pAddrHash       ;
protected:
   ContactList update_slot();
public slots:
   ContactList update(Akonadi::Collection collection);
   void collectionsReceived( const Akonadi::Collection::List& );
signals:
   void collectionChanged();
};

#endif