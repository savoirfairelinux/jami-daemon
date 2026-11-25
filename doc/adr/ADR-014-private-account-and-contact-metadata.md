# ADR-014: Sync Private Account and Contact Metadata as LWW Registers

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-11
**Recorded:** 2026-11-25

## Context

Clients need to store small, account-private state (for example settings or
per-contact annotations) that follows the user across their devices but is
never shared with contacts. Existing account data either travels with
contact or conversation lifecycle (contacts, trust requests, conversation
preferences) or is device-local configuration. Concurrent edits on several
devices, devices that are offline for long periods, and account archives used
to link or restore devices must all converge to the same state without a
central server.

## Decision

- Model metadata as a map of independent per-key last-writer-wins registers
  (`Metadata` in `src/metadata.h`). Each entry stores a value, a millisecond
  timestamp and the writer's device ID. Merges keep the higher
  `(timestamp, writer)` stamp; equal stamps with different values are rejected.
- Local updates are PATCHes: only supplied keys change, unchanged values keep
  their stamps, and a changed key is stamped
  `max(now, previous stamp + 1)`. Keys are never deleted; an empty value is
  a value.
- Keys are non-empty UTF-8 without NUL, up to 1024 bytes. Values are arbitrary
  bytes up to 64 KiB. State is limited to 16384 entries and 4 MiB. Remote and
  stored state is validated on decode and merge; invalid input is rejected
  atomically.
- Account metadata is persisted in its own MessagePack file
  (`accountMetadata`) by `MetadataStore`. Contact metadata is embedded in each
  `Contact` and saved with the contact list. Both write a staging file and
  rename it before reporting success or notifying clients.
- Contact metadata belongs to the current addition of an active contact.
  Removing or banning a contact discards it, and re-adding starts empty.
  Reading or writing metadata of a removed or banned contact fails. Removal
  emits no metadata signal; `ContactRemoved` already implies it.
- Contact metadata is part of the contact and follows the contact merge rule
  (`Contact::merge`): registers are merged only between copies of the same
  addition and dropped when the merged contact is inactive. This keeps removal convergent across devices without filtering
  registers by timestamp.
- Synchronize only between the account's own devices, inside `DeviceSync`.
  Full syncs on new sync connections carry the account registers and contacts
  with their metadata. A change sends the account registers, or the whole
  changed contact in `DeviceSync::peers`, over existing sync connections only,
  and never wakes disconnected devices.
- Include both kinds of metadata in the account archive so linked and restored
  devices start from the merged state.
- Expose `get/setAccountMetadata` and `get/setContactMetadata` in libjami, with
  `AccountMetadataChanged` and `ContactMetadataChanged` signals carrying the
  full snapshot. Values cross the API as byte vectors.

## Consequences

- Clients get cross-device private state without defining their own sync
  protocol or storage, and the daemon does not interpret keys or values.
- Convergence relies on device clocks; a device with a fast clock wins
  concurrent edits until others pass its timestamp. Per-key registers cannot
  merge structured values; clients must split data across keys when
  concurrent partial edits matter.
- Account keys accumulate forever. Clients needing deletion must encode it in
  values. Contact keys last until the contact is removed.
- Contact metadata cannot outlive a removal, even by mistake; clients that
  want to keep annotations about former contacts must store them elsewhere.
- A device that wrote contact metadata while unaware of a concurrent removal
  and re-addition loses those writes once it learns of the new addition.
- Each contact metadata change resends all of that contact's registers.
- Metadata changes reach offline devices only at their next full sync, not
  immediately.
- `DeviceSync`, `Contact` and the account archive gain optional fields.
  Older daemons neither expose nor merge them, so devices running them do not
  take part in metadata synchronization.
- Readers accept legacy MessagePack string values written before values became
  bytes; new values are written as MessagePack binary.

## References

- `44973f704` metadata: add reusable metadata store
- `c716370e1` account: sync private metadata across devices
- `3e9638fd8` contact: sync private metadata across devices
- `e31c506d8`, `604ed234e` metadata: store values as bytes
- API contracts: `src/jami/configurationmanager_interface.h`
