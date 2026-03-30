# Introduction

Jami is a voice-over-IP software phone.
Features include:
- user-friendly (fast, sleek, easy-to-learn interface)
- professional grade (file/call transfers, hold/resume, optimal audio quality, individual/group calls)
- compatible with Asterisk (using SIP account)
- peer-to-peer distributed calls (P2P-DHT)
- customizable

The SIP/audio daemon and the user interface are separate processes.
Different user interfaces and scripts can easily be used to control the daemon.
Jami comes with various graphical user interfaces and scripts to control the daemon from the shell.

Jami is currently used by the support team of Savoir-faire Linux Inc.

More information is available on the project homepage:
 - https://www.jami.net/

This source tree contains only the daemon, which handles the business logic of Jami.
User interfaces are located in different repositories.
See the [Contributing](#contributing) section for more information.

# Short description of the contents of the source tree

- `src/` is the core of libjami.
- `bin/` contains application and binding main code.
- `bin/dbus` contains the D-Bus XML interfaces, and C++ bindings

# About Savoir-faire Linux Inc.

Savoir-faire Linux Inc. is a consulting company based in Montreal, Quebec.
For more information, please visit the following website:
 - https://www.savoirfairelinux.com/

# Building and installing

For build instructions, required dependencies, and platform-specific notes, see
[INSTALL.md](INSTALL.md).

# Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to set up the development
environment, contribute code, and reach the community.

# COPYRIGHT NOTICE

Copyright (C) 2004-2026 Savoir-faire Linux Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

See [COPYING](/COPYING) or https://www.gnu.org/licenses/gpl-3.0.en.html for the full GPLv3 license.
