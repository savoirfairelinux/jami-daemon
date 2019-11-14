/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
#pragma once
#include "streamdata.h"
#include "observer.h"
#include <libavutil/frame.h>
#include <string>
#include <memory>

namespace jami {

using avSubjectPtr = std::shared_ptr<Observable<AVFrame*>>;

/**
 * @brief The MediaHandler class
 * Is the main object of the plugin
 */
class MediaHandler{

public:
    virtual ~MediaHandler() = default;

    std::string id() const { return id_;}
    void setId(const std::string& id) {id_ = id;}
private:
    std::string id_;
};

/**
 * @brief The CallMediaHandler class
 * It can hold multiple streams of data, and do processing on them
 */
class CallMediaHandler: public MediaHandler {
public:
    virtual void notifyAVFrameSubject(const StreamData& data, avSubjectPtr subject) = 0;
};
}
