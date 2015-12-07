/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

// Project
#include "datatransfer.h"
#include "noncopyable.h"
#include "intrin.h"

namespace ring {

class FileTransfer : public DataTransfer
{
public:
    explicit FileTransfer(const std::string& filename);

    ~FileTransfer();

    std::string getFilename() const noexcept {
        return filename_;
    }

    std::size_t read(void* buffer, std::size_t size) override;

    std::size_t write(void* buffer UNUSED, std::size_t size UNUSED) override {
        // not supported yet
        return 0;
    }

private:
    NON_COPYABLE(FileTransfer);
    const std::string filename_;
    void* handle_;
};

} // namespace ring
