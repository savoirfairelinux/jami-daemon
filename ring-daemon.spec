%define name        ring-daemon
%define version     2.3.0
%define release     1
%define daemon_tag  origin/master

Name:               %{name}
Version:            %{version}
Release:            %{release}%{?dist}
Summary:            Free software for distributed and secured communication.
Group:              Applications/Internet
License:            GPLv3
URL:                http://ring.cx/
Source:             daemon
BuildRequires:      make autoconf automake cmake pulseaudio-libs-devel
BuildRequires:      libsamplerate-devel libtool dbus-devel expat-devel
BuildRequires:      pcre-devel yaml-cpp-devel boost-devel dbus-c++-devel
BuildRequires:      dbus-devel libsndfile-devel libXext-devel libXfixes-devel
BuildRequires:      yasm git speex-devel chrpath check astyle uuid-c++-devel
BuildRequires:      gettext-devel gcc-c++ which alsa-lib-devel systemd-devel
BuildRequires:      libuuid-devel uuid-devel gnutls-devel nettle-devel
BuildRequires:      opus-devel jsoncpp-devel

%description
Ring is free software for distributed and secured communication.
This is the daemon.

%package devel
Summary:        Free software for distributed and secured communication.
Group:          Applications/Internet

%description devel
Ring is free software for distributed and secured communication.
This is the daemon headers.

%prep
%setup -q
git init
git remote add origin https://gerrit-ring.savoirfairelinux.com/ring-daemon
git fetch --all
git checkout %{daemon_tag} -f
git config user.name "joulupukki"
git config user.email "joulupukki@localhost"
git merge origin/packaging --no-commit
git reset HEAD
# Apply all patches
for patch_file in $(cat debian/patches/series | grep -v "^#")
do
%{__patch} -p1 < debian/patches/$patch_file
done


%build
rm -rf %{buildroot}
mkdir -p contrib/native
cd contrib/native
../bootstrap --disable-ogg --disable-flac --disable-vorbis --disable-vorbisenc --disable-speex --disable-sndfile --disable-speexdsp
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
echo '[ring]' > %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo
echo 'name=Ring - $basearch - Nightly' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo
echo 'baseurl=http://nightly.yum.ring.cx/fedora_$releasever' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo
echo 'gpgcheck=1' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo
echo 'gpgkey=http://gpl.savoirfairelinux.net/ring-download/ring.pub.key' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo
echo 'enabled=1' >> %{buildroot}/%{_sysconfdir}/yum.repos.d/ring-nightly.repo


%files
%defattr(-,root,root,-)
%{_libdir}/ring/dring
%{_datadir}/ring/ringtones
%{_datadir}/dbus-1/services/*
%{_datadir}/dbus-1/interfaces/*
%doc %{_mandir}/man1/dring*
%config %{_sysconfdir}/yum.repos.d/ring-nightly.repo

%files devel
%defattr(-,root,root,-)
%{_libdir}/libring.la
%{_libdir}/libring.a
/usr/include/dring/

%changelog
* Fri May  1 2015 Guillaume Roguez <guillaume.roguez@savoirfairelinux.com> - 2.3.0-1
- New upstream version

* Tue Apr 14 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.1.0-1
- New upstream version

* Fri Mar 27 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 2.0.1-1
- New upstream version
