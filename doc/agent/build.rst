==================================
Agent build steps (GNU/Linux only)
==================================

Requirements
============

Guile library version 3.0.7 or higher is required. Guile lib may require other
dependencies in particular libunistring-dev and libgc-deb packages.

Guile can be provided by the distro if available, or built locally. Note that
Guile v3.0.7 is quite recent and most likely not yet provided by most linux
distributions.  If the required version is available on your distro, just
install it using your distro's package manager. Development packages must be
installed as well.


Compile
=======

To compile the agent, one has to enable it while configuring the daemon. At this
stage, Guile must be already available::

  cd daemon
  ./configure --enable-agent  # other options can be added if needed such as --enable-debug
  cd test/agent
  make check


