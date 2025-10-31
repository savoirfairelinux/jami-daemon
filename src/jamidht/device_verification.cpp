/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
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

#include "device_verification.h"
#include "logger.h"

#include <dhtnet/multiplexed_socket.h>
#include <gnutls/gnutls.h>
#include <algorithm>
#include <cstring>

namespace jami {

// Carefully curated emoji palette for device verification
// Criteria: easily distinguishable, widely supported, culturally neutral, high contrast
const std::vector<std::string> DeviceVerification::EMOJI_PALETTE = {
    // Animals & Nature
    "🐶",
    "🐱",
    "🐭",
    "🐹",
    "🐰",
    "🦊",
    "🐻",
    "🐼",
    "🐨",
    "🐯",
    "🦁",
    "🐮",
    "🐷",
    "🐸",
    "🐵",
    "🐔",
    "🐧",
    "🐦",
    "🐤",
    "🦆",
    "🦅",
    "🦉",
    "🦇",
    "🐺",
    "🐗",
    "🐴",
    "🦄",
    "🐝",
    "🐛",
    "🦋",
    "🐌",
    "🐞",
    "🐢",
    "🐍",
    "🦎",
    "🦀",
    "🦞",
    "🦑",
    "🐙",
    "🦈",
    "🐬",
    "🐳",
    "🐋",
    "🐊",
    "🐆",
    "🐅",
    "🐃",
    "🐂",

    // Food & Drink
    "🍎",
    "🍊",
    "🍋",
    "🍌",
    "🍉",
    "🍇",
    "🍓",
    "🍈",
    "🍒",
    "🍑",
    "🥭",
    "🍍",
    "🥥",
    "🥝",
    "🍅",
    "🥑",
    "🍆",
    "🥦",
    "🥬",
    "🥒",
    "🌽",
    "🥕",
    "🥔",
    "🍠",
    "🌰",
    "🥜",
    "🍞",
    "🥐",
    "🥖",
    "🥨",
    "🧀",
    "🥚",
    "🍗",
    "🍖",
    "🦴",
    "🌭",
    "🍔",
    "🍟",
    "🍕",
    "🥪",

    // Activities & Objects
    "⚽",
    "🏀",
    "🏈",
    "⚾",
    "🥎",
    "🎾",
    "🏐",
    "🏉",
    "🥏",
    "🎱",
    "🏓",
    "🏸",
    "🏒",
    "🏑",
    "🥍",
    "🏏",
    "⛳",
    "🏹",
    "🎣",
    "🥊",
    "🥋",
    "🎽",
    "🛹",
    "🛷",
    "⛸️",
    "🥌",
    "🎿",
    "⛷️",
    "🏂",
    "🏋️",
    "🤺",
    "🤸",
    "⛹️",
    "🤾",
    "🏌️",
    "🏇",
    "🧘",
    "🏄",
    "🏊",
    "🤽",

    // Symbols & Shapes
    "❤️",
    "🧡",
    "💛",
    "💚",
    "💙",
    "💜",
    "🖤",
    "🤍",
    "🤎",
    "💔",
    "❣️",
    "💕",
    "💞",
    "💓",
    "💗",
    "💖",
    "💘",
    "💝",
    "⭐",
    "🌟",
    "✨",
    "⚡",
    "🔥",
    "💥",
    "☀️",
    "🌙",
    "⭐",
    "🌈",
    "☁️",
    "⛅",
    "⛈️",
    "🌩️",
    "🌧️",
    "☔",
    "⛄",
    "❄️",
    "🌬️",
    "💨",
    "🌪️",
    "🌊",
};

const std::vector<std::string>&
DeviceVerification::getEmojiPalette()
{
    return EMOJI_PALETTE;
}

std::vector<uint8_t>
DeviceVerification::extractVerificationMaterial(const std::shared_ptr<dhtnet::ChannelSocket>& channel,
                                                const std::string& device1Id,
                                                const std::string& device2Id,
                                                uint64_t operationId,
                                                size_t length)
{
    if (!channel) {
        JAMI_ERROR("[DeviceVerification] Invalid channel socket");
        return {};
    }

    try {
        // RFC 5705 label for Jami device verification
        // Using a unique label ensures this keying material is bound to this specific purpose
        static const std::string TLS_EXPORT_LABEL = "EXPORTER-Jami-Device-Verification";

        // Build context from device IDs and operation ID for defense-in-depth binding.
        // Both sides must compute the same context, so we use canonical ordering.
        std::string devA = device1Id;
        std::string devB = device2Id;
        if (devA > devB)
            std::swap(devA, devB); // Ensure consistent ordering

        std::string context = devA + ":" + devB + ":" + std::to_string(operationId);

        JAMI_DEBUG("[DeviceVerification] Exporting keying material with context: {}", context);

        // Export keying material using RFC 5705
        // The exported material is cryptographically bound to:
        //   1. This specific TLS session (via master secret)
        //   2. This specific purpose (via label)
        //   3. These specific devices and operation (via context)
        //
        // Both sides of a direct TLS connection will derive identical material.
        // A MITM attacker creating two separate TLS sessions will produce different
        // material on each side, allowing detection via emoji mismatch.
        auto material = channel->exportKeyingMaterial(TLS_EXPORT_LABEL, context, length);

        if (material.empty()) {
            JAMI_ERROR("[DeviceVerification] Failed to export TLS keying material");
            return {};
        }

        if (material.size() != length) {
            JAMI_WARNING("[DeviceVerification] Exported material size ({}) doesn't match requested ({})",
                         material.size(),
                         length);
        }

        JAMI_DEBUG("[DeviceVerification] Successfully extracted {} bytes of RFC 5705 keying material", material.size());
        return material;

    } catch (const std::exception& e) {
        JAMI_ERROR("[DeviceVerification] Exception while extracting verification material: {}", e.what());
        return {};
    }
}

std::vector<std::string>
DeviceVerification::generateEmojiSequence(const std::vector<uint8_t>& keyingMaterial, size_t emojiCount)
{
    std::vector<std::string> emojiSequence;

    if (keyingMaterial.empty()) {
        JAMI_ERROR("[DeviceVerification] Empty keying material provided");
        return emojiSequence;
    }

    if (emojiCount == 0 || emojiCount > 10) {
        JAMI_WARNING("[DeviceVerification] Invalid emoji count {}, using default 6", emojiCount);
        emojiCount = 6;
    }

    const size_t paletteSize = EMOJI_PALETTE.size();
    if (paletteSize == 0) {
        JAMI_ERROR("[DeviceVerification] Empty emoji palette");
        return emojiSequence;
    }

    // We need at least 2 bytes per emoji to have good distribution
    size_t bytesNeeded = emojiCount * 2;
    if (keyingMaterial.size() < bytesNeeded) {
        JAMI_WARNING("[DeviceVerification] Keying material too short ({} bytes, need {})",
                     keyingMaterial.size(),
                     bytesNeeded);
        // Continue but may have reduced quality
    }

    emojiSequence.reserve(emojiCount);

    // Generate emoji sequence from keying material
    // Use 2 bytes per emoji for better distribution across palette
    for (size_t i = 0; i < emojiCount; ++i) {
        size_t offset = (i * 2) % keyingMaterial.size();

        // Combine two bytes for better randomness
        uint16_t value = 0;
        if (offset + 1 < keyingMaterial.size()) {
            value = (static_cast<uint16_t>(keyingMaterial[offset]) << 8) | keyingMaterial[offset + 1];
        } else {
            // Fallback if we're near the end
            value = static_cast<uint16_t>(keyingMaterial[offset]) << 8;
            if (keyingMaterial.size() > 0) {
                value |= keyingMaterial[0];
            }
        }

        // Map to emoji palette index
        size_t emojiIndex = value % paletteSize;
        emojiSequence.push_back(EMOJI_PALETTE[emojiIndex]);
    }

    JAMI_DEBUG("[DeviceVerification] Generated emoji sequence of {} emojis", emojiCount);
    return emojiSequence;
}

} // namespace jami
