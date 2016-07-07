include $(top_srcdir)/globals.mak

.PHONY: all install clean

all: install build # we need the deps to run it

build:
	python setup.py build_ext --inplace
	python setup.py build

cython:
	python setup.py build_ext --inplace

sdist:
	python setup.py sdist # source distribution -> dist/

install: # into local user files
	pip install --user -r requirements.txt
	python setup.py install --user --record files.txt

uninstall:
	pip uninstall -y -r requirements.txt
	pip uninstall -y ring_api

clean: uninstall clean-files

clean-files: # TODO change .so for a variable to support other platforms
	python setup.py clean --all
	rm -rf dist/ build/ ring_api.egg-info/ files.txt \
		ring_api/*.so ring_api/wrappers/*{h,.cpp}
