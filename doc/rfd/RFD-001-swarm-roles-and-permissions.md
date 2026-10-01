# RFD 001: Swarm Roles and Permissions

**Last updated:** 2026-10-02

# Overview

Every swarm has a "mode", which indicates its purpose, and every participant has a "role", which, together with the mode, determines what actions they are allowed to take. The purpose of this RFD is to document the details of the currently supported modes and roles and the associated permissions.

# Swarm Modes

There are currently three supported modes:

| Mode | Description |
| --- | --- |
| `ONE_TO_ONE` | A private conversation between two contacts. |
| `INVITES_ONLY`| A group conversation where any existing member can invite new participants to join. |
| `DOCUMENT` | A collaborative document. |

*Note:* The `ConversationMode` enum contains two additional modes which were never actually implemented: `ADMIN_INVITES_ONLY`, for group conversations where only admins can invite new members to join, and `PUBLIC`, for public conversations where anyone can join without requiring an explicit invitation.

# Roles

There are five possible roles for a swarm participant:

| Role | Description |
| --- | --- |
| `ADMIN` | Administrator. There can currently only be one admin per swarm.  |
| `MEMBER` | Active swarm participant. |
| `INVITED` | A user who was invited to join the swarm but hasn't yet done so. |
| `BANNED`| Banned participants are no longer allowed to take any actions in the swarm or to fetch new commits added to it. |
| `LEFT` | A former participant who voluntarily left the swarm. This role is only used in `ONE_TO_ONE` swarms. |

*Note:* The `ADMIN` role is meant to represent the highest possible level of privilege in a Jami swarm. Other applications often use the word "owner" for this concept.

# Actions

The table below lists the currently supported actions across all swarm modes:

| Action | Definition | Representation in the git repository |
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
| BanUnbanMember | Block a participant or device from the conversation, or lift that block. | An admin's `vote` commit records the vote; once sufficient votes exist, a `member` commit with `action` set to `ban` or `unban` resolves it. |
| UpdateProfile | Update a swarm's shared profile, such as its title, description, or avatar in `profile.vcf`. | `application/update-profile` commit changes `profile.vcf` in the repository tree. |

# Permissions

The cells in the table below list the roles allowed to perform each action in the given swarm mode. A dash means the action is not supported in that mode.

| Action | `ONE_TO_ONE` | `INVITES_ONLY` | `DOCUMENT` |
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
| BanUnbanMember | `ADMIN` | `ADMIN` | `ADMIN` |
| UpdateProfile | --- | `ADMIN` | `ADMIN` |

*Note:* The restrictions on HostConference and UpdateProfile in `ONE_TO_ONE` swarms are not enforced in the code currently.

# Future Work

- Add the ability for an admin to override the default member permissions.
- Add the ability to create custom roles and thereby grant special privileges to certain members.
