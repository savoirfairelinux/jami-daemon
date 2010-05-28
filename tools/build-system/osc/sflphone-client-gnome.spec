#####################################################
# File Name: sflphone-client-gnome.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-10-07
#####################################################

Name:           sflphone-client-gnome
License:        GNU General Public License (GPL)
Group:          Productivity/Networking/System
Summary:        GNOME client for SFLphone
Version:        VERSION
Release:        VERSION_INDEX%{?dist}
URL:            http://www.sflphone.org/
Vendor:		Savoir-faire Linux
Packager:       Julien Bonjean <julien.bonjean@savoirfairelinux.com>

Group:          Applications/Communications
BuildRoot:      %{_tmppath}/%{name}
Source0:        sflphone-client-gnome-%{version}.tar.gz

%if %{defined suse_version}
BuildRequires:  scrollkeeper
BuildRequires:	update-desktop-files
BuildRequires:	liblog4c-devel
BuildRequires:	dbus-1-glib-devel
BuildRequires:	gnome-doc-utils-devel	
BuildRequires:	libgnomeui-devel
%endif

%if %{defined fedora_version}
BuildRequires:  rarian-compat
BuildRequires:	log4c-devel
BuildRequires:	dbus-glib-devel
BuildRequires:	gnome-doc-utils
BuildRequires:	libgnomeui-devel
BuildRequires:  gettext
# fix libproxy-pac expansion error
BuildRequires:	libproxy-webkit
# fix PolicyKit-authentication-agent expansion error
BuildRequires:	PolicyKit-gnome
%endif

%if %{defined mandriva_version}
BuildRequires:	log4c-devel
BuildRequires:	libgnomeui2-devel
BuildRequires:	libdbus-glib-1_2-devel
BuildRequires:	gnome-doc-utils
# fix libesound0 expansion error
BuildRequires:	pulseaudio-esound-compat
# fix libproxy-pac expansion error
BuildRequires:	libproxy-webkit
%endif

BuildRequires:	gtk2-devel
BuildRequires:	libnotify-devel
BuildRequires:	libsexy-devel
BuildRequires:	evolution-data-server-devel
BuildRequires:	check-devel

Requires:	sflphone-common = %{version}
Requires:	dbus-1-glib
Requires:	gtk2
Requires:	glib2
Requires:	libnotify
Requires:	libsexy

%if %{defined suse_version}
Requires:	librsvg
Requires:	liblog4c3
%endif

%if %{defined fedora_version}
Requires:	librsvg2
Requires:	log4c
%endif

Conflicts:	sflphone
Prefix:		%{_prefix}

%description
Provide a GNOME client for SFLphone.
 SFLphone is meant to be a robust enterprise-class desktop phone.
 SFLphone is released under the GNU General Public License.
 SFLphone is being developed by the global community, and maintained by
 Savoir-faire Linux, a Montreal, Quebec, Canada-based Linux consulting company.

Authors:
--------
    Julien Bonjean <julien.bonjean@savoirfairelinux.com>

%lang_package

%prep
%setup -q


%build
export SUSE_ASNEEDED=0 # fix opensuse linking issue (Since 11.2 uses default --as-needed for linking, the order of libraries is important)
./autogen.sh
./configure --prefix=%{_prefix}
make -j


%install
make DESTDIR=%{buildroot} install
%if %{defined suse_version}
%suse_update_desktop_file -n %{buildroot}/%{_prefix}/share/applications/sflphone.desktop
%endif
rm -rf $RPM_BUILD_ROOT/var/lib/scrollkeeper 

%clean
make clean

%files
%defattr(-, root, root)
%dir %{_prefix}/share/sflphone
%dir %{_prefix}/share/sflphone/ui
%dir %{_prefix}/share/omf/sflphone
%lang(fr) %{_prefix}/share/locale/fr/LC_MESSAGES/*.mo
%lang(es) %{_prefix}/share/locale/es/LC_MESSAGES/*.mo
%lang(de) %{_prefix}/share/locale/de/LC_MESSAGES/*.mo
%lang(ru) %{_prefix}/share/locale/ru/LC_MESSAGES/*.mo
%lang(zh_CN) %{_prefix}/share/locale/zh_CN/LC_MESSAGES/*.mo
%lang(zh_HK) %{_prefix}/share/locale/zh_HK/LC_MESSAGES/*.mo
%lang(it) %{_prefix}/share/locale/it/LC_MESSAGES/*.mo
%lang(pt_BR) %{_prefix}/share/locale/pt_BR/LC_MESSAGES/*.mo
%lang(da) %{_prefix}/share/locale/da/LC_MESSAGES/*.mo
%doc AUTHORS COPYING README
%doc %{_prefix}/share/man/man1/sflphone-client-gnome.1.gz
%doc %{_prefix}/share/man/man1/sflphone.1.gz
%doc %{_prefix}/share/gnome/help/sflphone
%{_prefix}/bin/sflphone
%{_prefix}/bin/sflphone-client-gnome
%{_prefix}/share/pixmaps/sflphone.svg
%{_prefix}/share/applications/sflphone.desktop
%{_prefix}/share/sflphone/*.svg
%{_prefix}/share/sflphone/*.gif
%{_prefix}/share/sflphone/log4crc
%{_prefix}/share/omf/sflphone/*
%{_prefix}/share/sflphone/ui/ui.xml

%changelog

