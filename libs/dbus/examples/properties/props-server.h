#ifndef __DEMO_PROPS_SERVER_H
#define __DEMO_PROPS_SERVER_H

#include <dbus-c++/dbus.h>
#include "props-glue.h"

class PropsServer
: public org::freedesktop::DBus::PropsDemo,
  public DBus::IntrospectableAdaptor,
  public DBus::PropertiesAdaptor,
  public DBus::ObjectAdaptor
{
public:

	PropsServer( DBus::Connection& connection );

	void on_set_property
		( DBus::InterfaceAdaptor& interface, const DBus::String& property, const DBus::Variant& value );
};

#endif//__DEMO_PROPS_SERVER_H
