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

#include "permissions.h"
#include "logger.h"

#include <unordered_set>

namespace jami {
    // Abstract Base Role Class
class Role {
public:
    virtual ~Role() = default;

    // Method to check if the role has a specific permission
    virtual bool hasPermission(Permission permission) const = 0;

    // Get the role's name
    virtual std::string name() const = 0;

    // Get the role's permissions
    virtual std::unordered_set<Permission> permissions() const = 0;
};

// Derived Class: Admin Role
class AdminRole : public Role {
public:
    AdminRole() {
        permissions_ = {
            Permission::SendFile, Permission::SendTextMessage, Permission::React, Permission::Reply,
            Permission::Call, Permission::Edit, Permission::AddMember, Permission::ChangeProfile,
            Permission::BanUnBanMember, Permission::CreateRole, Permission::RemoveRole, Permission::ChangeMemberRole
        };
    }

    bool hasPermission(Permission permission) const override {
        return permissions_.find(permission) != permissions_.end();
    }

    std::string name() const override {
        return "Admin";
    }

    std::unordered_set<Permission> permissions() const override {
        return permissions_;
    }

private:
    std::unordered_set<Permission> permissions_;
};

// Derived Class: Member Role
class MemberRole : public Role {
public:
    MemberRole() {
        permissions_ = {
            Permission::SendFile, Permission::SendTextMessage, Permission::React, Permission::Reply,
            Permission::Call, Permission::Edit, Permission::AddMember
        };
    }

    bool hasPermission(Permission permission) const override {
        return permissions_.find(permission) != permissions_.end();
    }

    std::string name() const override {
        return "Member";
    }

    std::unordered_set<Permission> permissions() const override {
        return permissions_;
    }

private:
    std::unordered_set<Permission> permissions_;
};

// Derived Class: Banned Role
class BannedRole : public Role {
public:
    BannedRole() {
        permissions_ = {};
    }

    bool hasPermission(Permission permission) const override {
        return permissions_.find(permission) != permissions_.end();
    }

    std::string name() const override {
        return "Banned";
    }

    std::unordered_set<Permission> permissions() const override {
        return permissions_;
    }

private:
    std::unordered_set<Permission> permissions_;
};

// Derived Class: Custom Role
class CustomRole : public Role {
public:
    explicit CustomRole(const std::string& n, const std::unordered_set<Permission>& customPermissions)
        : name_(n), permissions_(customPermissions) {}

    bool hasPermission(Permission permission) const override {
        return permissions_.find(permission) != permissions_.end();
    }

    std::string name() const override {
        return name_;
    }

    std::unordered_set<Permission> permissions() const override {
        return permissions_;
    }

private:
    std::string name_;
    std::unordered_set<Permission> permissions_;
};

}