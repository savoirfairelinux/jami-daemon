#ifndef __DEMO_ECHO_SERVER_H
#define __DEMO_ECHO_SERVER_H

#include <dbus-c++/dbus.h>
#include "echo-server-glue.h"

class EchoServer
: public org::freedesktop::DBus::EchoDemo,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
public:

	EchoServer( DBus::Connection& connection );

	DBus::Int32 Random();

	DBus::String Hello( const DBus::String & name );

	DBus::Variant Echo( const DBus::Variant & value );

	std::vector< DBus::Byte > Cat( const DBus::String & file );

	DBus::Int32 Sum( const std::vector<DBus::Int32> & ints );

	std::map< DBus::String, DBus::String > Info();
};

#endif//__DEMO_ECHO_SERVER_H
