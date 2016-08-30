# Index

* [Introduction](https://github.com/sevaivanov/ring-api/wiki/Home#what-it-is)
* Ring API
    * Python
    * Web interface
* [FAQ](https://github.com/sevaivanov/ring-api/wiki/Faq)

The documentation is at its early stage.

# What it is?

For those of you who just came to know this project, Ring is a peer-to-peer software for telecommunication. It makes it possible for users to stream video and audio to place calls or exchange text messages. Many additionnal features like sending GIFs, file-sharing, multi-device and even mining usernames are currently in development. All of the exchanged data is encrypted in real-time. There is no central server. Users search an OpenDHT network made of [D]istributed [H]ash [T]ables to find peers and establish encrypted communications. An analogy of it would be going outside to discover the world without a central established meeting point hosted by a certain authority.

The beauty of this project is in the bounding to the idea of direct interactions between living things. Indeed, it extends to anything and everything including the IoT (Internet Of Things), surveillance networks and so forth. Yes, at the moment its primary clients are humans. However, any device capable of transmitting multimedia would be suitable for the use of the Ring core.

What about hacking, then?

The project is built in the C++11 programming language with its primary interface being the D-Bus, which is an IPC (inter-process communication) only present on Gnu / Linux. Writing an API (application programming interface) for the Ring project would be a way of extending it to anything capable of using its functionalities.

The main idea was to bundle the Ring-daemon with a RESTful server to allow anything to communicate with it using the web protocols. However, after a thoughtful [research](https://github.com/sevaivanov/ring-for-the-web#introduction) of possible ways to achieve this, it was decided that we would go further. We wrapped the Ring-daemon using Cython which exposed the Ring-daemon to the Python echosphere. Nevertheless, the RESTful server is present but is not mandatory for hacking.

Python has enormous amounts of stable libraries and is a fast growing mature language. In fact, it opens the Ring project to developers that can quickly hack or architect any project ideas using Ring as a decentralized multimedia core over an OpenDHT network.

# How it works?

Here is its basic conceptual flow:

![ring-api flow diagram](https://raw.githubusercontent.com/wiki/sevaivanov/ring-api/diagrams/ring-api.png)

The *client.py* contains the Client class. This class is the controller of both dring and server parties. It implements the threading logic and is the entry point of ring\_api. The \__main__.py entry point called by ```python -m ring_api``` module instantiates the class, which acts according to the provided user's options.

At the start, it imports the generated shared Cython library *dring_cython.so* and creates an instance from it. From this point, depending on the user's options:

* It starts *server.py* (which is written with Flask-restful framework) along with the dring instance which the server will use to directly interact with the Ring-daemon

<p style="text-align:center">OR</p>

* It acts as a module used in a script or interpreter and it waits for manually triggered actions passing through the client interface.

# Contributing

The drafts are located in [ring-api/extra/wiki-drafts/](https://github.com/sevaivanov/ring-api/tree/master/extra/wiki-drafts).

Publish only the documentation approved by the team members.
