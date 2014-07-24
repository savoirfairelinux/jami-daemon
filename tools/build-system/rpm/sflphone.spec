%bcond_with video
Name:           sflphone
Version:        1.4.1
%if 0%{?nightly}
%define rel rc%{nightly}
%define tarball %{name}-%{version}-rc%{nightly}
%else
%define rel 1
%define tarball %{name}-%{version}
%endif
Release:        %{rel}%{?dist}
Summary:        SIP/IAX2 compatible enterprise-class software phone
Group:          Applications/Internet
License:        GPLv3
URL:            http://sflphone.org/
Source0:        https://projects.savoirfairelinux.com/attachments/download/6423/%{tarball}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:      gettext openssl-devel desktop-file-utils perl
BuildRequires:      libyaml-devel alsa-lib-devel pulseaudio-libs-devel
BuildRequires:      ccrtp-devel libzrtpcpp-devel dbus-c++-devel pcre-devel
BuildRequires:      gsm-devel speex-devel expat-devel libsamplerate-devel
BuildRequires:      gnome-doc-utils libtool libsexy-devel intltool yelp-tools
BuildRequires:      libnotify-devel check-devel rarian-compat ilbc-devel
BuildRequires:      evolution-data-server-devel gnome-common libsndfile-devel
# KDE requires
BuildRequires:      cmake kdepimlibs-devel
BuildRequires:      perl-podlators
%if %{with video} && 0%{?fedora} < 18
BuildRequires:      libudev-devel
%endif
%if %{with video} && 0%{?fedora} >= 18
BuildRequires:      systemd-devel
%endif

%description
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

%prep
%setup -q -n %{tarball}

%build
# Compile the daemon
pushd daemon
./autogen.sh
# Compile pjproject first
pushd libs
./compile_pjsip.sh
popd
# Compile daemon
%if %{with video}
%configure --enable-video
%else
%configure
%endif
make %{?_smp_mflags}
make doc
popd
pushd plugins
./autogen.sh
%configure
make %{?_smp_mflags}
popd
# Compile kde client (only without video)
pushd kde
sed -i '/^[^#]add_subdirectory.*test/s/^[^#]/#/' src/CMakeLists.txt
./config.sh --prefix=%{_prefix}
cd build
make %{?_smp_mflags}
popd
# Compile gnome client
pushd gnome
./autogen.sh
%if %{with video}
%configure --enable-video
%else
%configure
%endif
make %{?_smp_mflags}
popd


%if %{with video}
%package gnome-video
Summary:        SIP/IAX2 compatible enterprise-class software phone
Group:          Applications/Internet
Requires:       %{name}-common-video
Conflicts:      sflphone-gnome sflphone
BuildRequires:  ffmpeg-devel clutter-gtk-devel
%description gnome-video
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the Gnome client with videoconferencing ability

%package common-video
Summary:        SIP/IAX2 compatible enterprise-class software phone
Group:          Applications/Internet
Conflicts: sflphone sflphone-daemon sflphone-common
%description common-video
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the SFLPhone daemon with videoconferencing enabled
%else
%package common
Summary:        SIP/IAX2 compatible enterprise-class software phone
Group:          Applications/Internet
Conflicts: sflphone sflphone-daemon-video
%description common
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the SFLPhone common

%package gnome
Summary:        Gnome interface for SFLphone
Group:          Applications/Internet
%if %{with video}
Requires:       %{name}-common-video = %{version}
%else
Requires:       %{name}-common = %{version}
%endif
Obsoletes:      sflphone < 1.2.2-2
Conflicts:      sflphone-video
%description gnome
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the Gnome client

%endif

%package kde-video
Summary:        KDE interface for SFLphone
Group:          Applications/Internet
%if %{with video}
Requires:       %{name}-common-video = %{version}
%else
Requires:       %{name}-common = %{version}
%endif
%description kde-video
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the KDE client

%package plugins
Summary:        Plugins (address book) for SFLphone
Group:          Applications/Internet
%if %{with video}
Requires:       %{name}-common-video = %{version}
%else
Requires:       %{name}-common = %{version}
%endif
%description plugins
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

This package includes the address book plugin.

%install
rm -rf %{buildroot}
pushd daemon
make install DESTDIR=$RPM_BUILD_ROOT
popd
# Gnome install
pushd gnome
make install DESTDIR=$RPM_BUILD_ROOT
# Find Lang files
popd
# Plugins install
pushd plugins
make install DESTDIR=$RPM_BUILD_ROOT
popd
%find_lang sflphone --with-gnome
# Handling desktop file
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop
# KDE install
pushd kde/build
make install DESTDIR=$RPM_BUILD_ROOT
popd
%find_lang sflphone-client-kde --with-kde -f sflphone-client-kde
%find_lang sflphone-kde --with-kde -f sflphone-kde

%if %{with video}
%pre gnome-video
if [ "$1" -gt 1 ] ; then
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null
fi

