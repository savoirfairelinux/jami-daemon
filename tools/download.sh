#!/bin/sh
. ./config.sh

download()
{
    filename=`basename $1`
    echo -n "Checking for $filename..."
    if [ ! -e $filename ]; then
        echo -en "\nDownloading $filename..."
        wget -q $1 -U "http://www.sflphone.org/" && echo -en " OK "
    fi
    echo ""
}

untar()
{
    filename=`basename $1`
    if [ -z $2 ]; then
        dirname=`basename $filename '.tar.gz'`
    else
        dirname=$2
    fi
    echo -n "Checking for directory $dirname"
    if [ -e $filename -a ! -e $dirname ]; then
        echo -en "\nExtracting..."
        tar xzf $filename
    fi
    echo " OK"
}
backupdir()
{
    if [ -z $2 ]; then
        dirname=`basename $1 '.tar.gz'`
    else
        dirname=$2
    fi
    echo -n "Checking for backup for $dirname"
    if [ ! -e "$dirname.orig" ]; then
        echo -en "\nCopying..."
        cp -R $dirname "$dirname.orig"
    fi
    echo " OK"
}
configure() {
	./configure --prefix=$PREFIX
}

prepare()
{
    if [ -z $1 ]; then
	echo 'ERROR: prepare need a file URI not empty'
    else
        echo 'Preparation for' $1
        download $1
        untar $1 $2
    #   backupdir $1 $2
    fi
}

prepare $SFL_FILE_CCPP2 ''
prepare $SFL_FILE_CCRTP ''
prepare $SFL_FILE_LIBOSIP2 ''
#prepare $SFL_FILE_LIBEXOSIP2 ''
#prepare $SFL_FILE_PA_V19 $SFL_DIR_PA

