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
    std::string accountId; ///< Identifier of the emiter/receiver account
    DRing::DataTransferEventCode lastEvent; ///< Latest event code sent to the user

    // todo: should be uint32_t !!!
    std::bitset<32> flags; ///< Transfer global information. 0 reserved for invalid structure. ABI is equivalent to an uint32_t.
    int64_t totalSize; ///< Total number of bytes to sent/receive, 0 if not known
    int64_t bytesProgress; ///< Number of bytes sent/received
    std::string peer; ///< Identifier of the remote peer (in the semantic of the associated account)
    std::string displayName; ///< Human oriented transfer name
    std::string path; ///< associated local file path if supported (empty, if not)
    std::string mimetype; ///< MimeType of transfered data (https://www.iana.org/assignments/media-types/media-types.xhtml)
  };

  DRing::DataTransferError sendFile(const DRing::DataTransferInfo info, DRing::DataTransferId id);
  DRing::DataTransferError acceptFileTransfer(const DRing::DataTransferId id, const std::string file_path, int64_t offset);
  DRing::DataTransferError cancelDataTransfer(const DRing::DataTransferId id);
  DRing::DataTransferError dataTransferInfo(const DRing::DataTransferId id, DRing::DataTransferInfo info);
  DRing::DataTransferError dataTransferBytesProgress(const DRing::DataTransferId id, int64_t total, int64_t progress);

}

class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const DRing::DataTransferId transferId, int eventCode){}
};
