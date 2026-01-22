
Introduction
----

Jami is a Voice-over-IP software phone. We want it to be:
- user friendly (fast, sleek, easy to learn interface)
- professional grade (transfers, holds, optimal audio quality)
- compatible with Asterisk (using SIP account)
- decentralized call (P2P-DHT)
- customizable

As the SIP/audio daemon and the user interface are separate processes,
it is easy to provide different user interfaces. Jami comes with
various graphical user interfaces and even scripts to control the daemon from
the shell.

Jami is currently used by the support team of Savoir-faire Linux Inc.

More information is available on the project homepage:
  https://www.jami.net/

This source tree contains the daemon application only that handles
the business logic of Jami. UIs are located in differents repositories. See
the Contributing section for more information.


Short description of content of source tree
----

- src/ is the core of libjami.
- bin/ contains application and binding main code.
- bin/dbus, the D-Bus XML interfaces, and C++ bindings

Build
----

cf [BUILD.md](/BUILD.md)

About Savoir-faire Linux Inc.
----

Savoir-faire Linux Inc. is a consulting company based in Montreal,
Quebec.  For more information, please check out our website:
https://www.savoirfairelinux.com/

Contributing to Jami
----

Of course we love patches. And contributions. And spring rolls.

Development website / Bug Tracker:
 - https://git.jami.net/savoirfairelinux/jami-project

Repositories are hosted on Gerrit, which we use for code review. It also
contains the client subprojects:
 - https://review.jami.net/#/admin/projects/

Do not hesitate to join us and post comments, suggestions, questions
and general feedback on the Jami mailing-list:
https://lists.gnu.org/mailman/listinfo/jami

COPYRIGHT NOTICE
----

Copyright (C) 2004-2025 Savoir-faire Linux Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
