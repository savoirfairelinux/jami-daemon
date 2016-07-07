# Ring API

It's exposing the Ring-daemon using Cython to any high-level language.

The documentation is located in the [Wiki](https://github.com/sevaivanov/ring-api/wiki).

## Install

You need Python 3.5, pip and [Ring](https://ring.cx/en/download) which will contain the Ring-daemon.

**Note:** Until [this patch](https://gerrit-ring.savoirfairelinux.com/#/c/4327/) solving the [bug #699](https://tuleap.ring.cx/plugins/tracker/?aid=699) is merged, you need to do it manually. See: [extra/dev-docs/install.md](extra/dev-docs/install.md).

Install the Ring API:

    pip install --user -e .

## Running

You can either execute the *ring_api* module or import it into interpreter of your choice.

### Module

It is recommended that you start it with the *--rest* option to be able to interact with it.

    $ python -m ring_api -h
    Usage: client.py [options] arg1 arg2

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

### Interpreter

It was tested using IPython. It wasn't designed to be run with the REST Server.

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

## Examples

### Send a text message using *cURL*

Start the module along with the REST server:

    python -m ring_api -rv

1. Get the available accounts

        curl http://127.0.0.1:8080/api/v0.1/accounts/

2. To get the destination Ring Id, just text the server's Ring Id and copy-paste it from the console's output.

3. Send an account message:

    The data is in JSON string format.

        curl -X POST -d '{"ring_id":"<ring_id>","message":"curling","mime_type":"text/plain"}' http://127.0.0.1:8080/api/v0.1/accounts/<account_id>/message/

4. Get the message status:

        curl http://127.0.0.1:8080/api/v0.1/message/<message_id>/

### Run a node acting as a message replier

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

