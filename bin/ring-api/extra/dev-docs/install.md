# Install

You need Python 3.5 and *pip*.

## Automated

Download [Ring](https://ring.cx/en/download) which will contain Ring-daemon.

> The PyPA recommended tool for installing Python packages.

    # optional: go into virtualenv
    virtualenv -p python3.5 ENV
    source ENV/bin/activate

    # install it
    pip install -e .

    # to only build the package into dist/ folder
    python setup.py sdist

    # optional: to exit virtualenv write: deactivate

## Manual

1. Ring-daemon (shared library)

    Apply [this patch](https://gerrit-ring.savoirfairelinux.com/#/c/4327/). It was written due to bug [#699](https://tuleap.ring.cx/plugins/tracker/?aid=699) that was blocking the generation of the shared library.

    **As soon as it is merged, applying it won't be necessary.**

    1. Download the Ring-daemon

            git clone https://gerrit-ring.savoirfairelinux.com/ring-daemon

    2. Apply the patch by going to its url, clicking on *Download* and copy-pasting the *Checkout* line in the *ring-daemon* directory. You can verify it was applied with *git log*.

    3. Build the shared library

            cd contrib; mkdir build; cd build
            ../bootstrap
            make; make .opendht
            cd ../../
            ./autogen.sh
            ./configure --prefix=/usr
            make
            make install

2. Ring API

        # dependencies
        pip install --user -r requirements.txt

        # ring_api
        make

