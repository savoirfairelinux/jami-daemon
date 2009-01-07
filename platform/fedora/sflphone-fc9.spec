%define name sflphone
%define version 0.9.2
Autoreq: 0

Name:		%name
Summary:	A VoIP daemon with SIP protocol and IAX protocol
Version:	%version
Release:        2
License:	GPL
Group:		System Environment/Daemons
URL:		http://www.sflphone.org/
Packager:	Yun Liu <yun.liu@savoirfairelinux.com>
Source0:	%{name}-%{version}.tar.gz
Source1:        libpj-sfl.pc
BuildRoot:	%{_tmppath}/%{name}-%{version}
Requires: 	commoncpp2 ccrtp cppunit libsamplerate pulseaudio-libs-zeroconf pulseaudio-libs-devel gsm speex sflphone-iax2 dbus-c++ libgcc dbus-glib expat gtk2 glibc glib2 dbus-libs dbus-glib libsexy libnotify perl

%description
SFLPhoned is a VoIP daeamon with SIP protocol and IAX protocol.

%prep
%setup -q
cd libs/pjproject-1.0
./configure --prefix=/usr
make dep
make clean
make
cd ../..

%build
./autogen.sh
./configure --prefix=/usr
make
cd sflphone-gtk/
./autogen.sh
./configure --prefix=/usr
make
cd ..

%install
%makeinstall

cd sflphone-gtk/
%makeinstall

cd ../libs/pjproject-1.0
%makeinstall
cd ../..
cd %{buildroot}/usr/bin/
ln -sf ./sflphone-gtk sflphone 
cd -
cp %{SOURCE1} %{buildroot}/usr/lib/pkgconfig/ -f

rm -f  %{buildroot}/usr/include/Makefile.*
rm -rf  %{buildroot}/usr/lib/debug

%files
%defattr(-, root, root)
/usr/bin/*
/usr/include/*
/usr/lib/*
/usr/share/applications/*
/usr/share/dbus-1/services/*
/usr/share/locale/*
/usr/share/pixmaps/*
/usr/share/sflphone/*
/usr/share/man/*

%clean
rm -rf %{buildroot}



%changelog
* Mon Jan 5 2009 Yun Liu <yun.liu@savoirfairelinux.com>
  - Fix bug ticket #107, #108, #109, #110, #111, #117, #129

* Thu Nov 6 2008 Yun Liu <yun.liu@savoirfairelinux.com>
  - Packaging sflphone for Fedora 9
  
* Thu Nov 30 2006 Yan Morin <yan.morin@savoirfairelinux.com>
  Packaging sflphone for Fedora Core 6

* Wed Sep  6 2006 Yan Morin <yan.morin@savoirfairelinux.com>
  Packaging sflphone, sflphone-qt and sflphone-cli

* Mon Nov 21 2005 Yan Morin <yan.morin@savoirfairelinux.com>
- Final 0.6.0 version (Release / Source without alphatag)

