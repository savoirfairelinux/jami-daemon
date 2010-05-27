#####################################################
# File Name: sflphone-client-kde.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-10-07
#####################################################

Name:           sflphone-client-kde
License:        GNU General Public License (GPL)
Group:          Productivity/Networking/System
Summary:        KDE client for SFLphone
Version:        VERSION
Release:        VERSION_INDEX%{?dist}
URL:            http://www.sflphone.org/
Vendor:		Savoir-faire Linux
Packager:       Julien Bonjean <julien.bonjean@savoirfairelinux.com>

BuildRoot:      %{_tmppath}/%{name}
Source0:        sflphone-client-kde-%{version}.tar.gz

Requires:	sflphone-common = %{version}
Requires:	commoncpp2
Requires:	libkdepimlibs4
Requires:	libqt4-dbus-1
Requires:	libqt4-svg
Requires:	libqt4-x11

%if %{defined suse_version}
BuildRequires:  update-desktop-files
BuildRequires:  libkdepimlibs4-devel
BuildRequires:  libqt4-devel >= 4.3
BuildRequires:	gettext-tools
%endif

%if %{defined fedora_version}
BuildRequires:	gcc-c++
BuildRequires:	kdepimlibs-devel
BuildRequires:	qt4
BuildRequires:	gettext
%endif

BuildRequires:  cmake
BuildRequires:  commoncpp2-devel

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
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make

%install
make DESTDIR=%{buildroot} install
mkdir -p %{buildroot}/%{_prefix}/share/pixmaps
cp src/icons/sflphone.svg %{buildroot}/%{_prefix}/share/pixmaps/sflphone.svg
%suse_update_desktop_file -n %{buildroot}/%{_prefix}/share/applications/kde4/sflphone-client-kde.desktop

%clean
make clean

%files
%defattr(-, root, root)
%dir %{_prefix}/share/doc/kde/HTML/en/sflphone-client-kde
%dir %{_prefix}/share/kde4/apps/sflphone-client-kde 
%lang(fr) %{_prefix}/share/locale/fr/LC_MESSAGES/*.mo
%lang(es) %{_prefix}/share/locale/es/LC_MESSAGES/*.mo
%lang(de) %{_prefix}/share/locale/de/LC_MESSAGES/*.mo
%lang(ru) %{_prefix}/share/locale/ru/LC_MESSAGES/*.mo
%lang(zh_CN) %{_prefix}/share/locale/zh_CN/LC_MESSAGES/*.mo
%lang(zh_HK) %{_prefix}/share/locale/zh_HK/LC_MESSAGES/*.mo
%doc AUTHORS COPYING README
%doc %{_prefix}/share/man/man1/sflphone-client-kde.1.gz
%doc %{_prefix}/share/doc/kde/HTML/en/sflphone-client-kde/*
%{_prefix}/share/kde4/apps/sflphone-client-kde/*
%{_prefix}/share/kde4/config.kcfg/sflphone-client-kde.kcfg
%{_prefix}/bin/sflphone-client-kde
%{_prefix}/share/applications/kde4/sflphone-client-kde.desktop
%{_prefix}/share/pixmaps/sflphone.svg
%{_prefix}/share/icons/hicolor/128x128/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/16x16/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/22x22/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/32x32/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/48x48/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/64x64/apps/sflphone-client-kde.png
%{_prefix}/share/icons/hicolor/scalable/apps/sflphone-client-kde.svgz

%changelog
