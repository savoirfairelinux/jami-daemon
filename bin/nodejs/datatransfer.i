/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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
%apply uint32_t { libjami::DataTransferEventCode };
%apply uint32_t { libjami::DataTransferError };
%apply uint64_t { libjami::DataTransferId };
%apply uint64_t { const libjami::DataTransferId };
%apply int64_t& INOUT { libjami::DataTransferId& id };

%header %{
#include "jami/jami.h"
#include "jami/datatransfer_interface.h"

class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const std::string& accountId, const std::string& conversationId, const std::string& interactionId, const std::string& fileId, int eventCode){}
};
%}

%feature("director") DataTransferCallback;

%typemap(in) std::string& OUTPUT (std::string temp) {
  if (!$input.IsEmpty()) {
    SWIGV8_THROW_EXCEPTION(SWIGV8_STRING_NEW("array null"));
    return;
  }
  if (SWIGV8_ARRAY::Cast($input)->Length()) {
    SWIGV8_THROW_EXCEPTION(SWIGV8_STRING_NEW("Array must contain at least 1 element"));
  }
  $1 = &temp;
}
%typemap(argout) std::string& OUTPUT {
  auto value = SWIGV8_STRING_NEW(temp$argnum.c_str());
  SWIGV8_ARRAY_SET(SWIGV8_ARRAY::Cast($input), 0, value);
}
%apply std::string& OUTPUT { std::string& path_out }
%apply int64_t& OUTPUT { int64_t& total_out }
%apply int64_t& OUTPUT { int64_t& progress_out }


namespace libjami {

  struct DataTransferInfo
  {
    std::string accountId;
    libjami::DataTransferEventCode lastEvent;
    uint32_t flags;
    int64_t totalSize;
    int64_t bytesProgress;
    std::string author;
    std::string peer;
    std::string conversationId;
    std::string displayName;
    std::string path;
    std::string mimetype;
  };

  // Files Management 
  void sendFile(const std::string& accountId, const std::string& conversationId, const std::string& path, const std::string& displayName, const std::string& replyTo);
  uint64_t downloadFile(const std::string& accountId, const std::string& conversationId, const std::string& interactionId,const std::string& fileId, const std::string& path);
  libjami::DataTransferError cancelDataTransfer(const std::string& accountId, const std::string& conversationId, const std::string& fileId);
  libjami::DataTransferError fileTransferInfo(const std::string& accountId, const std::string& conversationId, const std::string& fileId, std::string &path_out, int64_t &total_out, int64_t &progress_out);
}


class DataTransferCallback {
public:
    virtual ~DataTransferCallback(){}
    virtual void dataTransferEvent(const std::string& accountId, const std::string& conversationId, const std::string& interactionId, const std::string& fileId, int eventCode){}
};
