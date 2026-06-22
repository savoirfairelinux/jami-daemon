# Jami core (daemon)

Jami is an open-source peer-to-peer communication tool that allows users to make voice and video calls, send messages, and share files. It is designed to be secure, private, and fully distributed. It is written in C++20.

# Building

From `build/`:
```bash
cmake .. -GNinja
ninja
```

# Dependencies

libjami-core (the daemon) uses the "contrib" system to manage its dependencies. See the contrib `skill` for details.
Key dependencies include:
- `opendht`: The kademlia DHT implementation.
- `dhtnet`: Used to establish TLS peer-to-peer connections using the opendht and ICE.
- `pjproject`: Jami and dhtnet use a fork of pjproject implementing TCP-ICE (rfc6544) to establish peer-to-peer TCP connections, and for SIP support used for calls and conferences in Jami.
- `ffmpeg`: Used for media processing, including encoding and decoding of audio and video streams.

# Work instructions

- Use the relevant skill or topic file before starting work
- Use the 'askQuestions' tool in case of any doubt about the path to follow.
- Always make sure everything builds after making changes. When relevant, test your changes before committing or completing work.
- Always commit your work in small, logical commits with clear messages.
