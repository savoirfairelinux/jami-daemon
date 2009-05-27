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

%define name sflphone-client-gnome
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
Source0:        sflphone-client-gnome.tar.gz
Requires:	commoncpp2 libccrtp1 libsamplerate pulseaudio libgsm1 libspeex  dbus-1-glib expat gtk2 glibc glib2 dbus-1 libsexy libnotify perl
Prefix: %{_prefix}

%description
SFLphoned is a VoIP client with SIP protocol and IAX protocol.

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
