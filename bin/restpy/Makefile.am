include $(top_srcdir)/globals.mk

.PHONY: all install install-user clean

all: install-user build

build:
	python setup.py build_ext --inplace
	python setup.py build

install-user:
	pip install --user -r requirements.txt
	python setup.py install --user --record files.txt

install:
	pip install -r requirements.txt
	python setup.py install --record files.txt

uninstall:
	pip uninstall -y -r requirements.txt
	pip uninstall -y ring_api

clean: uninstall clean-files

clean-files:
	python setup.py clean --all
	rm -rf dist/ build/ ring_api.egg-info/ files.txt \
		ring_api/*.so ring_api/wrappers/*{h,.cpp}
