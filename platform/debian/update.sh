#!/bin/sh

# @author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
# Update the existing debian package wit the updated installation script
# Use it if you modify the installation scripts sflphone.preinst, sflphone.postrm or sflphone.prerm

# you need the text base files: package and arch. They have just one line with respectively the name of the package and the architecture

base_file="package arch"

for i in $base_file
do
  if [ ! -f $i ]
    then
    echo " Could not find the file $i "
    exit 0
  fi
done

package=`(cat package)`
arch=`(cat arch)`

if [ ! -d $package ]
then
  exit 0
fi

# Update installation scripts
cp sflphone.preinst $package/DEBIAN/preinst
cp sflphone.postrm $package/DEBIAN/postrm
cp sflphone.prerm $package/DEBIAN/prerm

# Rebuild the debian package
dpkg --build $package ${package}_${arch}.deb 2> /dev/null

echo "Done!"
