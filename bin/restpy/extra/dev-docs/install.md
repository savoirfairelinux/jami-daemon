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

### Ring API (restpy)

It's located under *bin/restpy*. You have to build the Ring-daemon with it:

1. Download the Ring-daemon

        git clone https://gerrit-ring.savoirfairelinux.com/ring-daemon

2. Build it

        cd ring-daemon/contrib
        mkdir build; cd build
        ../bootstrap
        make
        cd ../

        ./autogen.sh
        ./configure prefix=/usr --without-dbus --with-restpy

        sudo make install

3. Run it

        dring-rest.py -h

### Ring-daemon (shared library)

* Apply [4482 patch](https://gerrit-ring.savoirfairelinux.com/#/c/4482) which is blocking many features as calls due to the absence on register_thread from sip_utils.

**As soon as it is merged, applying it won't be necessary.**

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
