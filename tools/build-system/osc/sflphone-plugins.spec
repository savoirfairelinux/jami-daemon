#####################################################
# File Name: sflphone-client-gnome.spec
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-05-27
# Last Modified: 2009-10-07
#####################################################

Name:           sflphone-plugins
License:        GNU General Public License (GPL)
Group:          Productivity/Networking/System
Summary:        Evolution addressbook plugin for SFLphone
Version:        VERSION
Release:        VERSION_INDEX%{?dist}
URL:            http://www.sflphone.org/
Vendor:		Savoir-faire Linux
Packager:       Julien Bonjean <julien.bonjean@savoirfairelinux.com>

Group:          Applications/Communications
BuildRoot:      %{_tmppath}/%{name}
Source0:        sflphone-plugins-%{version}.tar.gz

%if %{defined suse_version}
BuildRequires:	libgnomeui-devel
%endif

%if %{defined fedora_version}
BuildRequires:	libgnomeui-devel
%endif

%if %{defined mandriva_version}
BuildRequires:	libgnomeui2-devel
%endif

BuildRequires:	gtk2-devel
BuildRequires:	evolution-data-server-devel

Requires:	gtk2
Requires:	glib2

Conflicts:	sflphone
Prefix:		%{_prefix}

%description
Provide Evolution addressbok functionality for SFLphone client gnome.
 SFLphone is meant to be a robust enterprise-class desktop phone.
 SFLphone is released under the GNU General Public License.
 SFLphone is being developed by the global community, and maintained by
 Savoir-faire Linux, a Montreal, Quebec, Canada-based Linux consulting company.

Authors:
--------
    Alexandre Savard <alexandre.savard@savoirfairelinux.com>

%lang_package

%prep
%setup -q


%build
export SUSE_ASNEEDED=0 # fix opensuse linking issue (Since 11.2 uses default --as-needed for linking, the order of libraries is important)
./autogen.sh
./configure --prefix=%{_prefix}
make -j


%install
make DESTDIR=%{buildroot} install
%if %{defined suse_version}
%suse_update_desktop_file -n %{buildroot}/%{_prefix}/share/applications/sflphone.desktop
%endif
rm -rf $RPM_BUILD_ROOT/var/lib/scrollkeeper

%clean
make clean

%files
%defattr(-, root, root)
%dir %{_prefix}/etc/gconf/
%dir %{_prefix}/etc/gconf/schemas/

%dir %{_libdir}/sflphone
%dir %{_libdir}/sflphone/plugins

%{_libdir}/sflphone/plugins/*

%{_prefix}/etc/gconf/schemas/sflphone-client-gnome.schemas

%changelog

