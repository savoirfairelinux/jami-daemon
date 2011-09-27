#include "ContactBackend.h"

#include "Contact.h"
#include <QHash>

ContactBackend::ContactBackend(QObject* parent) : QObject(parent)
{
   
}

ContactList ContactBackend::update()
{
   return update_slot();
}
