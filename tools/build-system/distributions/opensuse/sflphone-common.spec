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

%define name sflphone-common
%define version VERSION
%define release 1suse

Name:           %name
License:        GNU General Public License (GPL)
Group:          System Environment/Daemons
Summary:        A VoIP daemon with SIP protocol and IAX protocol
Version:        %version
Release:        %release
URL:            http://www.sflphone.org/
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source0:        sflphone-common.tar.gz
Requires:	commoncpp2 libccrtp1 libsamplerate pulseaudio libgsm1 libspeex  dbus-1-glib expat gtk2 glibc glib2 dbus-1 libsexy libnotify perl
Prefix: %{_prefix}

%description
SFLphoned is a VoIP daemon with SIP protocol and IAX protocol.

Authors:
--------
    Julien Bonjean <julien.bonjean@savoirfairelinux.com>

%prep
%setup -q

%build
cd libs/pjproject-1.0.1
./autogen.sh --prefix=%{_prefix}
make dep
make clean
make
cd -
./autogen.sh --prefix=%{_prefix}
make -j

%install
cd libs/pjproject-1.0.1
make prefix=%{buildroot}/%{_prefix} install
cd -
make prefix=%{buildroot}/%{_prefix} install

%clean
cd libs/pjproject-1.0.1
make clean
cd -
make clean

%files
%defattr(-, root, root)
%{_prefix}/*
%exclude %{_prefix}/include
%doc AUTHORS COPYING README TODO

%changelog
