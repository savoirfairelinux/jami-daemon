%define name        ring-daemon
%define version     2.1.0
%define release     2
%define daemon_tag  %{version}

Name:               %{name}
Version:            %{version}
Release:            %{release}%{?dist}
Summary:            Free software for distributed and secured communication.
Group:              Applications/Internet
License:            GPLv3
URL:                http://ring.cx/
Source:             daemon
BuildRequires:      autoconf automake pulseaudio-libs-devel libsamplerate-devel
BuildRequires:      libtool dbus-devel expat-devel pcre-devel yaml-cpp-devel
BuildRequires:      boost-devel dbus-c++-devel dbus-devel libsndfile-devel libsrtp-devel
BuildRequires:      libXext-devel libXfixes-devel yasm git speex-devel
BuildRequires:      chrpath check astyle uuid-c++-devel gnutls-devel
BuildRequires:      gcc-c++ which alsa-lib-devel systemd-devel libuuid-devel uuid-devel

%description
Ring is free software for distributed and secured communication.
This is the daemon.

%package devel
Summary:        Free software for distributed and secured communication.
Group:          Applications/Internet

%description devel
Ring is free software for distributed and secured communication.
This is the daemon headers.

%package repository
Summary:        Free software for distributed and secured communication.
Group:          Applications/Internet

%description repository
Ring is free software for distributed and secured communication.
This is the ring repository

%prep
%setup -q
git init
git remote add origin https://gerrit-ring.savoirfairelinux.com/ring-daemon
git fetch --all
git checkout packaging-releases -f
git config user.name "joulupukki"
git config user.email "joulupukki@localhost"
git merge %{daemon_tag} --no-edit
rm -rf .git
# Apply all patches
for patch_file in $(cat debian/patches/series | grep -v "^#")
do
%{__patch} -p1 < debian/patches/$patch_file
done


%build
rm -rf %{buildroot}
mkdir -p contrib/native
cd contrib/native
../bootstrap --disable-ogg --disable-flac --disable-vorbis --disable-vorbisenc --disable-speex --disable-sndfile --disable-speexdsp --disable-gnutls
make list
make
cd ../..
echo "Contribs built"
./autogen.sh
%configure --prefix=/usr CFLAGS="$(CFLAGS) -fPIC" LDFLAGS="-Wl,-z,defs"
make -j %{?_smp_mflags}

%install
mkdir -p %{buildroot}/ring-daemon
make DESTDIR=%{buildroot} install
mkdir -p %{buildroot}/%{_sysconfdir}/yum.repos.d/
echo '[ring]' > %{buildroot}/%{_sysconfdir}/yum.repos.d/ring.repo
echo 'name=Fedora $releasever - $basearch - ring' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring.repo
echo 'baseurl=http://yum.ring.cx/fedora_$releasever' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring.repo
echo 'enabled=1' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring.repo
echo 'gpgcheck=0' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring.repo


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

%files repository
%config %{_sysconfdir}/yum.repos.d/ring-nightly.repo

%changelog
* Fri Apr 24 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.1.0-2
- Fix repository url

* Tue Apr 14 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.1.0-1
- New upstream version

* Fri Mar 27 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.0.1-1
- New upstream version
