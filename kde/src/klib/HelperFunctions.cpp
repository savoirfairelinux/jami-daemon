#include "HelperFunctions.h"

//Qt
#include <QtCore/QString>
#include <QtCore/QVariant>

//SFLPhone
#include "../lib/Contact.h"

ContactHash HelperFunctions::toHash(QList<Contact*> contacts) {
   QHash<QString,QHash<QString,QVariant> > hash;
   for (int i=0;i<contacts.size();i++) {
      Contact* cont = contacts[i];
      QHash<QString,QVariant> conth = cont->toHash();
      conth["phoneCount"] = cont->getPhoneNumbers().size();
      if (cont->getPhoneNumbers().size() == 1) {
         conth["phoneNumber"] = cont->getPhoneNumbers()[0]->getNumber();
         conth["phoneType"  ] = cont->getPhoneNumbers()[0]->getType();
      }
      else {
         conth["phoneNumber"] = QString::number(cont->getPhoneNumbers().size())+" numbers";
         conth["phoneType"  ] = "";
      }
      hash[contacts[i]->getUid()] = conth;
   }
   return hash;
}