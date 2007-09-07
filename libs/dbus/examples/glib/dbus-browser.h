#ifndef __DEMO_DBUS_BROWSER_H
#define __DEMO_DBUS_BROWSER_H

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include <gtkmm.h>

#include "dbus-glue.h"

class DBusInspector
: public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

	DBusInspector( DBus::Connection& conn, const char* path, const char* service )
	: DBus::ObjectProxy(conn, path, service)
	{}
};

class DBusBrowser
: public org::freedesktop::DBus,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy,
  public Gtk::Window
{
public:

	DBusBrowser( ::DBus::Connection& );

private:

	void NameOwnerChanged( const ::DBus::String&, const ::DBus::String&, const ::DBus::String& );

	void NameLost( const ::DBus::String& );

	void NameAcquired( const ::DBus::String& );

	void on_select_busname();

	void _inspect_append( Gtk::TreeModel::Row*, const std::string&, const std::string& );

private:

	class InspectRecord : public Gtk::TreeModel::ColumnRecord
	{
	public:

		InspectRecord() { add(name); }

		Gtk::TreeModelColumn<Glib::ustring> name;
	};

	Gtk::VBox			_vbox;
	Gtk::ScrolledWindow		_sc_tree;
	Gtk::ComboBoxText		_cb_busnames;
	Gtk::TreeView			_tv_inspect;
	Glib::RefPtr<Gtk::TreeStore>	_tm_inspect;
	InspectRecord			_records;
};

#endif//__DEMO_DBUS_BROWSER_H
