%define name        ring-daemon
%define version     2.0.1
%define release     1

Name:               %{name}
Version:            %{version}
Release:            %{release}%{?dist}
Summary:            Secured and distributed communication software
Group:              Applications/Internet
License:            GPLv3
URL:                http://ring.cx/
Source:             /var/lib/joulupukki/daemon
BuildRequires:      autoconf automake pulseaudio-libs-devel libsamplerate-devel
BuildRequires:      libtool dbus-devel expat-devel pcre-devel yaml-cpp-devel
BuildRequires:      boost-devel dbus-c++-devel dbus-devel libsndfile-devel libsrtp-devel
BuildRequires:      libXext-devel libXfixes-devel yasm git speex-devel
BuildRequires:      chrpath check astyle uuid-c++-devel gnutls-devel 
BuildRequires:      gcc-c++ which alsa-lib-devel systemd-devel libuuid-devel uuid-devel

%description
Ring is a secured and distributed communication software.
This is the daemon.

%package devel
Summary:        Secured and distributed communication software
Group:          Applications/Internet

%description devel
Ring is a secured and distributed communication software.
This is the daemon headers.


%prep
%setup -q

%build
rm -rf %{buildroot}
mkdir -p contrib/native
cd contrib/native
../bootstrap --disable-ogg --disable-flac --disable-vorbis --disable-vorbisenc --disable-speex --disable-sndfile --disable-speexdsp --disable-gnutls
make list
make -j2
cd ../..
echo "Contribs built"
./autogen.sh
%configure --prefix=/usr CFLAGS="$(CFLAGS) -fPIC" LDFLAGS="-Wl,-z,defs"
make -j %{?_smp_mflags}

%install
mkdir -p %{buildroot}/ring-daemon
make DESTDIR=%{buildroot} install

%files
%defattr(-,root,root,-)
%{_libexecdir}/dring
%{_datadir}/ring/ringtones
%{_datadir}/dbus-1/services/cx.ring.Ring.service
%doc %{_mandir}/man1/dring*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libring.la
%{_libdir}/libring.a
/usr/include/dring/

%changelog
* Fri Mar 27 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.0.1-1
- New upstream version
