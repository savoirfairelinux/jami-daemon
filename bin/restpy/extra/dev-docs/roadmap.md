# Roadmap

## 0.1.0

* Cython
    * ~~Initialize Ring~~
    * ~~Start Ring~~
    * ~~Parse arguments~~
    * ~~Get account info for demonstration~~
    * ~~Implement (de)serialization~~
    * ~~Register callbacks~~
    * ~~Define python callbacks API~~
    * Segment wrappers into multiple files

* ~~Client~~
    * ~~Add threading~~
    * ~~Define start options~~

* Server
    * ~~Implement RESTful API skeleton~~
    * ~~Decide whether to use REST + WebSockets or only WebSockets~~
    * ~~Select multi-threaded RESTful server~~
    * ~~Define RESTful API in json~~
    * ~~Implement RESTful API using Flask-REST~~
    * ~~Implement WebSockets structure for server initiated callbacks~~
    * ~~Add unit tests~~
    * Format the logs to dring style
    * Solve the 'Dirty Hack' in server.py

* ~~Integration~~
    * ~~Implement the Python package architecture~~
    * ~~Integrate the project to Ring-daemon Autotools (GNU Build System)~~

            ...
            ./configure prefix=/usr --without-dbus --with-restpy
            make install
            dring-rest.py

        See Gerrit public draft at [4518](https://gerrit-ring.savoirfairelinux.com/#/c/4518/).

* Wiki
    * ~~Write a wiki base~~
    * ~~Define RESTful API standards~~
    * ~~Write how it works with and draw a diagram~~
    * Document the server and WebSockets software choices

* Functionalities:

    * **Notes**
        * It is defined as done when it's implemented up to the server top layer
        * Audio is fully controlled by the daemon

    * Daemon
        * ~~Talk to the REST HTTP interface of the daemon~~
        * ~~Control the "static" configuration of the daemon: add/remove an account, modify properties, enable/disable them~~
        * ~~Listen to the changes from the daemon (framework for signals)~~

    * Instant message
      * ~~Receive a message text (IM) out-of-call~~
      * ~~Send an IM out-of-call~~
      * Send / receieve in-call

    * Call
        * ~~Accept/refuse an incoming call~~
        * ~~Display the status of a call and stop a call~~
        * Video
            * ~~Send~~
            * Receive
            * Display
            * Preview for camera setup
                Justify that it does into the api

        * Controls
            * Media pause
            * Transfer
            * Audio controls
            * Conferences

        * SmartInfo statistics features
        * ~~Certificates controls~~
