# Ring API

It is an Application Program Interface to interact with the headless Ring-daemon written in C++11.

This project uses Cython for Python <-> C++ bindings. We wrap this with Python along with a RESTful web-server. It allows anyone to communicate with the daemon using a RESTful interface and to receive daemon-originated signals using WebSockets. It can be directly used in Python or through the latter web-interface.

Originally developed on [GitHub](https://github.com/sevaivanov/ring-api) with the documentation located in the [Wiki](https://github.com/sevaivanov/ring-api/wiki).

## Install

1. You need Python3.5 and pip.

    I didn't tested with other Python versions yet. Feel free to tell me it works!

2. You need [Ring](https://ring.cx/en/download) which will contain the Ring-daemon.

    **Note:** until patch [4482](https://gerrit-ring.savoirfairelinux.com/#/c/4482) is merged, you need to do it manually. See: [extra/dev-docs/install.md](extra/dev-docs/install.md).

3. Install the Ring API:

        pip install --user -e .

## Running

You can choose from the below options depending on your needs.

* Client script
    * Execute the *ring_api* Python module
    * Execute the Ring-daemon soon integrated version

* Extend in Python
    * Import it into interpreter of your choice
    * Import it into a Python script

### Module

It is recommended that you start it with the *--rest* option to be able to interact with it.

    $ python -m ring_api -h
    Usage: __main__.py [options] arg1 arg2

    Options:
      -h, --help         show this help message and exit
      -v, --verbose      activate all of the verbose options
      -d, --debug        debug mode (more verbose)
      -c, --console      log in console (instead of syslog)
      -p, --persistent   stay alive after client quits
      -r, --rest         start with restful server api
      --port=HTTP_PORT   server http port for rest
      --ws-port=WS_PORT  server websocket port for callbacks
      --host=HOST        restful server host
      --auto-answer      force automatic answer to incoming call
      --dring-version    show Ring-daemon version
      --interpreter      adapt threads for interpreter interaction

### Python / Interpreter

To see an example of usage inside a Python script, you can skip to the *Examples* > *Node: message replier* section.

In interpreter, it was tested using IPython but the threading wasn't designed to be run with the RESTful server.

    from ring_api import client

    # Options
    options = client.options()
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
    cbs['account_message'] = on_text
    ring.dring.register_callbacks(cbs)

    ring.start()

    # i.e. interogate the daemon
    account = ring.dring.config.accounts()[0]
    details = ring.dring.config.account_details(account)

    # i.e. send a text message
    ring.dring.config.send_account_message(
        account, '<to_ring_id>', {'text/plain': 'hello'})

    # Extra

    # show accessible content
    dir(client)
    dir(cb_api)

    # show documentation of some method
    help(ring.dring.config.account_details)

    # show callbacks documentation
    help(cb_api.account_message)

### Ring-daemon integration

The main difference with the Python module script is that the RESTful server is forced and the interpreter mode is disabled. From the Ring-daemon perspective the RESTful interface is a replacement for the D-Bus. The *Ring API* integration under *restpy* name acts as an executable which runs and is used by other clients. The daemon only needs an API for IPC (inter-process communication) which is in this case our RESTful server.

1. Build

    See the section *Manual > Ring API (restpy)* of the [install docs](extra/dev-docs/install.md)

2. Run

    This executable is copied from [extra/run/force-rest.py](extra/run/force-rest.py) and renamed during the install.

        dring-rest.py -h

## Examples

### REST API layout

You can get the documentation in JSON format by going to:

    http://127.0.0.1:8080/api/v1/

It's the documentaion located in the ```ring_api/rest-api/<version>/api.json``` file.

### Texting with *curl*

Start the module along with the REST server:

    python -m ring_api -rv

1. Get the available accounts

        curl http://127.0.0.1:8080/api/v1/accounts/

2. To get the destination Ring Id, just text the server's Ring Id and copy-paste it from the console's output.

3. Send an account message:

    The data is in JSON string format.

        curl -X POST -d '{"ring_id":"<ring_id>","message":"curling","mime_type":"text/plain"}' http://127.0.0.1:8080/api/v1/accounts/<account_id>/message/

4. Get the message status:

        curl http://127.0.0.1:8080/api/v1/messages/<message_id>/

### Calling

**Experimental**

You will send and receive video with audio. However, video displaying is WIP (Work In Progress).

    # Call

    $ curl -d '{"ring_id":"<ring_id>"}' http://localhost:8080/api/v1/accounts/<account_id>/call/
    {
      "call_id": "<call_id>",
      "status": 200
    }

    # Handup

    $ curl -X PUT -d '{"action":"hangup"}' http://localhost:8080/api/v1/calls/<call_id>/
    {
      "status": 200,
      "unhold": false
    }

### Node: message replier

It is located under *extra/examples/*. The Node has an EchoBot which stores and forwards the account messages using a bot-like *!bang* syntax. It is important to understand that this Node never leaves the Ring over OpenDHT network.

    $ ./node.py -h
    usage: node.py [-h] [-v] -c CLIENTS

    Ring API node using bots

    optional arguments:
      -h, --help            show this help message and exit
      -v, --verbose
      -c CLIENTS, --clients CLIENTS
                            Clients as JSON string of '{"alias": "ring_id"}'

The advantage of using aliases as secrets associated with your Ring Ids is that it very simple to remember and write them. The requester will get all of the messages from all contacts which makes it simple to pull everything associated with your slave machine Ring Id at once.

1. Start the node using the Ring API on your slave machine which will act as a replier.

        ./node.py -vc '{"roger":"<ring_id>"}'

2. From any other device -- text your slave machine which will enqueue messages.

    From a third or same as previous (i.e. cellphone) device -- text the slave machine to forward all of the messages to the 'Ring Id' associated with 'roger' alias:

        !echo roger reply

    The slave machine will display that:

        [Ring node is listening]
        [Received    ] '!echo roger reply' : rerouting all messages to '<ring_id>'
        [Forwarding  ] '[2016-07-23 13:39:40.351862 : <ring_id>] : Did you know'
        [Forwarding  ] '[2016-07-23 13:39:42.346681 : <ring_id>] : that I like ..'
        [Forwarding  ] '[2016-07-23 13:39:44.717613 : <ring_id>] : waffles?'

    The device from which you requested it will display the messages in order under this form:

        [<date time> : <ring_id_dest>] : <message>

This a quick practical example. The exhaustive documentation is comming soon.

## Contributing

### Coding

* Style
    * [PEP 8](https://www.python.org/dev/peps/pep-0008/)

* Docstring
    * [PEP 257](https://www.python.org/dev/peps/pep-0257/)

### Packaging

* Overview
    * [Human video](https://www.youtube.com/watch?v=4fzAMdLKC5k)
    * [Python docs](https://packaging.python.org/distributing/)

* Versioning
    * [PEP 440](https://www.python.org/dev/peps/pep-0440/)
    * [Semantic Versioning 2.0.0](http://semver.org/)

* [Classifiers](https://pypi.python.org/pypi?%3Aaction=list_classifiers)

## License

The code is licensed under a GNU General Public License [GPLv3+](http://www.gnu.org/licenses/gpl.html).

## Authors

Seva Ivanov seva.ivanov@savoirfairelinux.com

Simon Zeni  simon.zeni@savoirfairelinux.com
