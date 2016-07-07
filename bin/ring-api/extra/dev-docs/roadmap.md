# Roadmap

## 0.1.0

* ~~Initialize Ring~~
* ~~Start Ring~~
* ~~Parse arguments~~
* ~~Get account info for demonstration~~
* ~~Implement RESTful API skeleton~~
* ~~Implement encoding / decoding protocols~~
* ~~Implement the Python package architecture~~
* ~~Add threading~~
* ~~Register callbacks~~
* ~~Define python callbacks API~~
* Segment wrappers into multiple files
* ~~Decide whether to use REST + WebSockets or only WebSockets~~
* ~~Select multi-threaded RESTful server~~
* ~~Define RESTful API standards~~
* ~~Define RESTful API in json~~
* ~~Implement RESTful API using Flask-REST~~
* ~~Implement WebSockets structure for server initiated callbacks~~
* ~~Write a wiki base~~
* Wiki: write how it works with and draw a diagram
* Wiki: document the server and WebSockets software choices
* ~~Add unit tests~~
* Integrate the project to Ring-daemon Autotools (GNU Build System)

        ./configure prefix=/usr --without-dbus --with-restcython

* Implement the functionalities:

    **It considered done when it's implement in both Cython and the RESTful server.**

    - ~~possibility to talk to the REST http interface of the daemon (the framework that you've written so far)~~
    - ~~control the "static" configuration of the daemon: add/remove an account, modify properties, enable/disable them~~
    - ~~be able to listen to the changes from the daemon (framework for signals)~~
    - execute dynamic features:
      - tx/rx IM in-call
      - ~~receive a message text (IM) out-of-call~~
      - ~~send an IM out-of-call~~
      - ~~be able to accept/refuse an incoming call~~
        - TODO: hangup crash
      - ~~be able to display the status of a call and stop a call~~
      - ~~display video, in-call and preview for camera setup (audio is fully controlled by the daemon)~~
      - add full call controls (media pause, transfer, audio controls, conferences, ...)
      - add full "smartInfo" features
      - ~~certificates controls~~

