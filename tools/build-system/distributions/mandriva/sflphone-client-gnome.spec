#####################################################
# File Name: sflphone-client-gnome.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-05-27 17:23:32 -0400
#####################################################

Name:           sflphone-client-gnome
License:        GNU General Public License (GPL)
Group:          Productivity/Networking/System
Summary:        GNOME client for SFLphone
Version:        VERSION
Release:        mandriva
URL:            http://www.sflphone.org/
Vendor:		Savoir-faire Linux
Packager:       Julien Bonjean <julien.bonjean@savoirfairelinux.com>

BuildRoot:      %{_tmppath}/%{name}-%{version}
Source0:        sflphone-client-gnome.tar.gz
BuildRequires:	gtk2-devel
BuildRequires:	libnotify-devel
BuildRequires:	libsexy-devel
BuildRequires:	evolution-data-server-devel
BuildRequires:	check-devel
BuildRequires:	libdbus-glib-devel
BuildRequires:	log4c-devel
Requires:	sflphone-common = %{version}
Requires:	dbus-1-glib
Requires:	gtk2
Requires:	glib2
Requires:	dbus-1-glib
Requires:	libnotify
Requires:	librsvg
Requires:	log4c
Requires:	libsexy
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
./autogen.sh --prefix=%{_prefix}
make -j

%install
make prefix=%{buildroot}/%{_prefix} install

%clean
make clean

%files
%defattr(-, root, root)
%{_prefix}/*
%doc AUTHORS COPYING README

%changelog
