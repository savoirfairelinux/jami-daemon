. ./config.sh

# configure, make and make install
cmmi() {
	dirname=
	configparam=

	if [ -z $2 ]; then
		dirname=`basename $1 '.tar.gz'`
	else
		dirname=$2
	fi

	configok=0
	cd $dirname
	if [ ! -e Makefile ]; then
		configok=2
		echo "Configuring and installing: $dirname"
		if [ ! -z $3 ]; then
			configparam=$3
		fi
		./configure $configparam --prefix=$SFL_PREFIX || exit
	else
		echo "$dirname is already configure. Remove the Makefile to reinstall"
	fi
	if [ $configok -eq 0 ]; then
		echo -en 'Do you want to make and make install the package? (y/n) '
		read r
		if [ "$r" = "y" ]; then
			configok=2
		fi
	fi
	if [ $configok -eq 2 ]; then
		make || return 
		echo "Enter you password to install the package as $USER (make install): "
		if [ "$SFL_INSTALL_USER" = "$USER" ]; then
			make install || exit
		else
			PATH=$PATH:/sbin su $SFL_INSTALL_USER -c 'make install' || exit;
		fi
	fi
	cd ..
}


#cmmi $SFL_FILE_CCPP2 '' '--without-libxml2'
#echo "Settings CPPFLAGS to $CPPFLAGS"
#export CPPFLAGS="-I$SFL_PREFIX/include/cc++2"
cmmi $SFL_FILE_CCRTP ''
#cmmi $SFL_FILE_LIBOSIP2 ''
#cmmi $SFL_FILE_LIBEXOSIP2 '' --disable-josua
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$SFL_PREFIX/lib/pkgconfig
#cmmi $SFL_FILE_PA_V19 $SFL_DIR_PA



