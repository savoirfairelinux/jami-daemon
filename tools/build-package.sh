#!/bin/bash
#
# @author: Yun Liu <yun.liu@savoirfairelinux.com>
#
# Build sflphone rpm packages for Fedora 10 and openSUSE 11
# 1 - The SFLphone package must be build with a specific GnuPG key. Please contact us to have more information about that (<sflphoneteam@savoirfairelinux.com>)
# 2. The source code can be teched through anonymous http access. So no need of special access.
# 3. After having all the prerequisites, you can run  "build-rpm-package.sh" to build rpm packages for sflphone.
#
# Refer to http://www.sflphone.org for futher information

# Analyze parameters
if [ "$1" == "--help" ] || [ "$1" == "" ];then
        echo -e '\E[34mThis script is used to build sflphone rpm packages on ubuntu series(8.04,8,10,9), Fedora 10 and SUSE 11 platform.'
        echo -e '\E[34mYou can add --fedora, --suse or --ubuntu to start packaging.'
	echo
	echo "The SFLphone package must be build with a specific GnuPG key. Please contact us to have more information about that (<sflphoneteam@savoirfairelinux.com>)"
	echo
	echo "For fedora and SUSE, you also need to add the following lines to $HOME/.rpmmacros:"
	echo -e '\E[32m%_gpg_path /home/yun/.gnupg'
	echo -e '\E[32m%_gpg_name Savoir-Faire Linux Inc. (Génération des paquets pour SFLphone) <sflphoneteam@savoirfairelinux.com>'
	echo -e '\E[32m%_gpgbin /usr/bin/gpg'
	echo
	echo -e '\E[34mAfter all these preparations done, you can run ./build-package.sh --platform-name'
	echo
	echo -e '\E[36mHave fun!'

	tput sgr0                               # Reset colors to "normal."
	echo
	exit 1
elif [ $1 == "--fedora" ];then
        BUILDDIR=$HOME/rpmbuild
	platform="fedora"
elif [ $1 == "--suse" ];then
        BUILDDIR=/usr/src/packages
	platform="suse"
elif [ $1 == "--ubuntu" ];then
	platform="ubuntu"
else
	echo "This script can only be used for Ubuntu series, Fedora 10 and SUSE 11 platform. Use --help to get more information."
	exit 1
fi

if [ -d "sflphone" ]; then
        echo "Directory sflphone already exists. Please remove it first."
        exit 1
fi

# Anonymous git http access
git clone http://sflphone.org/git/sflphone.git
cd sflphone
git checkout origin/release -b release

# Get system parameters
arch_flag=`getconf -a|grep LONG_BIT | sed -e 's/LONG_BIT\s*//'`
os_version=`lsb_release -d -s -c | sed -e '1d'`
ver=0.9.5

if [ $platform == "ubuntu" ];then
	# Generate the changelog, according to the distribution and the git commit messages
    sed 's/%system%/'$os_version'/g' debian/changelog > debian/changelog.tmp && mv debian/changelog.tmp debian/changelog
fi

cd ..

# Remove useless git directory
rm sflphone/.git/ -rf

# Get the public gpg key to sign the packages
wget -q http://www.sflphone.org/downloads/gpg/sflphone.gpg.asc -O- | gpg --import -

if [ $platform == "ubuntu" ];then
	# Copy the appropriate control file based on different archtecture
	cp sflphone/debian/control.$os_version sflphone/debian/control

	echo "Building sflphone package on Ubuntu $os_version $arch_flag bit architecture...."
	# Provide prerequisite directories used by debuild
	cp sflphone sflphone-$ver -r
	cp sflphone sflphone-$ver.orig -r
	
	# Build packages
	cd sflphone-$ver/debian; debuild -k'Savoir-Faire Linux Inc.'

	# Post clean
	cd ..
	rm sflphone-$ver  sflphone -rf
	echo "Done! All the source packages and binary packages are generated in the current directory"

else
	# Prepare for packaging
	mv sflphone sflphone-$ver

	cp sflphone-$ver/platform/$platform.spec $BUILDDIR/SPECS/sflphone.spec
 	sed -e "s!@PREFIX@!/usr!" sflphone-$ver/libs/pjproject-1.0/libpj-sfl.pc.in > $BUILDDIR/SOURCES/libpj-sfl.pc
	tar zcvf sflphone-$ver.tar.gz sflphone-$ver

	rm sflphone-$ver -rf
	mv sflphone-$ver.tar.gz $BUILDDIR/SOURCES
	echo "Building sflphone package on $platform $arch_flag bit architecture...."

	# Build packages
	cd $BUILDDIR/SPECS/
	rpmbuild -ba --sign sflphone.spec

	echo "Done! All source rpms and binary rpms are stored in $BUILDDIR/SRPMS and $BUILDDIR/RPMS"
fi
