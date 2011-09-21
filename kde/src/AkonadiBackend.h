#ifndef AKONADI_BACKEND_H
#define AKONADI_BACKEND_H

#include <QObject>
#include <akonadi/session.h>
#include <akonadi/collectionmodel.h>
#include <kabc/addressee.h>
#include <kabc/addresseelist.h>

class Contact;

typedef QList<Contact*> ContactList;

class AkonadiBackend : public QObject {
   Q_OBJECT
public:
   static AkonadiBackend* getInstance();
   Contact* getContactByPhone(QString phoneNumber);
   Contact* getContactByUid(QString uid);
private:
   AkonadiBackend(QObject* parent);
   virtual ~AkonadiBackend();
   
   //Attributes
   static AkonadiBackend*   m_pInstance;
   Akonadi::Session*        m_pSession;
   Akonadi::Collection      m_pCollection;
   QHash<QString,Contact*>  m_pContactByPhone;
   QHash<QString,Contact*>  m_pContactByUid;
public slots:
   ContactList update();
   ContactList update(Akonadi::Collection collection);
   void collectionsReceived( const Akonadi::Collection::List& );
signals:
   void collectionChanged();
};

#endif