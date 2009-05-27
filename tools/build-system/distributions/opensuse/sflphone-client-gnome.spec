%define name sflphone-client-gnome
%define version 0.9.8
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

%prep
%setup -q

%build
./autogen.sh --prefix=%{_prefix}
make

%install
make prefix=%{buildroot}/%{_prefix} install

%clean
make clean

%files
%defattr(-, root, root)
%{_prefix}/*
%doc AUTHORS COPYING README TODO

%changelog
* Mon Dec 04 2006 - my@mail.de
- initial package

