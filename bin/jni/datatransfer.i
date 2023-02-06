/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

%typemap(jstype) std::string& OUTPUT "String[]"
%typemap(jtype) std::string& OUTPUT "String[]"
%typemap(jni) std::string& OUTPUT "jobjectArray"
%typemap(javain)  std::string& OUTPUT "$javainput"
%typemap(in) std::string& OUTPUT (std::string temp) {
  if (!$input) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
    return $null;
  }
  if (JCALL1(GetArrayLength, jenv, $input) == 0) {
    SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
  }
  $1 = &temp;
}
%typemap(argout) std::string& OUTPUT {
  jstring jvalue = JCALL1(NewStringUTF, jenv, temp$argnum.c_str());
  JCALL3(SetObjectArrayElement, jenv, $input, 0, jvalue);
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
