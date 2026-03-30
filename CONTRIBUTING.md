# Contributing to jami-daemon

We welcome contributions of all kinds - bug fixes, features, tests, and
documentation improvements. Below is everything you need to get started.

## Community & code review

Development website and issue tracker:
- https://git.jami.net/savoirfairelinux/

Repositories are hosted on Gerrit for code review.
It also contains the client sub-projects:
- https://review.jami.net/admin/repos/

Jami user forums.
- https://forum.jami.net/

## Development environment with Nix

### What is Nix?

[Nix](https://nixos.org/) is a package manager and build system that describes
software environments purely and reproducibly. Unlike `apt`, `brew`, or `dnf`,
Nix installs packages into an isolated store (`/nix/store`) and never modifies
the rest of your system. Each package is identified by a cryptographic hash of
all its inputs, which means:

- the environment is **identical on every machine** regardless of what the host
  OS has installed,
- multiple versions of the same library can coexist without conflict,
- the environment can be deleted cleanly without leaving behind any system-wide
  changes.

We use Nix to declare the complete build environment for jami-daemon in
[flake.nix](flake.nix), including all compilers, build tools, and library
dependencies.

### Disk space

Nix downloads and builds packages into `/nix/store`. For this project, expect
the development shell to occupy roughly **3–5 GB** of store space on a fresh
machine. If you already use Nix for other projects, most of the common
packages (GCC, glibc, cmake, etc.) will already be present and are shared, so
the incremental cost will be considerably lower.

### Installing Nix

Install Nix using the official [installer script](https://nixos.org/download/) (supports Linux and macOS).

### Entering the development shell

The development environment is fully specified in [flake.nix](flake.nix). To
enter it, run the following from the repository root:

```bash
nix develop --experimental-features 'nix-command flakes'
```

This drops you into a bash shell with all build tools and library dependencies
on `PATH` and in `PKG_CONFIG_PATH`. The first invocation will download and
build all missing packages; subsequent invocations are instant.

If Nix complains about experimental features, add the following to
`~/.config/nix/nix.conf` to make it permanent:

```
experimental-features = nix-command flakes
```

One can also enable automatic entry of the nix development environment for the project by installing [direnv](https://direnv.net/), and typing
```
direnv allow
```
at the root of the project. This allows for a more seamless development experience as you do not need to manually enter the nix shell each time.


### Fully sandboxed builds with `nix develop -i`

For a pure, sandboxed environment that is completely isolated from your host
system (no `~/.local`, no system `PATH`, no host environment variables), run:

```bash
nix develop -i
```

### Building inside the shell

Once inside `nix develop`, follow the standard build instructions in
[INSTALL.md](INSTALL.md). The quickest path to a working build on Linux is:

```bash
mkdir build && cd build
cmake .. -DJAMI_DBUS=On
make -j$(nproc)
```

## Building without Nix

If you prefer not to use Nix, see [INSTALL.md](INSTALL.md) for per-platform
dependency lists and build instructions for Linux, macOS, Windows, Android,
and Docker.
