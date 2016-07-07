# Architecture

There are two ways to interact with the Ring-daemon. In both cases, you are using the client. You can either run a **Client script** located at the project root called *client.py* that instantiates the Client class located in *ring_api/client.py* or import the *ring_api/client.py* with **Interpreter** mode to a Python interpreter (i.e. IPython).

## API

It has a modular layout which means that the Cython API is independent of the RESTful API.

    ring_api/
       ├── client.py
       ├── interfaces
       │   ├── configuration_manager.pxd
       │   └── dring.pxd
       │
       ├── restfulserver
       │   ├── api
       │   │   ├── ring.py
       │   │   ├── root.py
       │   │   └── user.py
       │   └── server.py
       │
       ├── setup.py
       ├── utils
       │   └── std.pxd
       │
       └── wrappers
           └── dring.pyx

### Cython API

First, we expose the Ring-daemon interfaces located in the */usr/include/dring/* header files. We accomplish that using Cython by rewriting the relevant parts we wish to expose. These *.pxd* files are located in the *interfaces/* directory. Afterwards, we wrap these interfaces using Cython again to sanitize between Cython <-> Python. These *.pyx* files are located in *wrappers/*.

By itself, Cython wrapping may be considered an API because it is interacting with C++ Ring-daemon. In the Interpreter mode, you're using the Cython wrappers without the need to start a RESTful API server.

### RESTful API

The API modules which are located in the *restfulserver/api/* are receiving an instance of the dring during intialization which is located in the Cython wrappers. The server directly uses this instance to communicate with the Ring-daemon.

#### Layout

This is a general layout that we should respect in all of the API modules located in */restfulserver/api/*. As the API will become bigger, we should keep it intuitive and consistent.

See [REST API Standards](https://github.com/sevaivanov/ring-api/wiki/REST-API-Standards).
