**NewDev**:
UI: User clicks on "connect from another device"
libjami::addAccount() -> accountId
| id = dht::crypto::generateIdentity("Jami-auth")
| connectionManager = dhtnet::ConnectionManager(id)
| emitSignal(OldDevuthState, accountId, TokenAvailable, id.second->getId())
| UI -> shows QR code for `jami-auth://$token` and text token to copy

**OldDev**:
UI: User clicks on "Add device" and scans QR code, or enter token manually
UI: User enters main account password (if any)
(?) libjami::addDevice(accountId, token, password) -> opId
 | C = argon2(deviceId + R).substr(6)
 | account.connectDevice(token, "jami-auth") -> socket
 | emitSignal(AddDeviceState, CodeAvailable, opId, C)
 | socket->write(fileutils::readArchive(archive, password))
 | socket->write(R)

**NewDev**:
connectionManager->onConnectionReady -> socket
| archive = socket->read()
| R = socket->read()
| C = argon2(socket->deviceId + R).substr(6)
| emitSignal(OldDevuthState, CodeAvailable, opId, C)
UI -> User confirms code
 | (?) libjami::addAccount("auth-session://opId")
 | ArchiveAccountManager -> get session from opId
 | emitSignal(OldDevuthState, Ended, opId, 0 /* success */)
 | socket->close()
 | ArchiveAccountManager::loadFromFile

**OldDev**:
socket->onShutdown()
 | emitSignal(AddDeviceState, Ended, opId, 0 /* success */)
