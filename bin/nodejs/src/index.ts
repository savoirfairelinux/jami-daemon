/*
 * Copyright (C) 2022-2026 Savoir-faire Linux Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * TypeScript definitions for the Jami daemon SWIG Node.js addon (jamid.node).
 *
 * This module is the single source of truth for SWIG type definitions,
 * conversion helpers, and the JamiSwig interface. Used by both the server
 * and the SDK.
 */

import { createRequire } from 'node:module'

/**
 * Signals emitted by the Jami daemon.
 *
 * The definition of signals can be found in `daemon/bin/nodejs/callback.h`.
 */
export enum DataTransferError {
  success = 0,
  unknown = 1,
  io = 2,
  invalid_argument = 3,
}

/**
 * Signals emitted by the Jami daemon.
 *
 * The definition of signals can be found in `daemon/bin/nodejs/callback.h`.
 */
export enum JamiSignal {
  // libjami::ConfigurationSignal
  AccountsChanged = 'AccountsChanged',
  AccountDetailsChanged = 'AccountDetailsChanged',
  AccountProfileReceived = 'AccountProfileReceived',
  RegistrationStateChanged = 'RegistrationStateChanged',
  ComposingStatusChanged = 'ComposingStatusChanged',
  IncomingTrustRequest = 'IncomingTrustRequest',
  ContactAdded = 'ContactAdded',
  ContactRemoved = 'ContactRemoved',
  ExportOnRingEnded = 'ExportOnRingEnded',
  NameRegistrationEnded = 'NameRegistrationEnded',
  RegisteredNameFound = 'RegisteredNameFound',
  VolatileDetailsChanged = 'VolatileDetailsChanged',
  KnownDevicesChanged = 'KnownDevicesChanged',
  IncomingAccountMessage = 'IncomingAccountMessage',
  AccountMessageStatusChanged = 'AccountMessageStatusChanged',
  ProfileReceived = 'ProfileReceived',
  DeviceAuthStateChanged = 'DeviceAuthStateChanged',
  AddDeviceStateChanged = 'AddDeviceStateChanged',
  UserSearchEnded = 'UserSearchEnded',
  DeviceRevocationEnded = 'DeviceRevocationEnded',
  ClearCache = 'ClearCache',

  // libjami::CallSignal
  StateChange = 'StateChange',
  IncomingMessage = 'IncomingMessage',
  IncomingCall = 'IncomingCall',
  IncomingCallWithMedia = 'IncomingCallWithMedia',
  MediaChangeRequested = 'MediaChangeRequested',

  // libjami::ConversationSignal
  ConversationLoaded = 'ConversationLoaded',
  SwarmLoaded = 'SwarmLoaded',
  MessagesFound = 'MessagesFound',
  SwarmMessageReceived = 'SwarmMessageReceived',
  SwarmMessageUpdated = 'SwarmMessageUpdated',
  ReactionAdded = 'ReactionAdded',
  ReactionRemoved = 'ReactionRemoved',
  ConversationProfileUpdated = 'ConversationProfileUpdated',
  ConversationRequestReceived = 'ConversationRequestReceived',
  ConversationRequestDeclined = 'ConversationRequestDeclined',
  ConversationReady = 'ConversationReady',
  ConversationRemoved = 'ConversationRemoved',
  ConversationMemberEvent = 'ConversationMemberEvent',
  OnConversationError = 'OnConversationError',
  OnConferenceInfosUpdated = 'OnConferenceInfosUpdated',

  // libjami::DataTransferSignal
  DataTransferEvent = 'DataTransferEvent',

  // libjami::PresenceSignal
  SubscriptionStateChanged = 'SubscriptionStateChanged',
  NearbyPeerNotification = 'NearbyPeerNotification',
  NewBuddyNotification = 'NewBuddyNotification',
  NewServerSubscriptionRequest = 'NewServerSubscriptionRequest',
  ServerError = 'ServerError',
}

// ── Generic SWIG collection types ───────────────────────────

export interface Constructable<T> {
  new (): T
}

interface SwigVect<T> {
  size(): number
  get(index: number): T | undefined
}

interface SwigMap<T, U> {
  keys(): SwigVect<T>
  get(key: T): U | undefined
  set(key: T, value: U): void
}

export type StringVect = SwigVect<string>
export type StringMap = SwigMap<string, string>
export type VectMap = SwigVect<StringMap>
export type Blob = SwigVect<number>

// ── Conversion helpers ──────────────────────────────────────

function* swigVectToIt<T>(swigVect: SwigVect<T>) {
  const size = swigVect.size()
  for (let i = 0; i < size; i++) {
    yield swigVect.get(i)!
  }
}

function* swigMapToIt<T, U>(swigMap: SwigMap<T, U>) {
  const keys = swigVectToIt(swigMap.keys())
  for (const key of keys) {
    const value = swigMap.get(key)!
    yield [key, value]
  }
}

export function stringVectToArray(stringVect: StringVect): string[] {
  return Array.from(swigVectToIt(stringVect))
}

export function stringMapToRecord(stringMap: StringMap): Record<string, string> {
  const record: Record<string, string> = {}
  for (const [key, value] of swigMapToIt(stringMap)) {
    record[key] = value
  }
  return record
}

export function vectMapToRecordArray(vectMap: VectMap): Record<string, string>[] {
  const records = []
  for (const stringMap of swigVectToIt(vectMap)) {
    records.push(stringMapToRecord(stringMap))
  }
  return records
}

export interface JamiInitOptions {
  flags?: number
}

// ── JamiSwig interface ──────────────────────────────────────

