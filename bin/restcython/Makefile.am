include $(top_srcdir)/globals.mak

all:
	cd ring_api && python setup.py build_ext --inplace

clean:
	cd ring_api && python setup.py clean
	rm -f ring_api/wrappers/dring_cython.cpp
	rm -f ring_api/wrappers/dring_cython.h
