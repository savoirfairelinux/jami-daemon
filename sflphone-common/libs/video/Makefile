CFLAGS=-Wall -g -O2 `pkg-config --cflags theora`
SERVER_LIBS=-lavutil -lavdevice -lavformat -lavcodec -lz -lavutil -lswscale \
			-lm -pthread -lasound -lx264 -ljack -lvpx `pkg-config --libs theora` -ltheoraenc -ltheoradec
CLUTTER_CFLAGS=`pkg-config --cflags clutter-1.0`
CLUTTER_LIBS=`pkg-config --libs clutter-1.0`

APPS = ffmpeg2shm shmclient ffmpeg2rtp
all:  $(APPS)

ffmpeg2rtp: ffmpeg2rtp.c
	$(CC) -o $@ $^ $(SERVER_LIBS) $(CFLAGS)

ffmpeg2shm: ffmpeg2shm.c
	$(CC) -o $@ $^ $(SERVER_LIBS) $(CFLAGS)

shmclient: shmclient.c
	$(CC) -o $@ $^ $(CFLAGS) $(CLUTTER_CFLAGS) $(CLUTTER_LIBS)

.PHONY: clean

clean:
	rm -rf $(APPS)