/**
 * Non-exhaustive list of properties for the SWIG-generated native addon.
 *
 * The full list of methods can be found in SWIG interface files (`.i`) in `daemon/bin/nodejs`.
 */
export interface JamiSwig {
  init(args: Record<string, unknown>, options?: JamiInitOptions): void
  fini(): void

  monitor(continuous: boolean): void

  clearCache(accountId: string, conversationId: string): void

  // Accounts
  getAccountDetails(accountId: string): StringMap
  getVolatileAccountDetails(accountId: string): StringMap
  setAccountDetails(accountId: string, details: StringMap): void
  addAccount(details: StringMap): string
  removeAccount(accountId: string): void
  updateProfile(accountId: string, displayName: string, avatarPath: string, fileType: string, botOwner: string, flag: number): void
  getAccountList(): StringVect

  // Account text messages
  sendAccountTextMessage(accountId: string, contactId: string, message: StringMap, flag: number): void

  // Name service
  lookupName(accountId: string, nameserver: string, username: string): boolean
  lookupAddress(accountId: string, nameserver: string, address: string): boolean
  registerName(accountId: string, username: string, scheme: string, password: string): boolean
  searchUser(accountId: string, query: string): boolean

  // Devices
  getKnownRingDevices(accountId: string): StringMap
  revokeDevice(accountId: string, deviceId: string, scheme: string, password: string): boolean
  addDevice(accountId: string, uri: string): boolean
  confirmAddDevice(accountId: string, opId: number): boolean
  cancelAddDevice(accountId: string, opId: number): boolean
  provideAccountAuthentication(accountId: string, credentials: string, scheme: string): boolean

  // Contacts
  addContact(accountId: string, contactId: string): void
  removeContact(accountId: string, contactId: string, ban: boolean): void
  getContacts(accountId: string): VectMap
  getContactDetails(accountId: string, contactId: string): StringMap
  sendTrustRequest(accountId: string, to: string, payload: Blob): void
  acceptTrustRequest(accountId: string, from: string): boolean
  discardTrustRequest(accountId: string, from: string): boolean

  // Default moderators
  getDefaultModerators(accountId: string): StringVect
  setDefaultModerator(accountId: string, uri: string, state: boolean): void

  // Conversations
  startConversation(accountId: string): string
  getConversations(accountId: string): StringVect
  conversationInfos(accountId: string, conversationId: string): StringMap
  getConversationMembers(accountId: string, conversationId: string): VectMap
  removeConversation(accountId: string, conversationId: string): void
  updateConversationInfos(accountId: string, conversationId: string, infos: StringMap): void
  searchConversation(
    accountId: string,
    conversationId: string,
    author: string,
    lastId: string,
    regexSearch: string,
    type: string,
    after: number,
    before: number,
    maxResult: number,
    flag: number,
  ): boolean

  // Conversation requests
  getConversationRequests(accountId: string): VectMap
  acceptConversationRequest(accountId: string, conversationId: string): void
  declineConversationRequest(accountId: string, conversationId: string): void

  // Conversation members
  addConversationMember(accountId: string, conversationId: string, uri: string): void
  removeConversationMember(accountId: string, conversationId: string, uri: string): void

  // Conversation preferences
  getConversationPreferences(accountId: string, conversationId: string): StringMap
  setConversationPreferences(accountId: string, conversationId: string, preferences: StringMap): void

  // Messages
  sendMessage(accountId: string, conversationId: string, message: string, replyTo: string, flag: number): void
  loadConversationMessages(accountId: string, conversationId: string, fromMessage: string, n: number): number
  loadConversation(accountId: string, conversationId: string, fromMessage: string, n: number): number
  loadSwarmUntil(accountId: string, conversationId: string, fromMessage: string, toMessage: string): number
  setIsComposing(accountId: string, conversationId: string, isWriting: boolean): void
  setMessageDisplayed(accountId: string, conversationId: string, messageId: string, status: number): boolean

  // Calls
  getCallList(accountId: string): StringVect
  getCallDetails(accountId: string, callId: string): StringMap

  // File transfer
  sendFile(accountId: string, conversationId: string, path: string, displayName: string, replyTo: string): void
  downloadFile(accountId: string, conversationId: string, interactionId: string, fileId: string, path: string): number
  cancelDataTransfer(accountId: string, conversationId: string, fileId: string): DataTransferError
  fileTransferInfo(
    accountId: string,
    conversationId: string,
    fileId: string,
    path_out: string,
    total_out: number,
    progress_out: number,
  ): DataTransferError

  // Presence
  publish(accountId: string, status: boolean, note: string): void
  answerServerRequest(uri: string, flag: boolean): void
  subscribeBuddy(accountId: string, uri: string, flag: boolean): void
  getSubscriptions(accountId: string): VectMap
  setSubscriptions(accountId: string, uris: string[]): void

  // Constructors for SWIG types
  StringMap: Constructable<StringMap>
  Blob: Constructable<Blob>
}

// ── Native module loader ────────────────────────────────────

/**
 * Load the Jami native addon (`jamid.node`) for the current platform.
 *
 * Attempts to load a platform-specific package
 * (e.g. `@savoirfairelinux/jami-core-linux-x64`), which is installed via
 * `optionalDependencies` on matching platforms.
 */
export function loadNativeModule(): JamiSwig {
  const require = createRequire(import.meta.url)
  const platform = process.platform
  const arch = process.arch
  const packageName = `@savoirfairelinux/jami-core-${platform}-${arch}`

  try {
    return require(packageName) as JamiSwig
  } catch (error: unknown) {
    const reason = error instanceof Error ? error.message : String(error)
    throw new Error(`Failed to load Jami native module for ${platform}-${arch} ` + `(${packageName}): ${reason}`)
  }
}
