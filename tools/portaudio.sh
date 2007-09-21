#!/bin/sh
PORTAUDIO_SOURCE=http://portaudio.com/archives/pa_snapshot_v19.tar.gz
PORTAUDIO_FILE=$(basename $PORTAUDIO_SOURCE)
PORTAUDIO_DIR=portaudio
PORTAUDIO_CONFIGURE_ARG=$@

if [ ! -f $PORTAUDIO_FILE ]; then
 wget $PORTAUDIO_SOURCE
fi

if [ ! -d $PORTAUDIO_DIR ]; then
 tar xzvf $PORTAUDIO_FILE
fi

if [ ! -f $PORTAUDIO_DIR/Makefile ]; then
  cd $PORTAUDIO_DIR
  ./configure --enable-cxx $PORTAUDIO_CONFIGURE_ARG

  make
  
  echo "To install portaudio (as root): " 
  echo "su -c \"cd $PORTAUDIO_DIR && make install\""
  echo ""

  
fi
