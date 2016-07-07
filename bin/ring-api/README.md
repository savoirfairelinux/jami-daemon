# Ring API

The documentation is located in the [Wiki](https://github.com/sevaivanov/ring-api/wiki).

Tested and stable at [6aae66b](https://github.com/sevaivanov/ring-api/commit/6aae66bb0d28514c24e87db1cecf004bea4d48f0).

## Roadmap

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
* Implement WebSockets structure for server initiated callbacks
* ~~Write a wiki base~~
* Wiki: write how it works with and draw a diagram
* Wiki: document the server and WebSockets software choices
* Add unit tests
* Add integration tests
* Integrate the project to Ring-daemon Autotools using the *--without-dbus* option
* Implement the functionalities:

    **It considered done when it's implement in both Cython and the RESTful server.**

    - ~~possibility to talk to the REST http interface of the daemon (the framework that you've written so far)~~
    - ~~control the "static" configuration of the daemon: add/remove an account, modify properties, enable/disable them~~
    - ~~be able to listen to the changes from the daemon (framework for signals)~~
    - execute dynamic features:
      - receive a message text (IM) out-of-call
      - send an IM out-of-call
      - be able to accept/refuse an incoming call
      - be able to display the status of a call and stop a call
      - tx/rx IM in-call
      - ~~display video, in-call and preview for camera setup (audio is fully controlled by the daemon)~~
      - add full call controls (media pause, transfer, audio controls, conferences, ...)
      - add full "smart list" features
      - ~~certificates controls~~


### Installation

#### Dependencies

1. Ring-daemon with [this patch](https://gerrit-ring.savoirfairelinux.com/#/c/4327/) written due to bug [#699](https://tuleap.ring.cx/plugins/tracker/?aid=699) that was blocking the generation of the shared library. As soon as it is merged, applying it won't be necessary.

    1. Download the Ring-daemon

            git clone https://gerrit-ring.savoirfairelinux.com/ring-daemon

    2. Apply the patch by going to its url, clicking on *Download* and copy-pasting the *Checkout* line in the *ring-daemon* directory. You can verify it was applied with *git log*.

    3. Build the shared library

            cd contrib; mkdir build; cd build
            ../bootstrap
            make; make .opendht
            cd ../../
            ./autogen.sh
            ./configure --prefix=/usr
            make
            make install

2. Python RESTful server

        pip install --user -r requirements.txt

3. Cython shared library

Install Cython and generate the ring_api library:

    cd ring_api; make; cd ../

### Running

There are two ways to interact with the Ring-daemon using the API. In both cases, you are using the client. You can either run a **Client script** located at project root called *client.py* that instantiates the Client class located in *ring_api/client.py* or import the *ring_api/client.py* in **Interpreter** mode to a Python interpreter (for example into [IPython](http://ipython.org/)).

#### Client script

It is recommended that you start it with the *--rest* option to be able to interact with it.

    $ ./client.py -h
    Usage: client.py [options] arg1 arg2

    Options:
      -h, --help        show this help message and exit
      -v, --verbose     activate all of the verbose options
      -d, --debug       debug mode (more verbose)
      -c, --console     log in console (instead of syslog)
      -p, --persistent  stay alive after client quits
      -r, --rest        start with restful server api
      --port=PORT       restful server port
      --host=HOST       restful server host
      --auto-answer     force automatic answer to incoming call
      --dring-version   show Ring-daemon version
      --interpreter     adapt threads for interpreter interaction

##### Examples

    ./client.py -rv

List all of the API routes at http://127.0.0.1:8080/all_routes/.

###### Send a text message

In another terminal you can send a text message:

    curl -X POST http://127.0.0.1:8080/user/send/text/<account_id>/<to_ring_id>/<message>/

#### Interpreter

It was tested using IPython.

    from ring_api import client

    # Options
    (options, args) = client.options()
    options.verbose = True
    options.interpreter = True

    # initialize the client
    ring = client.Client(options)

    # Callbacks
    cbs = ring.dring.callbacks_to_register()

    # i.e. define a simple callback
    from ring_api.callbacks import cb_api

    def on_text(account_id, from_ring_id, content):
        print(account_id, from_ring_id, content)

    # i.e. register this callback
    cbs['text_message'] = on_text
    ring.dring.register_callbacks(cbs)

    ring.start()

    # i.e. interogate the daemon
    account = ring.dring.config.accounts()[0]
    details = ring.dring.config.account_details(account)

    # i.e. send a text message
    ring.dring.config.send_text_message(
        account, '<to_ring_id>', {'text/plain': 'hello'})

    # Extra

    # show accessible content
    dir(client)
    dir(cb_api)

    # show documentation of some method
    help(ring.dring.config.account_details)

    # show callbacks documentation
    help(cb_api.text_message)

## Contributing

### Style

Coding: [PEP 8](https://www.python.org/dev/peps/pep-0008/)

Docstring: [PEP 257](https://www.python.org/dev/peps/pep-0257/)

## License

The code is licensed under a GNU General Public License [GPLv3](http://www.gnu.org/licenses/gpl.html).

## Authors

Seva Ivanov seva.ivanov@savoirfairelinux.com

Simon Zeni  simon.zeni@savoirfairelinux.com
