#####################################################
# File Name: sflphone-common.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-05-27 17:23:32 -0400
#####################################################

Name:           sflphone-common
License:        GNU General Public License (GPL)
Group:          System Environment/Daemons
Summary:        SIP and IAX2 compatible softphone - Core
Version:        VERSION
Release:        mandriva
URL:            http://www.sflphone.org/
Vendor:         Savoir-faire Linux
Packager:	Julien Bonjean <julien.bonjean@savoirfairelinux.com>

BuildRoot:      %{_tmppath}/%{name}-%{version}
Source0:        sflphone-common.tar.gz
BuildRequires:	glibc-devel
BuildRequires:	libccrtp-devel
BuildRequires:	libsamplerate-devel
BuildRequires:	libdbus-1-devel
BuildRequires:	libexpat-devel
BuildRequires:	libgsm-devel
BuildRequires:	speex-devel
BuildRequires:	libcppunit-devel
BuildRequires:	libcommoncpp-devel
BuildRequires:	libalsa2-devel
BuildRequires:	libpulseaudio-devel
BuildRequires:	libext2fs-devel
Requires:	libsamplerate
Requires:	libexpat1
Requires:	commoncpp2
Requires:	libgsm1
Requires:	libspeex
Requires:	dbus-1
Requires:	libasound2
Requires:	libpulse0
Requires:	libccrtp1
Conflicts:      sflphone
Prefix:		%{_prefix}

%description
SFLphone is meant to be a robust enterprise-class desktop phone.
 SFLphone is released under the GNU General Public License.
 SFLphone is being developed by the global community, and maintained by
 Savoir-faire Linux, a Montreal, Quebec, Canada-based Linux consulting company.

Authors:
--------
    Julien Bonjean <julien.bonjean@savoirfairelinux.com>

%prep
%setup -q

%build
cd libs/pjproject-1.0.3
./autogen.sh
./configure --prefix=%{_prefix}
make dep
make clean
make
cd -
./autogen.sh
./configure --prefix=%{_prefix}
make -j

%install
cd libs/pjproject-1.0.3
make DESTDIR=%{buildroot} install
cd -
make DESTDIR=%{buildroot} install

%clean
cd libs/pjproject-1.0.3
make clean
cd -
make clean

%files
%defattr(-, root, root)
%{_prefix}/*
%exclude %{_prefix}/include
%doc AUTHORS COPYING README TODO

%changelog
