/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

using JamiDBusDataTransferInfo = sdbus::Struct<std::string, uint32_t, uint32_t, int64_t, int64_t, std::string, std::string, std::string, std::string>;

auto
dataTransferList() -> decltype(DRing::dataTransferList())
{
    return DRing::dataTransferList();
}

std::tuple<uint32_t, uint64_t>
sendFile(const JamiDBusDataTransferInfo& in)
{
    DRing::DataTransferInfo info;
    info.accountId = in.get<0>();
    info.lastEvent = DRing::DataTransferEventCode(in.get<1>());
    info.flags = in.get<2>();
    info.totalSize = in.get<3>();
    info.bytesProgress = in.get<4>();
    info.peer = in.get<5>();
    info.displayName = in.get<6>();
    info.path = in.get<7>();
    info.mimetype = in.get<8>();

    DRing::DataTransferId id;
    auto error = static_cast<uint32_t>(DRing::sendFile(info, id));
    return std::make_tuple(error, id);
}

std::tuple<uint32_t, JamiDBusDataTransferInfo>
dataTransferInfo(const uint64_t& dataTransferId)
{
    DRing::DataTransferInfo info;
    JamiDBusDataTransferInfo out;
    auto error = DRing::dataTransferInfo(dataTransferId, info);
    if (error == DRing::DataTransferError::success) {
        out = sdbus::make_struct(info.accountId,
                                 uint32_t(info.lastEvent),
                                 info.flags,
                                 info.totalSize,
                                 info.bytesProgress,
                                 info.peer,
                                 info.displayName,
                                 info.path,
                                 info.mimetype );
    }

    return std::make_tuple(static_cast<uint32_t>(error), out);
}

std::tuple<uint32_t, int64_t, int64_t>
dataTransferBytesProgress(const uint64_t& id)
{
    int64_t total = 0;
    int64_t progress = 0;
    auto error = static_cast<uint32_t>(DRing::dataTransferBytesProgress(id, total, progress));
    return std::make_tuple(error, total, progress);
}

uint32_t
acceptFileTransfer(const uint64_t& id, const std::string& file_path,
                                             const int64_t& offset)
{
    return uint32_t(DRing::acceptFileTransfer(id, file_path, offset));
}

uint32_t
cancelDataTransfer(const uint64_t& id)
{
    return uint32_t(DRing::cancelDataTransfer(id));
}
