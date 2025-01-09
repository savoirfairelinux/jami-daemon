#include "permissions.h"

#include <exception>

namespace jami {

std::string permissionToString(Permission permission) {
    return permissions.at(permission);
}

Permission stringToPermission(const std::string& str) {
    for (const auto& [permission, name] : permissions) {
        if (name == str) {
            return permission;
        }
    }
    throw std::invalid_argument("Invalid permission string");
}

std::vector<std::string> permissionToStrings() {
    std::vector<std::string> result;
    result.reserve(permissions.size());
    for (const auto& [_, str] : permissions) {
        result.push_back(str);
    }
    return result;
}

} // namespace jami