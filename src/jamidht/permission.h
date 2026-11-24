#pragma once

#include <optional>
#include <string_view>

namespace jami {

// Values are persisted in initial commits; keep them stable.
enum class ConversationMode : int {
    ONE_TO_ONE = 0,
    ADMIN_INVITES_ONLY = 1,
    INVITES_ONLY = 2,
    PUBLIC = 3,
    DOCUMENT = 4,
    FEED = 5
};

enum class MemberRole { ADMIN = 0, MEMBER, INVITED, BANNED, LEFT };

enum class Permission {
    Text,
    TextReply,
    File,
    FileReply,
    Reaction,
    Call,
    HostConference,
    Edit,
    Delete,
    DeleteFile,
    CreateDocument,
    DeleteDocument,
    UpdateDocument,
    AddDocumentAttachment,
    Join,
    Leave,
    AddMember,
    BanUnbanMember,
    UpdateProfile,
    ReadChannel,
    ManageChannels,
    ManageRoles,
    AssignRoles,
    ModerateConference,
    Count,
};

constexpr bool
isValidPermission(Permission permission) noexcept
{
    return permission >= Permission::Text && permission < Permission::Count;
}

constexpr std::string_view
permissionWireName(Permission permission) noexcept
{
    switch (permission) {
    case Permission::Text:
        return "text";
    case Permission::TextReply:
        return "text-reply";
    case Permission::File:
        return "file";
    case Permission::FileReply:
        return "file-reply";
    case Permission::Reaction:
        return "reaction";
    case Permission::Call:
        return "call";
    case Permission::HostConference:
        return "host-conference";
    case Permission::Edit:
        return "edit";
    case Permission::Delete:
        return "delete";
    case Permission::DeleteFile:
        return "delete-file";
    case Permission::CreateDocument:
        return "create-document";
    case Permission::DeleteDocument:
        return "delete-document";
    case Permission::UpdateDocument:
        return "update-document";
    case Permission::AddDocumentAttachment:
        return "add-document-attachment";
    case Permission::Join:
        return "join";
    case Permission::Leave:
        return "leave";
    case Permission::AddMember:
        return "add-member";
    case Permission::BanUnbanMember:
        return "ban-unban-member";
    case Permission::UpdateProfile:
        return "update-profile";
    case Permission::ReadChannel:
        return "read-channel";
    case Permission::ManageChannels:
        return "manage-channels";
    case Permission::ManageRoles:
        return "manage-roles";
    case Permission::AssignRoles:
        return "assign-roles";
    case Permission::ModerateConference:
        return "moderate-conference";
    default:
        return {};
    }
}

constexpr std::optional<Permission>
permissionFromName(std::string_view name) noexcept
{
    if (name.empty())
        return std::nullopt;
    for (int value = 0; value < static_cast<int>(Permission::Count); ++value) {
        auto permission = static_cast<Permission>(value);
        if (name == permissionWireName(permission))
            return permission;
    }
    return std::nullopt;
}

constexpr bool
hasPermission(ConversationMode mode, MemberRole role, Permission permission) noexcept
{
    if (role == MemberRole::INVITED) {
        switch (mode) {
        case ConversationMode::ONE_TO_ONE:
        case ConversationMode::INVITES_ONLY:
        case ConversationMode::DOCUMENT:
        case ConversationMode::FEED:
            return permission == Permission::Join;
        default:
            return false;
        }
    }
    if (role != MemberRole::ADMIN && role != MemberRole::MEMBER)
        return false;

    switch (mode) {
    case ConversationMode::ONE_TO_ONE:
        switch (permission) {
        case Permission::Text:
        case Permission::TextReply:
        case Permission::File:
        case Permission::FileReply:
        case Permission::Reaction:
        case Permission::Call:
        case Permission::Edit:
        case Permission::Delete:
        case Permission::DeleteFile:
        case Permission::CreateDocument:
        case Permission::DeleteDocument:
        case Permission::Leave:
        case Permission::AddMember:
            return true;
        default:
            return false;
        }
    case ConversationMode::INVITES_ONLY:
        switch (permission) {
        case Permission::Text:
        case Permission::TextReply:
        case Permission::File:
        case Permission::FileReply:
        case Permission::Reaction:
        case Permission::HostConference:
        case Permission::Edit:
        case Permission::Delete:
        case Permission::DeleteFile:
        case Permission::CreateDocument:
        case Permission::DeleteDocument:
        case Permission::Leave:
        case Permission::AddMember:
            return true;
        case Permission::BanUnbanMember:
        case Permission::UpdateProfile:
            return role == MemberRole::ADMIN;
        default:
            return false;
        }
    case ConversationMode::FEED:
        switch (permission) {
        case Permission::Leave:
            return role == MemberRole::MEMBER;
        case Permission::Text:
        case Permission::File:
        case Permission::AddMember:
        case Permission::BanUnbanMember:
        case Permission::UpdateProfile:
            return role == MemberRole::ADMIN;
        case Permission::TextReply:
        case Permission::FileReply:
        case Permission::Reaction:
        case Permission::Edit:
        case Permission::Delete:
        case Permission::DeleteFile:
            return true;
        default:
            return false;
        }
    case ConversationMode::DOCUMENT:
        switch (permission) {
        case Permission::UpdateDocument:
        case Permission::AddDocumentAttachment:
        case Permission::Leave:
        case Permission::AddMember:
            return true;
        case Permission::BanUnbanMember:
        case Permission::UpdateProfile:
            return role == MemberRole::ADMIN;
        default:
            return false;
        }
    default:
        return false;
    }
}

} // namespace jami
