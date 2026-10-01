# ADR-012: Make Swarm Permissions Explicit

**Status:** Accepted
**Origin:** Contemporary
**Decision date:** 2026-10
**Recorded:** 2026-11-02

## Context

Swarm actions are already restricted by the conversation mode and the
participant's role, but these permissions are implicit in operation-specific
code and commit validation functions. This makes adding new swarm modes
error-prone and would make it difficult to add support for custom roles or
to allow a swarm's admin to change the default permissions.

## Decision

Introduce an explicit permissions system for swarms. The initial implementation
will cover the following permissions:

| Permission | Description | Associated git commits |
| --- | --- | --- |
| Text | Send a new text message to the conversation. | `text/plain` commit with message text in `body`. |
| TextReply | Send a new text message in reply to an existing message. | `text/plain` commit with `reply-to` referencing the original message. |
| File | Share a file in the conversation. | `application/data-transfer+json` commit with `displayName`, `sha3sum`, `tid`, and `totalSize`. |
| FileReply | Share a file in reply to an existing message. | `application/data-transfer+json` commit with file metadata and `reply-to`. |
| Reaction | Add a reaction to an existing message. | `text/plain` commit with the reaction in `body` and the target in `react-to`. |
| Call | Initiate an audio or video call with a peer. | `application/call-history+json` commit when the call ends, with `to`, `duration`, and optional `reason`. |
| HostConference | Host an audio or video conference. | `application/call-history+json` commits when hosting starts and ends, both with `confId`, `device`, and `uri`; the end commit also has `duration`. |
| Edit | Change the content of a text message sent by oneself. | `text/plain` commit with the new `body` and `edit` referencing the original commit (`application/edited-message` in older commits). |
| Delete | Remove a text message or reaction sent by oneself from the conversation. | `text/plain` commit with an empty `body` and `edit` referencing the original commit. |
| DeleteFile | Remove a file shared by oneself from the conversation. | `application/data-transfer+json` commit with an empty `tid` and `edit` referencing the original commit. |
| CreateDocument | Announce a collaborative document in a conversation. | `application/collab-doc+json` commit with the document repository ID in `uri`, plus `displayName` and `mimeType`. |
| DeleteDocument | Remove a collaborative document announced by oneself. | `application/collab-doc+json` commit with `edit` referencing the announcement. |
| UpdateDocument | Save collaborative document updates in the document repository. | `application/checkpoint` commit with base64 updates in `body`. |
| AddDocumentAttachment | Add an attachment to a collaborative document. | `application/checkpoint` commit with an empty `body` and a content-addressed attachment in the repository tree. |
| AddMember | Invite or re-add a participant. In one-to-one conversations, only the original peer may rejoin; a third participant cannot be added. | `member` commit with `action` set to `add` and `uri` identifying the participant. |
| Join | Accept an invitation to the conversation. | `member` commit with `action` set to `join` and `uri` identifying the author. |
| Leave | Leave the conversation. | `member` commit with `action` set to `remove` and `uri` identifying the author. |
| BanUnbanMember | Block a participant or device from the conversation, or lift that block. | An admin's `vote` commit records the vote; once sufficient votes exist, a `member` commit with `action` set to `ban` or `unban` resolves it. |
| UpdateProfile | Update a swarm's shared profile, such as its title, description, or avatar in `profile.vcf`. | `application/update-profile` commit changes `profile.vcf` in the repository tree. |

The permissions for a given swarm participant are determined by their role and the swarm's mode,
as follows:

| | `ONE_TO_ONE` | `INVITES_ONLY` | `DOCUMENT` |
| --- | --- | --- | --- |
| Text | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| TextReply | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| File | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| FileReply | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Reaction | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Call | `ADMIN`, `MEMBER` | --- | --- |
| HostConference | --- | `ADMIN`, `MEMBER` | --- |
| Edit | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Delete | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| DeleteFile | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| CreateDocument | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| DeleteDocument | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| UpdateDocument | --- | --- | `ADMIN`, `MEMBER` |
| AddDocumentAttachment | --- | --- | `ADMIN`, `MEMBER` |
| AddMember | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` |
| Join | `INVITED` | `INVITED` | `INVITED` |
| Leave | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` |
| BanUnbanMember | --- | `ADMIN` | `ADMIN` |
| UpdateProfile | --- | `ADMIN` | `ADMIN` |

*Note:* The `ConversationMode` enum contains two additional modes which were never actually implemented
(`ADMIN_INVITES_ONLY` and `PUBLIC`) and are therefore omitted from the above table.

## Consequences

- Permissions will be discoverable and reviewable in one place.
- Less code will need to be added or modified when adding a new swarm mode.
  In particular, the existing commit validation methods should not need to
  be changed in most cases.
- Any new actions added to swarms must be assigned permissions.
- It will be easier to implement custom roles and admin overrides of default
  permissions in the future.
