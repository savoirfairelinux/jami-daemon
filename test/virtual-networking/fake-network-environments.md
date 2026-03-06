# UPnP/DHTNet Testing with Virtual Network Namespaces

## Overview

These test procedures verify that **Jami (via dhtnet)** correctly uses UPnP in a
controlled virtual network. The lab is set up with `setup-fake-upnp-network.sh`,
which creates three network namespaces:

```text
 ┌───────────────┐      ┌──────────────────────┐      ┌──────────────┐
 │   lan (Jami)  │──────│  rtr (NAT+miniupnpd) │──────│  wan (peer)  │
 │ 192.168.100.2 │      │  .100.1 ←→ 11.0.0.2  │      │   11.0.0.1   │
 └───────────────┘      └──────────────────────┘      └──────────────┘
```

Jami runs in `lan` behind NAT. The `wan` namespace simulates an external peer.
miniupnpd on `rtr` provides the UPnP IGD that dhtnet discovers.

### Prerequisites

- Root access (netns operations require it)
- `miniupnpd` installed (`sudo apt install miniupnpd` on Debian)
- `upnpc` (miniupnpc client) installed
- `opendht` Python package (`pip install opendht`) — for test case 3 only

### Starting the lab

All commands below assume your working directory is the directory containing
the scripts.

```bash
# Terminal 1: start the network lab (stays in foreground; Ctrl-C to stop)
sudo ./setup-fake-upnp-network.sh

# Terminal 2: open a shell in the lan namespace as your user
sudo ip netns exec lan sudo -u $USER -H bash -l
```

### Key reference: dhtnet port ranges

| What                  | Range / Default                              |
|-----------------------|----------------------------------------------|
| UPnP UDP external     | 20000–25000 (hardcoded in `upnp_context.cpp`)|
| UPnP TCP external     | 10000–15000 (idem)                           |
| DHT port range        | 4000–8888 (`JamiAccount::DHT_PORT_RANGE`)    |
| Default DHT port      | 0 (auto-select within range)                 |

### Key dhtnet log strings to watch for

Enable debug logging in Jami. Relevant log patterns:

| Event                    | Log pattern (grep for)                          |
|--------------------------|-------------------------------------------------|
| IGD discovered           | `Discovered a new IGD`                          |
| IGD validated            | `Added a new IGD`                               |
| External IP obtained     | `Setting IGD.*public address`                   |
| Mapping requested        | `Request mapping JAMI-`                         |
| Mapping created          | `successfully performed`                        |
| Mapping failed           | `Request for mapping.*failed`                   |
| DHT UPnP port allocated  | `Allocated port changed to`                     |
| DHT started on port      | `Mapping request is in.*state: starting the DHT`|

---

## Test Case 1: Port Setup — dhtnet Creates UPnP Mappings

**Goal:** Confirm that when Jami starts in the `lan` namespace, dhtnet
discovers the IGD and creates UPnP port mappings on the router.

### Steps

1. **Verify clean state** inside the `lan` namespace — no pre-existing mappings:

   ```bash
   upnpc -l
   ```

   Expected: no mappings listed (or only unrelated ones).

2. **Start Jami** inside the `lan` namespace:

   ```bash
   # In the lan shell (Terminal 2):
   jami -f /tmp/jami.log &    # or jami-qt, the ./build.py script, or however you launch the client
   ```

3. **Wait ~10 seconds** for dhtnet to discover the IGD and request mappings.

4. **Check the router's mapping table:**

   ```bash
   upnpc -l
   ```

5. **Check Jami logs** for UPnP activity:

   ```bash
   grep -iE "IGD|UPnP|mapping.*performed|public address"  /tmp/jami.log
   # or ~/.local/share/jami/jami*.log, or wherever your Jami logs are stored
   ```

### PASS criteria

- `upnpc -l` shows **at least one UDP mapping** in the 20000–25000 range
  pointing to `192.168.100.2`.
- Jami logs contain `Discovered a new IGD` and `successfully performed`.

### FAIL criteria

- No mappings appear after 30 seconds.
- Logs show `Request for mapping.*failed` with no subsequent success.
- `upnpc -l` reports one of the following scenarios:

   ```bash
   connect: Connection timed out
   No valid UPNP Internet Gateway Device found.
   ```

   or

   ```bash
   sendto: Network is unreachable
   No IGD UPnP Device found on the network !
   ```

   In these cases, check that the `setup-fake-upnp-network.sh` script is still running and that `upnpc -l` is called from inside the `lan` namespace.

---

## Test Case 2: WAN Reachability — DHT Node Access Through UPnP

**Goal:** Confirm that an external peer in the `wan` namespace can reach the
Jami DHT node through the UPnP-mapped port on the router.

### Steps

1. **Start Jami** in the lan namespace. Wait for UPnP mappings.

2. **Run the probe from the host** (not from inside any namespace):

   ```bash
   sudo bash probe-dht-from-wan.sh
   ```

   Without arguments, the script compares the UPnP-mapped ports (miniupnpd nft
   table) against the ports Jami is actually listening on (`ss -ulnp`), reports
   any mismatch, and sends a DHT ping for each port in the intersection.

   You can also test a specific port:

   ```bash
   sudo bash probe-dht-from-wan.sh 22238
   ```

### PASS criteria

- The script finds at least one port that is both UPnP-mapped and bound by Jami.
- The DHT ping succeeds for that port (`DHT_REACHABLE`).

### FAIL criteria

- **No overlapping ports:** The UPnP-mapped ports and Jami's bound ports are
  completely disjoint. The DHT node is unreachable from WAN.
- **DHT ping timed out:** A port is both mapped and bound but the ping still
  fails. Verify the port is a DHT listener and not an ICE/connection channel:
  `sudo ip netns exec lan ss -ulnp | grep <port>`

---

## Tearing Down

```bash
sudo ./setup-fake-upnp-network.sh down
```
