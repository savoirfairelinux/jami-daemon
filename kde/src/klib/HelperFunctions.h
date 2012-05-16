#ifndef HELPER_FUNCTIONS
#define HELPER_FUNCTIONS

//Qt
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QHash>
#include <QtCore/QList>

//SFLPhone
#include "../lib/Contact.h"

//Typedef
typedef QHash<QString,QHash<QString,QVariant> > ContactHash;

///@class HelperFunctions little visitor not belonging to libqtsflphone
///Ramdom mix of dynamic property and transtypping
class LIB_EXPORT HelperFunctions {
public:
   static ContactHash toHash(QList<Contact*> contacts);
};
#endif