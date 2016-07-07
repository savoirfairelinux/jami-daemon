# Install

You need Python 3.5 and *pip*.

## Automated

Download [Ring](https://ring.cx/en/download) which will contain Ring-daemon.

> The PyPA recommended tool for installing Python packages.

    # optional: dive into virtualenv
    virtualenv -p python3.5 ENV
    source ENV/bin/activate

    make install

    # optional: to exit virtualenv write: deactivate

## Manual

### Ring-daemon (shared library)

    * Apply [4327 patch](https://gerrit-ring.savoirfairelinux.com/#/c/4327/). It was written due to bug [#699](https://tuleap.ring.cx/plugins/tracker/?aid=699) that was blocking the generation of the shared library.

    * Apply [4482 patch](https://gerrit-ring.savoirfairelinux.com/#/c/4482) which is blocking many features as calls due to the absence on register_thread from sip_utils.

    **As soon as the are merged, applying them won't be necessary.**

    To apply the patches:

    1. Download the Ring-daemon

            git clone https://gerrit-ring.savoirfairelinux.com/ring-daemon

    2. Apply the patch by going to its url, clicking on *Download* and copy-pasting the *Checkout* line in the *ring-daemon* directory. You can verify it was applied with *git log*.

    3. Build the shared library

            cd contrib; mkdir build; cd build
            ../bootstrap
            make; make
            make .opendht # needed on archlinux
            cd ../../
            ./autogen.sh
            ./configure --prefix=/usr
            make
            make install
