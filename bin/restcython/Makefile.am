include $(top_srcdir)/globals.mak

all:
	pip install --user -r requirements.txt
	cd ring_api
	make -j1
	cd ../..
	ln -s restcython/client.py dring
