#!/bin/sh

# @author: Emmanuel Milou - emmanuel.milou@savoirfairelinux.com
# Build a binary debian package of sflphone
# Pre requisite: make install of the all repository

if [ $1 = "-h" ]
then
  echo "Usage: ./autopackage.sh version arch"
  echo "For instance: ./autopackage.sh 0.8.2 all"
  exit 0
fi

# Libraries dependencies
dependencies="libgcc1 , libsamplerate0 (>=0.1.2) , libdbus-glib-1-2 (>= 0.73), libexpat1 , libgtk2.0-0 , libc6 (>= 2.3.6-6) , libglib2.0-0 (>= 2.12.0) , libosip2-2, libexosip2-4, libcommoncpp2-1.6-0 , libccrtp1-1.6-0  , sflphone-iax2 , libgsm1 (>=1.0.10) , libspeex1 (>=1.1.12) , dbus-c++-1 (>=0.5.0)"

# Package Infos
package="sflphone"
version="$1"
section="gnome"
priority="optional"
essential="no"
#size="1945"
arch="$2"
homepage="http://www.sflphone.org"
maintainer="SavoirFaireLinux Inc <emmanuel.milou@savoirfairelinux.com>"
desc="SFLphone - SIP and IAX2 compatible softphone\n SFLphone is meant to be a robust enterprise-class desktop phone. It is design with a hundred-calls-a-day receptionist in mind. It can work for you, too.\n .\n SFLphone is released under the GNU General Public License.\n .\n SFLphone is being developed by the global community, and maintained by Savoir-faire Linux, a Montreal, Quebec, Canada-based Linux consulting company."

# Get the needed stuff

echo "Gathering libraries, binaries and data ..."

sfldir="sflphone_$1"
bindir="/usr/bin"
libdir="/usr/lib"
sharedir="/usr/share"
debdir="$sfldir/DEBIAN"

#/usr/bin
mkdir -p $sfldir$bindir
cp $bindir/sflphoned $sfldir$bindir
cp $bindir/sflphone-gtk $sfldir$bindir
ln -sf $bindir/sflphone-gtk $sfldir$bindir/sflphone

#/usr/lib
mkdir -p $sfldir/usr/lib/sflphone/codecs

#/usr/lib/sflphone/codecs
cp $libdir/sflphone/codecs/libcodec_*	$sfldir$libdir/sflphone/codecs/

#/usr/share/applications
mkdir -p $sfldir$sharedir/applications
cp $sharedir/applications/sflphone.desktop $sfldir$sharedir/applications/
#/usr/share/dbus-1/services
mkdir -p $sfldir$sharedir/dbus-1/services
cp $sharedir/dbus-1/services/org.sflphone.SFLphone.service $sfldir$sharedir/dbus-1/services/
/usr/share/pixmaps
mkdir -p $sfldir$sharedir/pixmaps
cp $sharedir/pixmaps/sflphone.png $sfldir$sharedir/pixmaps
#/usr/share/sflphone
mkdir -p $sfldir$sharedir/sflphone/ringtones
cp $sharedir/sflphone/* $sfldir$sharedir/sflphone
#/usr/share/sflphone/ringtones
cp $sharedir/sflphone/ringtones/* $sfldir$sharedir/sflphone/ringtones
#/usr/share/locale/fr/LC_MESSAGES
mkdir -p $sfldir$sharedir/locale/fr/LC_MESSAGES
cp $sharedir/locale/fr/LC_MESSAGES/sflphone.mo	$sfldir$sharedir/locale/fr/LC_MESSAGES
mkdir -p $sfldir$sharedir/locale/es/LC_MESSAGES
cp $sharedir/locale/es/LC_MESSAGES/sflphone.mo	$sfldir$sharedir/locale/es/LC_MESSAGES
#/usr/share/doc/sflphone
mkdir -p $sfldir$sharedir/doc/sflphone
cp changelog.Debian.gz $sfldir$sharedir/doc/sflphone
cp copyright $sfldir$sharedir/doc/sflphone
cp TODO $sfldir$sharedir/doc/sflphone

# DEBIAN files
mkdir -p $debdir 
# Create control file
control="$debdir/control"
touch $control
echo "Package: $package" > $control
echo "Version: $version" >> $control
echo "Section: $section" >> $control
#echo "Installed-Size: $size" >> $control
echo "Priority: $priority" >> $control
echo "Architecture: $arch" >> $control
echo "Essential: $essential" >> $control
echo "Depends: $dependencies" >> $control
echo "Homepage: $homepage" >> $control
echo "Maintainer: $maintainer" >> $control
echo "Description: $desc" >> $control

# Create the debian package
echo "Build the debian package ... "
dpkg --build $sfldir ${sfldir}_$2.deb

# Clean up the generated stuff
echo "Clean up ... "
rm -rf $sfldir 