%post gnome-video
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null

%preun gnome-video
if [ "$1" -eq 0 ] ; then
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null
fi
%else
%pre gnome
if [ "$1" -gt 1 ] ; then
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null
fi

%post gnome
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null

%preun gnome
if [ "$1" -eq 0 ] ; then
    glib-compile-schemas %{_datadir}/glib-2.0/schemas &>/dev/null
fi
%endif

%post kde-video -p /usr/sbin/ldconfig
%postun kde-video -p /usr/sbin/ldconfig

%if %{with video}
%files common-video
%else
%files common
%endif
%defattr(-,root,root,-)
%doc daemon/AUTHORS COPYING NEWS README
%{_libdir}/%{name}/*
%{_datadir}/dbus-1/services/org.%{name}.SFLphone.service
%{_mandir}/man1/sflphoned.1.gz*
%{_datadir}/pixmaps/%{name}.svg
%{_datadir}/%{name}/*

%if %{with video}
%files -f sflphone.lang gnome-video
%else
%files -f sflphone.lang gnome
%endif
%defattr(-,root,root,-)
%{_bindir}/sflphone
%{_bindir}/sflphone-client-gnome
%{_datadir}/glib-2.0/schemas/org.sflphone.SFLphone.gschema.xml
%{_datadir}/applications/%{name}.desktop
%{_mandir}/man1/sflphone.1.gz
%{_mandir}/man1/sflphone-client-gnome.1.gz
%{_datadir}/pixmaps/%{name}.svg
%{_datadir}/%{name}/*

%files plugins
%{_libdir}/sflphone/plugins/libevladdrbook.so

%files kde-video -f sflphone-kde -f sflphone-client-kde
%{_bindir}/sflphone-client-kde
%{_datadir}/kde4/apps/sflphone-client-kde
%{_datadir}/config.kcfg/sflphone-client-kde.kcfg
%{_datadir}/applications/kde4
%doc %{_mandir}/man1/*kde*
%{_datadir}/icons/hicolor
%{_libdir}/libksflphone.so*
%{_libdir}/libqtsflphone.so*
%exclude %{_includedir}/kde4/ksflphone/*.h
%exclude %{_includedir}/qtsflphone/*.h

%changelog
* Wed Jul 23 2014 Simon Piette <simon.piette@savoirfairelinux.com> - 1.4.1-2
- Always build kde package

* Tue Jul 15 2014 Tristan Matthews <tristan.matthews@savoirfairelinux.com> - 1.4.1-1
- Start development of 1.4.1

* Tue Jul 15 2014 Tristan Matthews <tristan.matthews@savoirfairelinux.com> - 1.4.0-1
- Update to 1.4.0

* Wed Jul 9 2014 Tristan Matthews <tristan.matthews@savoirfairelinux.com> - 1.3.0-5
- Drop uuid dependency

* Mon Jul 07 2014 Simon Piette <simon.piette@savoirfairelinux.com> - 1.3.0-n
- Support both nightly and release

* Thu May 15 2014 Simon Piette <simon.piette@savoirfairelinux.com> - 1.3.0-rc%{nightly}
- Adapt for nightly builds

* Tue Jan 21 2014 Simon Piette <simon.piette@savoirfairelinux.com> - 1.3.0-2
- Fix "Fix KDE paths"

* Mon Jan 13 2014 Tristan Matthews <tristan.matthews@savoirfairelinux.com> - 1.3.0-1
- Update to 1.3.0
- Fix KDE paths (tested on f20)
- Added libuuid dependency for pjsip

* Wed Jun 19 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.3-1
- Update to 1.2.3
- Enable ilbc

* Mon Feb 18 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-6
- Add sflphone-plugins

* Mon Feb 18 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-5
- Renamed daemon to config

* Mon Feb 18 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-4
- Video variant for gnome

* Wed Feb 13 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-3
- split daemon and gnome packages

* Wed Feb 13 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-2
- creates a kde client package

* Tue Jan 15 2013 Simon Piette <simonp@fedoraproject.org> - 1.2.2-1
- upgraded to 1.2.2
- updated BuildRequires
- disabled ilbc
- replaced gconf with gsettings

* Tue Sep 11 2012 Simon Piette <simonp@fedoraproject.org> - 1.2.0-1
- upgraded to 1.2.0 (tested on f16)
- updated BuildRequires

* Wed Apr 20 2011 Prabin Kumar Datta <prabindatta@fedoraproject.org> - 0.9.13-1
- avoiding compling with Celt codec support to resolve build problem
- removed clean section since not required
- upgraded to 0.9.13

* Mon Apr 18 2011 Prabin Kumar Datta <prabindatta@fedoraproject.org> - 0.9.12-2
- Fixed schema registration problem

* Fri Mar 25 2011 Prabin Kumar Datta <prabindatta@fedoraproject.org> - 0.9.12-1
- Initial build
