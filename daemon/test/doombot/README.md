SFL DoomBot
===========

This collection of script make it easy to run a native application in a `gdb`
shell. This allow unit, integration, fuzzy and stress tests to monitor the
process itself in case of a crash or assert. The DoomBot will then create a
report the developers can use to fix the issues.

We created this script to test `sflphone`, a network facing application. The
`SIP` protocol has some corner case and possible race conditions that may leave
the application in an unstable state.

## Installation

**Require:**

* python 3.0+
* gdb 7.7+
* flask-python3
* sqlite3 (+python3 bindings)

## Usage

### Common use case

#### Unit test executable

#### DBus interface

Make a script that call dbus methods until it crash

#### Network packet injection

Make a script that send packets to a port

#### File loading

Add an "load file and exit command line args"

## Web service API

The API is quite simple for now

```sh
    # GDB session
    curl http://127.0.0.1:5000/run/

    # Kill the current GDB session
    curl http://127.0.0.1:5000/kill/
```

## Roadmap

* The application is still tightly integrated with sflphone, a better separation is needed.
* Add valgrind support
* Add an API to add metadata
* Add sqlite backend
* Add md5 checksumming
* Use the gdb python API to check frames instead of `threads apply all bt full`
* Add a basic graph of the last 52 weeks
* Add the ability to execute a `gdb` script the next time this happen

## FAQ

### Is it possible to add accounts and passwords?

No, this service is not designed to be published on the internet. It allow
remote code execution by design. Keep this service in your intranet or add
some kind of HTTP authentication.

### Your code look like PHP4!

Yes, it does, the DoomBot frontend was designed as simple and as quick to develop
as possible. It doesn't have huge abstraction or any kind of template system. It
does what it does and that's it. If this ever take of, it would be one of the
first thing to fix.
