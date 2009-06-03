#####################################################
# File Name: sflphone-client-kde.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-05-27 17:23:32 -0400
#####################################################

Name:           sflphone-client-kde
License:        GNU General Public License (GPL)
Group:          Productivity/Networking/System
Summary:        KDE client for SFLphone
Version:        VERSION
Release:        mandriva
URL:            http://www.sflphone.org/
Vendor:		Savoir-faire Linux
Packager:       Julien Bonjean <julien.bonjean@savoirfairelinux.com>

BuildRoot:      %{_tmppath}/%{name}-%{version}
Source0:        sflphone-client-kde.tar.gz
Requires:	sflphone-common = %{version}
Requires:	commoncpp2
Requires:	libkdepimlibs4
Requires:	libqt4-dbus-1
Requires:	libqt4-svg
Requires:	libqt4-x11
BuildRequires:  cmake
BuildRequires:  libcommoncpp-devel
BuildRequires:  kdepimlibs4-devel
Conflicts:	sflphone
Prefix:		%{_prefix}

%description
Provide a KDE client for SFLphone.
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
cmake . -DCMAKE_INSTALL_PREFIX=%{buildroot}/%{_prefix}
make

%install
make install

%clean
make clean

%files
%defattr(-, root, root)
%{_prefix}/*
%doc AUTHORS COPYING README

%changelog
