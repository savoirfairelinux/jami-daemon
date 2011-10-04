#ifndef CONTACT_BACKEND_H
#define CONTACT_BACKEND_H

#include <QObject>
#include <QHash>

#include "typedefs.h"

//SFLPhone
class Contact;

typedef QList<Contact*> ContactList;

class LIB_EXPORT ContactBackend : public QObject {
   Q_OBJECT
public:
   ContactBackend(QObject* parent);
   virtual Contact*    getContactByPhone ( QString phoneNumber ) = 0;
   virtual Contact*    getContactByUid   ( QString uid         ) = 0;
   virtual void        editContact       ( Contact* contact    ) = 0;
   virtual void        addNewContact     ( Contact* contact    ) = 0;
protected:
   virtual ContactList update_slot       (                     ) = 0;
   QHash<QString,Contact*>        m_pContactByPhone ;
   QHash<QString,Contact*>        m_pContactByUid   ;
public slots:
   ContactList update();
   
private slots:
   
signals:
   
};

#endif