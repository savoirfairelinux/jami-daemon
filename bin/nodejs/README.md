# @savoirfairelinux/jami-core

Jami daemon Node.js bindings with TypeScript types.

This package loads the platform-specific native addon from the matching optional dependency package:

- `@savoirfairelinux/jami-core-darwin-arm64`
- `@savoirfairelinux/jami-core-linux-arm64`
- `@savoirfairelinux/jami-core-linux-x64`

Install `@savoirfairelinux/jami-core` directly. npm will install the supported native package for the current platform when available.

## Account and contact metadata

The metadata getters and setters use a `BlobMap`: keys are strings and
values are byte buffers. For example, with the loaded addon `jami`:

```js
const updates = new jami.BlobMap()
const value = new jami.Blob()
for (const byte of [0x00, 0x80, 0xff]) value.add(byte)
updates.set('example', value)
jami.setAccountMetadata(accountId, updates)
```

`AccountMetadataChanged` and `ContactMetadataChanged` deliver the full
snapshot as an object with Node.js `Buffer` values. An empty buffer is a
stored value, not a deletion.