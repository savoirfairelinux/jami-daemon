# Agent build steps (GNU/Linux only)
Last revision: 2021-08-31

# Requirements
Guile library version 3.0.7 or higher is required. Guile lib may require other
dependencies in particular libunistring-dev and libgc-deb packages.

Guile can be provided by the distro if available, or built locally. Note that
Guile v3.0.7 is quite recent and most likely not yet provided by most linux
distributions.
If the required version is available on your distro, just install it using your
distro's package manager. Development packages must be installed as well.

# Build Guile library
To build Guile locally, you first need to enable it when building contrib, then
recompile contrib:

```sh
cd daemon/contrib/native
../bootstrap --enable-guile
make list
make fetch
make -j
```

# Compile
To compile the agent, one has to enable it while configuring the daemon. At this
stage, Guile must be already available.

```sh
cd daemon
./configure --enable-agent  # other options can be added if needed such as --enable-debug
cd test/agent
make check
```

# Running the agent
The agent is actually a wrapper around Guile's Scheme shell.  By default, you
enter an interactive prompt that allows you to interact with Jami using the
primitive bindings or the by using the agent helper.  For options on starting
the agent, run `./agent --help`.

# Guile bindings
Guile needs primitive bindings to communicate with Jami.  Usually, these
bindings can written in pure Scheme using the foreign function interface (FFI)
and some dlopen() magics.  However, this method cannot applies here for two main
reasons:

  1. Jami source code is in C++ not C.
  2. Dynamic loading is not present on some platform supported by Jami.

The first reason makes it hard to interface C++ container types and other
standard types to bytevector used by Guile to interface with foreign functions.
In C, it's trivial to just types and pointers to bytevector.

The second reason is a constraint on the agent.  Since the goal is to have a set
of bindings that can run on any platform where Jami is supported, bindings
should be registered in C++.

All bindings can be found under `src/bindings/*`.  Bindings should be decouple
into module that reflect their common functionnality.  For example,
`src/bindings/account.h` has all the Jami's bindings for managing accounts.  When
a set of bindings is to be added to a new module, the latter has to be
registered into Guile.  In order accomplish this, one has to include the
bindings and define the module under `src/bindings/bindings.cpp`.

When a binding is called from Guile, the arguments passed are Scheme objects of
type `SCM`.  This is an opaque type that is generic.  In order to be clear on
what the underlying type needed by the primitive procedure is, one should add the
suffix of the type at the end.

For example, `my_primitive_procedure()` expects that `some_variable_str`
will be of type `string`.

There's also a set of utilities that can be used to convert C++ object to Scheme
and vice versa.  These utilities can be found under `src/utils.h` and are all
template based and can usually be used without specifying any type thanks to
inference.

For example, to convert `std::string bar` to `SCM baz`, onw would do
`baz = to_guile(bar)`.  One can also do the oposite like so
`bar = from_guile(baz)`.

# Examples
See `examples/`
