# @savoirfairelinux/jami-core

Jami daemon Node.js bindings with TypeScript types.

This package loads the platform-specific native addon from the matching optional dependency package:

- `@savoirfairelinux/jami-core-darwin-arm64`
- `@savoirfairelinux/jami-core-linux-arm64`
- `@savoirfairelinux/jami-core-linux-x64`

Install `@savoirfairelinux/jami-core` directly. npm will install the supported native package for the current platform when available.