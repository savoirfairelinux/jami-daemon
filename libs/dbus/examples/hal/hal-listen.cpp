#include "hal-listen.h"

#include <signal.h>
#include <iostream>

HalManagerProxy::HalManagerProxy( DBus::Connection& connection )
: DBus::InterfaceProxy("org.freedesktop.Hal.Manager"),
  DBus::ObjectProxy(connection, "/org/freedesktop/Hal/Manager", "org.freedesktop.Hal")
{
	connect_signal(HalManagerProxy, DeviceAdded, DeviceAddedCb);
	connect_signal(HalManagerProxy, DeviceRemoved, DeviceRemovedCb);

	std::vector< DBus::String > devices = GetAllDevices();

	std::vector< DBus::String >::iterator it;
	for(it = devices.begin(); it != devices.end(); ++it)
	{
		DBus::Path udi = *it;

		std::cout << "found device " << udi << std::endl;

		_devices[udi] = new HalDeviceProxy(connection, udi);
	}
}

std::vector< DBus::String > HalManagerProxy::GetAllDevices()
{
	std::vector< DBus::String > udis;
	DBus::CallMessage call;

	call.member("GetAllDevices");

	DBus::Message reply = invoke_method(call);
	DBus::MessageIter it = reply.reader();

	it >> udis;
	return udis;
}

void HalManagerProxy::DeviceAddedCb( const DBus::SignalMessage& sig )
{
	DBus::MessageIter it = sig.reader();
	DBus::String devname;

	it >> devname;

	DBus::Path udi(devname);

	_devices[devname] = new HalDeviceProxy(conn(), udi);
	std::cout << "added device " << udi << std::endl;
}

void HalManagerProxy::DeviceRemovedCb( const DBus::SignalMessage& sig )
{
	DBus::MessageIter it = sig.reader();
	DBus::String devname;

	it >> devname;

	std::cout << "removed device " << devname << std::endl;

	_devices.erase(devname);
}

HalDeviceProxy::HalDeviceProxy( DBus::Connection& connection, DBus::Path& udi )
: DBus::InterfaceProxy("org.freedesktop.Hal.Device"),
  DBus::ObjectProxy(connection, udi, "org.freedesktop.Hal")
{
	connect_signal(HalDeviceProxy, PropertyModified, PropertyModifiedCb);
	connect_signal(HalDeviceProxy, Condition, ConditionCb);
}

void HalDeviceProxy::PropertyModifiedCb( const DBus::SignalMessage& sig )
{
	typedef DBus::Struct< DBus::String, DBus::Bool, DBus::Bool > HalProperty;

	DBus::MessageIter it = sig.reader();
	DBus::Int32 number;

	it >> number;

	DBus::MessageIter arr = it.recurse();

	for(int i = 0; i < number; ++i, ++arr)
	{
		HalProperty hp;

		arr >> hp;

		std::cout << "modified property " << hp._1 << " in " << path() << std::endl;
	}
}

void HalDeviceProxy::ConditionCb( const DBus::SignalMessage& sig )
{
	DBus::MessageIter it = sig.reader();
	DBus::String condition;

	it >> condition;

	std::cout << "encountered condition " << condition << " in " << path() << std::endl;
}

DBus::BusDispatcher dispatcher;

void niam( int sig )
{
	dispatcher.leave();
}

int main()
{
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SystemBus();

	HalManagerProxy hal(conn);

	dispatcher.enter();

	return 0;
}
