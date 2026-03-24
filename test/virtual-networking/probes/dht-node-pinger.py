#!/usr/bin/env python3
# Ping a DHT node and report whether it is reachable.
# Usage: python3 probes/dht-node-pinger.py <host> <port>
# Exit 0 if the remote node responded, 1 otherwise.
import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <host> <port>", file=sys.stderr)
    sys.exit(1)

import opendht as dht

host, port = sys.argv[1], sys.argv[2]

addrs = dht.SockAddr.resolve(host, port)
if not addrs:
    print(f"Could not resolve {host}:{port}", file=sys.stderr)
    sys.exit(1)

node = dht.DhtRunner()
node.run()

try:
    reachable = node.ping(addrs[0])
finally:
    node.shutdown()

if reachable:
    print("DHT_REACHABLE")
    sys.exit(0)
else:
    print("DHT_UNREACHABLE")
    sys.exit(1)
