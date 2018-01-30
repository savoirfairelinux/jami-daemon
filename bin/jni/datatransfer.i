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

%apply int32_t { DRing::DataTransferEventCode };
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
      bool isOutgoing;
      DRing::DataTransferEventCode lastEvent;
      std::size_t totalSize {0};
      std::streamsize bytesProgress {0};
      std::string displayName;
      std::string path;
      std::string accountId;
      std::string peer;
  };

  void acceptFileTransfer(const DRing::DataTransferId id, const std::string &file_path, std::size_t offset);
  void cancelDataTransfer(const DRing::DataTransferId id);
  std::streamsize dataTransferBytesProgress(const DRing::DataTransferId id);
  DRing::DataTransferInfo dataTransferInfo(const DRing::DataTransferId id) throw(std::invalid_argument);
  /* std::vector<uint64_t> dataTransferList(); */
  DRing::DataTransferId sendFile(const std::string &account_id, const std::string &peer_uri, const std::string &file_path, const std::string &display_name) throw(std::invalid_argument, std::runtime_error);

}

class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const DRing::DataTransferId transferId, int eventCode){}
};
