/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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
#include "restvideomanager.h"
#include "client/videomanager.h"

RestVideoManager::RestVideoManager() :
    resources_()
{
    populateResources();
}

std::vector<std::shared_ptr<restbed::Resource>>
RestVideoManager::getResources()
{
    return resources_;
}

// Private

std::map<std::string, std::string>
RestVideoManager::parsePost(const std::string& post)
{
    std::map<std::string, std::string> data;

    auto split = [](const std::string& s, char delim){
        std::vector<std::string> v;
        auto i = 0;
        auto pos = s.find(delim);
        while (pos != std::string::npos)
        {
            v.push_back(s.substr(i, pos-i));
            i = ++pos;
            pos = s.find(delim, pos);

            if (pos == std::string::npos)
                v.push_back(s.substr(i, s.length()));
        }

        return v;
    };

    if(post.find_first_of('&') != std::string::npos)
    {
        std::vector<std::string> v = split(post, '&');

        for(auto& it : v)
        {
            std::vector<std::string> tmp = split(it, '=');
            data[tmp.front()] = tmp.back();
        }
    }
    else
    {
        std::vector<std::string> tmp = split(post, '=');
        data[tmp.front()] = tmp.back();
    }

    return data;
}

void
RestVideoManager::populateResources()
{
    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/videoManager");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestVideoManager::defaultRoute, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/deviceList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestVideoManager::getDeviceList, this, std::placeholders::_1));
}

void
RestVideoManager::defaultRoute(const std::shared_ptr<restbed::Session> session)
{
    JAMI_INFO("[%s] GET /videoManager", session->get_origin().c_str());

    std::string body = "Available routes are : \r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void RestVideoManager::getDeviceList(const std::shared_ptr<restbed::Session> session)
{
    JAMI_INFO("[%s] GET /deviceList", session->get_origin().c_str());
    std::vector<std::string> list = DRing::getDeviceList();

    std::string body = "";

    for(auto& it : list)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);

}

void RestVideoManager::getCapabilities(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::getSettings(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::applySettings(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::setDefaultDevice(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::getDefaultDevice(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::startCamera(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::stopCamera(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::switchInput(const std::shared_ptr<restbed::Session> session)
{

}

void RestVideoManager::hasCameraStarted(const std::shared_ptr<restbed::Session> session)
{

}
