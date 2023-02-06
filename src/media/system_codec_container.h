/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Eloi BAIL <eloi.bail@savoirfairelinux.com>
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

#ifndef __SYSTEM_CODEC_CONTAINER_H__
#define __SYSTEM_CODEC_CONTAINER_H__

#include "media_codec.h"
#include "ring_types.h"

#include <string>
#include <vector>
#include <memory>

namespace jami {

class SystemCodecContainer;

extern decltype(getGlobalInstance<SystemCodecContainer>)& getSystemCodecContainer;

class SystemCodecContainer
{
public:
    SystemCodecContainer();
    ~SystemCodecContainer();

    std::vector<std::shared_ptr<SystemCodecInfo>> getSystemCodecInfoList(
        MediaType mediaType = MEDIA_ALL);

    std::vector<unsigned> getSystemCodecInfoIdList(MediaType type = MEDIA_ALL);

    std::shared_ptr<SystemCodecInfo> searchCodecById(unsigned codecId, MediaType type = MEDIA_ALL);

    std::shared_ptr<SystemCodecInfo> searchCodecByName(const std::string& name,
                                                       MediaType type = MEDIA_ALL);

    std::shared_ptr<SystemCodecInfo> searchCodecByPayload(unsigned payload,
                                                          MediaType type = MEDIA_ALL);

    void removeCodecByName(const std::string& name, MediaType type = MEDIA_ALL);

    void initCodecConfig();

private:
    /* available audio & video codec  */
    std::vector<std::shared_ptr<SystemCodecInfo>> availableCodecList_;

    bool setActiveH265();
    void checkInstalledCodecs();
};

} // namespace jami

#endif // SYSTEM_CODEC_CONTAINER
