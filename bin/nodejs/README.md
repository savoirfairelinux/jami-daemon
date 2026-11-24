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

## Circles

The native interface exposes the methods from `circle_interface.h`, including
circle creation, invitations, policy updates, moderation and channels. Policy
rules are JSON strings such as
`{"allow":["read-channel","text"],"deny":[]}`. Optional arguments retain the
daemon's defaults. `createCircleChannel` creates and joins a channel; creating a
circle alone does not create one.

Register `CircleRequestReceived`, `CircleReady`, `CircleUpdated`, `CircleRemoved`
and `CircleChannelReady` in the callback map passed to `init`.

`getCircleProfile(accountId, circleId, channelId?)` returns shared vCard fields
`title`, `description` and `avatar` (base64). Pass a channel ID for its title
and topic; channel profiles are visible to every circle member, even before
joining the channel. `updateCircleProfile(accountId, circleId, fields, channelId?)`
updates only supplied fields; an empty value clears one. Circle edits require
`update-profile`, channel edits require `manage-channels` on that channel,
and successful changes raise `CircleUpdated`.

A joined channel holds messages like a conversation: pass its ID as the
conversation ID to `sendMessage`, `loadConversation`, `loadSwarmUntil`,
`clearCache` and `getConversationPermissions`, and receive its messages with
`SwarmLoaded`, `SwarmMessageReceived`, `SwarmMessageUpdated`, `ReactionAdded`,
`ReactionRemoved` and `OnConversationError`.
