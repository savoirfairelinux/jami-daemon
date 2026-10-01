# RFD 001: Swarm Roles and Permissions

**Last updated:** 2026-10-01

# Overview

Every swarm has a "mode", which indicates its purpose, and every participant has a "role", which, together with the mode, determines what actions they are allowed to take. The purpose of this RFD is to document the details of the currently supported modes and roles and the associated permissions.

# Swarm Modes

There are currently three supported modes:

| Mode | Description |
| --- | --- |
| `ONE_TO_ONE` | A private conversation between two contacts. |
| `INVITES_ONLY`| A group conversation where any existing member can invite new participants to join. |
| `DOCUMENT` | A collaborative document. |

*Note:* The `ConversationMode` enum contains two additonal modes which were never actually implemented: `ADMIN_INVITES_ONLY`, for group conversations where only admins can invite new members to join, and `PUBLIC`, for public conversations where anyone can join without requiring an explicit invitation.

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

| Action | Commit type | Additional commit keys | Definition |
| --- | --- | --- | --- |
| Text | `text/plain` | None | Send a new text message to the conversation. |
| TextReply | `text/plain` | `reply-to` | Send a new text message in reply to an existing message. |
| File | `application/data-transfer+json` | `displayName`, `sha3sum`, `totalSize` | Share a file in the conversation so its participants can receive it. |
| FileReply | `application/data-transfer+json` | `displayName`, `sha3sum`, `totalSize`, `reply-to` | Share a file in reply to an existing message. |
| Reaction | `text/plain` | `react-to` | Add a reaction to a conversation message. |
| Call | `application/call-history+json` | One-to-one: `to`, `duration`, optional `reason`; group: `confId`, `device`, `uri`, and `duration` when the call ends | Record an audio or video call associated with the conversation. |
| Edit | `text/plain` (`application/edited-message` in older commits) | `edit` | Change the content of an existing text message; `edit` refers to its original commit. |
| Delete | `text/plain` | `edit` | Remove a text message or reaction from the conversation with an empty `body`. |
| DeleteFile | `application/data-transfer+json` | `edit` | Remove a shared file from the conversation with an empty `tid`. |
| CreateDocument | `application/collab-doc+json` | `uri`, `displayName`, `mimeType` | Announce a collaborative document in a conversation; its contents live in a separate repository. |
| DeleteDocument | `application/collab-doc+json` | `edit` | Remove a collaborative document announcement; only its author may remove it. |
| Checkpoint | `application/checkpoint` | None | Save collaborative document updates (base64 in `body`) or an attachment (empty `body`) in the document repository. |
| AddMember | `member` | None | Invite or re-add a participant (`action`: `add`). In one-to-one conversations, only the original peer may rejoin; a third participant cannot be added. |
| JoinMember | `member` | None | Accept an invitation and join the conversation (`action`: `join`). |
| LeaveMember | `member` | None | Leave the conversation (`action`: `remove`). |
| Vote | `vote` | None | Cast an admin vote to ban or unban a participant or device. |
| BanUnbanMember | `member` | None | Resolve a vote to block a participant or device from the conversation, or lift that block (`action`: `ban` or `unban`). |
| UpdateProfile | `application/update-profile` | None | Update a swarm's shared profile, such as its title, description, or avatar in `profile.vcf`. This does not cover a participant’s personal Jami profile or the Qt client’s one-to-one peer contact profile. |

# Permissions

The cells list the roles allowed to perform each action in the given swarm mode. A dash means the action is not supported in that mode. Edits and deletions additionally require the author of the original commit; joining requires an invitation, and re-inviting someone in a one-to-one swarm is limited to its original peer.

| Action | `ONE_TO_ONE` | `INVITES_ONLY` | `DOCUMENT` |
| --- | --- | --- | --- |
| Text | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| TextReply | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| File | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| FileReply | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Reaction | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Call | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Edit | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Delete | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| DeleteFile | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| CreateDocument | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| DeleteDocument | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | --- |
| Checkpoint | --- | --- | `ADMIN`, `MEMBER` |
| AddMember | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` |
| JoinMember | `INVITED` | `INVITED` | `INVITED` |
| LeaveMember | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` | `ADMIN`, `MEMBER` |
| Vote | `ADMIN` | `ADMIN` | `ADMIN` |
| BanUnbanMember | `ADMIN` | `ADMIN` | `ADMIN` |
| UpdateProfile | `ADMIN` | `ADMIN` | `ADMIN` |


# Future Work
