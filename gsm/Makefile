CC=g++

CFLAGS=-Wall -g -O3 -I. -I.. -DHAVE_CONFIG_H

AR=ar

OBJS = add.o\
	gsm_create.o\
	gsm_explode.o\
	long_term.o\
	short_term.o\
	code.o\
	gsm_decode.o\
	gsm_implode.o\
	lpc.o\
	table.o\
	debug.o\
	gsm_destroy.o\
	gsm_option.o\
	preprocess.o\
	decode.o\
	gsm_encode.o\
	gsm_print.o\
	rpe.o

all:	libgsm.a

.c.o: 
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.a *~ *.moc.cpp *.bak

libgsm.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

