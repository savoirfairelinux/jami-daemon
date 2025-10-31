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
#pragma once

#include <dhtnet/multiplexed_socket.h>
#include <vector>
#include <string>
#include <memory>

namespace jami {

/**
 * DeviceVerification - Utility class for device linking verification using emoji
 * 
 * This class provides functionality to:
 * - Extract TLS keying material from a secure channel
 * - Generate a deterministic emoji sequence for device verification
 * - Support secure device linking with visual verification
 */
class DeviceVerification
{
public:
    /**
     * Extract verification material from the TLS session using RFC 5705.
     * 
     * This function exports keying material from an established TLS session using the
     * RFC 5705 keying material exporter. The exported material is cryptographically
     * bound to:
     *   1. The TLS session's master secret (session-specific)
     *   2. The specific purpose via label
     *   3. The device identities and operation (via context) for defense-in-depth
     * 
     * Both peers in a direct TLS connection will derive identical material,
     * while a man-in-the-middle attacker performing two separate TLS handshakes
     * will produce different material on each side.
     * 
     * @param channel The TLS channel socket to export material from
     * @param device1Id First device identifier (order doesn't matter, will be canonicalized)
     * @param device2Id Second device identifier (order doesn't matter, will be canonicalized)
     * @param operationId Unique identifier for this linking operation
     * @param length The number of bytes of keying material to export (default: 32)
     * @return A vector of bytes containing the exported keying material, or empty on error
     */
    static std::vector<uint8_t> extractVerificationMaterial(
        const std::shared_ptr<dhtnet::ChannelSocket>& channel,
        const std::string& device1Id,
        const std::string& device2Id,
        uint64_t operationId,
        size_t length = 32);

    /**
     * Generate emoji sequence from keying material
     * 
     * Converts cryptographic keying material into a human-friendly emoji sequence
     * for visual verification. The emoji set is carefully chosen to be easily
     * distinguishable and widely supported across platforms.
     * 
     * @param keyingMaterial The TLS keying material
     * @param emojiCount Number of emojis to generate (default 6)
     * @return Vector of emoji strings for display
     */
    static std::vector<std::string> generateEmojiSequence(
        const std::vector<uint8_t>& keyingMaterial,
        size_t emojiCount = 6);

    /**
     * Get the complete emoji palette used for verification
     * 
     * Returns the set of all possible emojis that can be used in verification.
     * The palette is designed with the following criteria:
     * - Easily distinguishable visually
     * - Widely supported across platforms
     * - Culturally neutral
     * - High contrast and clear shapes
     * 
     * @return Vector of all available emoji strings
     */
    static const std::vector<std::string>& getEmojiPalette();

private:
    // Emoji palette for verification - carefully curated for clarity and accessibility
    static const std::vector<std::string> EMOJI_PALETTE;
    
    // Label for RFC 5705 key material export
    static constexpr const char* TLS_EXPORT_LABEL = "JAMI DEVICE VERIFICATION";
    static constexpr size_t TLS_EXPORT_CONTEXT_LENGTH = 0;
};

} // namespace jami

