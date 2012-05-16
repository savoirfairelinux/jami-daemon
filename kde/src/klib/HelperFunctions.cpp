#include "HelperFunctions.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QVariant>

//SFLPhone
#include "../lib/Contact.h"

ContactHash HelperFunctions::toHash(QList<Contact*> contacts) {
   QHash<QString,QHash<QString,QVariant> > hash;
   for (int i=0;i<contacts.size();i++) {
      hash[contacts[i]->getUid()] = contacts[i]->toHash();
   }
   return hash;
}