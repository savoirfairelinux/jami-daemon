/*
 *  Copyright (C) 2004-2017 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Pierre Duchemin <pierre.duchemin@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

%apply uint32_t { DRing::DataTransferEventCode };
%apply uint32_t { DRing::DataTransferError };
%apply uint64_t { DRing::DataTransferId };
%apply uint64_t { const DRing::DataTransferId };

%header %{
#include "dring/dring.h"
#include "dring/datatransfer_interface.h"

class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const DRing::DataTransferId transferId, int eventCode){}
};
%}

%feature("director") DataTransferCallback;

namespace DRing {

  struct DataTransferInfo
  {
    std::string accountId;
    DRing::DataTransferEventCode lastEvent;
    uint32_t flags;
    int64_t totalSize;
    int64_t bytesProgress;
    std::string peer;
    std::string displayName;
    std::string path;
    std::string mimetype;
  };

  DRing::DataTransferError sendFile(const DRing::DataTransferInfo info, DRing::DataTransferId id);
  DRing::DataTransferError acceptFileTransfer(const DRing::DataTransferId id, const std::string file_path, int64_t offset);
  DRing::DataTransferError cancelDataTransfer(const DRing::DataTransferId id);
  DRing::DataTransferError dataTransferInfo(const DRing::DataTransferId id, DRing::DataTransferInfo &info);
  DRing::DataTransferError dataTransferBytesProgress(const DRing::DataTransferId id, int64_t &total, int64_t &progress);

}

class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const DRing::DataTransferId transferId, int eventCode){}
};
