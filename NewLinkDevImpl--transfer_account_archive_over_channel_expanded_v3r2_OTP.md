# New Link Device Documentation (WIP+TODO)

```mermaid
sequenceDiagram
	participant New Device Client
	participant New Device Core
	participant Old Device Core
	participant Old Device Client
	Old Device Client-)+Old Device Client: press export account
	Old Device Client-)+Old Device Client: choose scan QR || enter OTP to add new device to your account (UI tries to get camera: t->QR, f->OTP)
	New Device Client-)+New Device Client: user presses link existing account (two options: (1) scan QR or (2) enter OTP)
	New Device Client->>+New Device Core: call addAccount(scheme="qr" or "otp")
	New Device Core-)+New Device Core: Generate a temporary account && opId
	New Device Core->>+New Device Client: Status Code TokenAvail && scheme "jami-auth://$tmpId?code=XXXXXX" ([0-9]x6) (TODO uint64_t with java compat) && opId
	New Device Client--)+New Device Client: generate & show QR || OTP from scheme (OTP should easily map to tmpAccount name either 1-1 or like PIN function in archive_account_manager)
	New Device Core-->>+New Device Client: SIG TokenAvail && startAuthTimeout (TODO updates ctx with a timestamp to check if connection should close??)
	New Device Client->>+Old Device Client: user goes back to Old Device
	Old Device Client->>+Old Device Client: on Old Device: QR is scanned || OTP is entered
	Old Device Client->>+Old Device Core: SIG tobenamed_(tmpId) (this starts the ChannelSocket connection)
	New Device Core->+Old Device Core: Dht Connection established
	New Device Core->+Old Device Core: TLS P2P connection established
    Old Device Core->>+New Device Core: PROTOCOL=ESTABLISHED: establish password scheme (key (bytes) || none)
   	New Device Core-->>+Old Device Core: send back schemeMsg w/ version
	Old Device Core->>+Old Device Client: SIG ClientConnection (TODO naming)
	Old Device Client-->>+Old Device Client: stop showing camera screen || OTP field (show dialog that says to enter your acconut password on the other device if hasPassword==t)
   	New Device Core-->>+New Device Client: SIG ClientConnection
    New Device Client--)+New Device Client: blur QR || OTP visual (can also display connected animation)
    New Device Core-->+New Device Core: START_PASS marker
    New Device Core-->>+New Device Client: SIG PeerConnected(opId, passwordEnabled=t/f)
    New Device Client-->+New Device Client: show the account identity/profile + password field (if password=true)
	New Device Client--)+New Device Client: user enters archive password & presses enter
    New Device Client-->>+New Device Core: libjami::provideAccountAuthentication(opId, password as bytes)
    New Device Core->>+Old Device Core: send password over TLS channel
    Old Device Core->>+Old Device Core: verify password
    Old Device Core-->>+Old Device Client: authSuccessCallback || authFailureCallback
    Old Device Core->>+New Device Core: (3x) msg w/ jami_account_archive IF password correct ELSE failed status code
    New Device Core-->>+New Device Client: SIG DeviceAuthState: success || fail
    New Device Core-->>+New Device Core: (3x) IF failed go back to START_PASS marker ELSE continue
    Old Device Core->+New Device Core: close TLS
    Old Device Core->+New Device Core: close DHT Connection
    New Device Core->>+New Device Core: remove tmpAccount from account list
    New Device Client-)+New Device Client: Home/Welcome Back Page
    Old Device Client-)+Old Device Client: Success Page (authSuccessCallback) || Failure Page (authFailureCallback)
```

## Notes

- `AccountId` is an interaction that involves the user scanning a QR code displayed on the __New Device__ using the camera on the __Old Device__
- `Dht Connection` is an abstract representation of a lower-level set of operations performed by DhtNet
- `TLS Connection` is an abstract representation of a lower-level set of operations performed by TLS
- all other interactions are using the secure TLS protocol to transmit data
- `jami_account_archive` will be a default account config OR empty if __Old Device__ encounters an *error*

------

more notes on states:

- TokenAvail
  - client: need to show QR code on current screen (think it's add account screen TODO)
- ...

todos:

- ponder JAMS integration in future (2FA for authenticateAccount, etc.)

# Communication Channel Implementation

The two devices have to balance user interaction and TLS communication. The devices will use the DHT to initiate a peer-to-peer connection and communicate over a single TLS channel. Both devices send and receive messages throughout the process of transferring the archive from the old device to the new device, so the channel is bidirectional.

```mermaid
graph TD
	0((Old Device))
	1((New Device))
```

# Future Plans for the Protocol

- the protocol should be reversible because not all devices have cameras and it may be more convenient to do the reverse OOB operation of scanning qr code from old device with new device (new device scans qr code from old device)
  - need to verify that this does not pose any security threats due to modifications to the order of presented certificate chains (ask Adrien)
  - need to rework the UI in order to accomodate the accessibility of being scan a qr from either end
- generalize protocol to request loading any account address via proxy account TLS connection with an API that accepts username (username  or other identifier of the account that is associated with the requested archive), password (password associated with opening this account), sender_id (tmp account that will send the archive over TLS)
  - need to make sure that connection is initiated first before password is sent and have some sort of verification code that the user must verify before the channel communicates the password or the archive to prevent mitm/faking/phishing

# Archive Download Communication Scheme v1.0

Developed after partner programming with Adrien on 2023-11-20.

## Summary

This protocol is for validating the account transfer by requiring a password or key that only the account owner can provide. This lock and key mechanism ensures that the archive cannot be exported by attackers.

## Diagrams

Old Device (client):

````mermaid
graph LR
	established-->error & schemeSent
	schemeSent-->error & requestTransmitted
	requestTransmitted-->error & authError & archiveSent
````

New Device (server):

````mermaid
graph LR
	established-->error & schemeKnown
	schemeKnown-->error & requestTransmitted
	requestTransmitted-->error & authError & archiveReceived
````

---------

## State Descriptions

- Established
  - communication channel is opened
  - scheme version needs to be communicated before any further interaction can occur
    - ensures backwards compatibility
    - web-like behavior is well-understood and can be built upon easily
- SchemeSent / SchemeKnown
  - ...
- GenericError
  - can be reached from any state if invalid message received
  - currently error states are terminal and will close the communication channel

- AuthError
  - if password is incorrect send this
  - currently error statses are terminal and will close the communication channel

# State Machine for Status Transitions

Two Signals:

1. `AddDeviceStateChanged`
   1.
2. `DeviceAuthStateChanged`
   1.

# Documentation of Status Codes

- 0XX
  - general description: TODO
  - list of codes
    - TokenAvailabe: TokenAvail, 1
    -
- 1XX
  - some sort of continuation
- 2XX

# Android UI Demo

## New Device

### Flow

```mermaid
graph TD
	0(Token Available\nshows qr code)
	1(Connecting\nshows a connecting animation screen)
	0-->1
	2(Auth\nshows profile of account to import and a password box if auth required)
	1-->2
	3(Success/Done\nshows a completion message and something about using your account with some confetti)
	2-->3
```

### Error States

These error states can occur at any time and modify the above flow of UI changes.

```mermaid
graph LR
	4(Timeout\nfor a connection timeout if no devices connect within given time)
	5(Auth Error\nfor the case where the password or credentials are not entered correctly ie., 2FA)
	6(Network Error\nfor things like archive fails to download or if connection to wifi is lost, etc.)
```
