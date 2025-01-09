/*
 *  Copyright (C) 2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <msgpack.hpp>

namespace jami {

/**
 * @enum Permission
 * @brief Enum representing various permissions in the Jami application.
 */
enum class Permission {
    SendFile,              ///< Permission to send files.
    SendTextMessage,       ///< Permission to send text messages.
    React,                 ///< Permission to react to messages.
    Reply,                 ///< Permission to reply to text messages.
    Call,                  ///< Permission to initiate calls.
    Edit,                  ///< Permission to edit messages.
    DeleteMessage,         ///< Permission to delete messages.
    AddMember,             ///< Permission to add members to a group.
    ChangeProfile,         ///< Permission to change profile information.
    BanUnBanMember,        ///< Permission to ban or unban members.
    ChangeMemberRole,      ///< Permission to change member role.
    CreateRole,            ///< Permission to create new roles.
    RemoveRole             ///< Permission to remove role.
};

/**
 * @brief A static map that associates Permission enums with their corresponding string representations.
 */
static const std::unordered_map<Permission, std::string> permissions = {
    { Permission::SendFile, "SendFile" },
    { Permission::SendTextMessage, "SendTextMessage" },
    { Permission::React, "React" },
    { Permission::Reply, "Reply" },
    { Permission::Call, "Call" },
    { Permission::Edit, "Edit" },
    { Permission::AddMember, "AddMember" },
    { Permission::ChangeProfile, "ChangeProfile" },
    { Permission::BanUnBanMember, "BanUnBanMember" },
    { Permission::ChangeMemberRole, "ChangeMemberRole" },
    { Permission::CreateRole, "CreateRole" },
    { Permission::RemoveRole, "RemoveRole" }
};

/**
 * @brief Converts a Permission enum to its corresponding string representation.
 *
 * @param permission The Permission enum to convert.
 * @return A string representation of the given Permission.
 */
std::string permissionToString(Permission permission);

/**
 * @brief Converts a string representation of a permission to its corresponding Permission enum.
 *
 * @param str The string representation of the permission.
 * @return The corresponding Permission enum.
 * @throws std::invalid_argument if the provided string does not correspond to any Permission enum.
 */
Permission stringToPermission(const std::string& str);

} // namespace jami

MSGPACK_ADD_ENUM(jami::Permission)