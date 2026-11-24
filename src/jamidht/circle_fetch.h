#pragma once

#include "jami/def.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace dhtnet {
class ChannelSocketInterface;
}

namespace jami {

/**
 * Downloads a peer's main for a reserved circle route (circles/v1/<id> or channels/v1/<id>)
 * into ref of the local bare repository, over an already connected git channel.
 * Downloaded objects are not trusted: callers must validate the returned head.
 */
LIBJAMI_TEST_EXPORT std::optional<std::string> downloadCircleRoute(
    const std::filesystem::path& repository,
    std::string_view device,
    std::string_view route,
    const std::shared_ptr<dhtnet::ChannelSocketInterface>& socket,
    std::string_view ref);

} // namespace jami
