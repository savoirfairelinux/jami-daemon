/*
 *  Copyright (C) 2021-2023 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>
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

#include <vector>
#include <iostream>
#include <string>
#include <map>

class SIPFmt
{
public:
    SIPFmt(const std::vector<uint8_t>& data);

    bool parse(const std::vector<uint8_t>& blob);

    void pushBody(char *bytes, size_t len);


    const std::string& getField(const std::string& field) const ;
    const std::vector<uint8_t>& getBody();

    void setVersion(const std::string& version) { version_ = version; }
    void setField(const std::string& field);
    void setFieldValue(const std::string& field, const std::string& value);
    void setMethod(const std::string& method) { method_ = method; }
    void setURI(const std::string& URI) { URI_ = URI; }
    void setStatus(const std::string& status) { status_ = status; }
    void setMsg(const std::string& msg) { msg_ = msg; }

    void swapBody(std::vector<uint8_t>& body);

    bool isValid() const { return isValid_; }
    bool isRequest() const { return isValid_ and isRequest_; }
    bool isResponse() const { return isValid_ and not isRequest_; }

    bool isApplication(const std::string& app) const
    {
          return isValid() and (std::string::npos != getField("content-type").find("application/" + app));
    }

    void setAsRequest() { isRequest_ = true; };
    void setAsResponse() { isRequest_ = false; };

    void swap(std::vector<uint8_t>& with);

private:
    std::vector<uint8_t> body_ {};
    std::string status_ {};
    std::string version_ {};
    std::string msg_ {};
    std::string URI_ {};
    std::string method_ {};
    std::map<std::string, std::string> fields_ {};
    bool isValid_;
    bool isRequest_ {};
};
