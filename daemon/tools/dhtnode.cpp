/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "dhtcpp/dhtrunner.h"
#include "dhtcpp/dht.h"

#include "logger.h"

#include <sys/socket.h>

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>

using namespace dht;

int
main(int argc, char **argv)
{
    setDebugMode(true);
    setConsoleLog(true);

    if (argc < 2) {
        SFL_ERR("Entrez un port");
        std::terminate();
    }

    int i = 1;
    int p = atoi(argv[i++]);
    if (p <= 0 || p >= 0x10000) {
        SFL_ERR("Port invalide : %d", p);
        std::terminate();
    }
    in_port_t port = p;

    std::vector<sockaddr_storage> bootstrap_nodes {};
    while (i < argc) {
        addrinfo hints{};
        addrinfo *info = nullptr, *infop = nullptr;
        hints.ai_socktype = SOCK_DGRAM;
        /*if(!ipv6)
            hints.ai_family = AF_INET;
        else if(!ipv4)
            hints.ai_family = AF_INET6;
        else
            hints.ai_family = 0;*/
        int rc = getaddrinfo(argv[i], argv[i + 1], &hints, &info);
        if(rc != 0) {
            SFL_ERR("getaddrinfo: %s", gai_strerror(rc));
            std::terminate();
        }

        i++;
        if(i >= argc)
            break;

        infop = info;
        while(infop) {
            sockaddr_storage tmp;
            memcpy(&tmp, infop->ai_addr, infop->ai_addrlen);
            bootstrap_nodes.push_back(tmp);
            infop = infop->ai_next;
        }
        freeaddrinfo(info);

        i++;
    }

    gnutls_global_init();

    DhtRunner dht;
    dht.run(port, dht::crypto::generateIdentity(), [](dht::Dht::Status ipv4, dht::Dht::Status ipv6) {
        std::cout << (int)ipv4 << (int)ipv6 << std::endl;
    }, true);

    dht.bootstrap(bootstrap_nodes);

#if 0
    Dht::InfoHash id = Dht::InfoHash::get("coucou");

    i = 0;
    while (true)
    {
        std::vector<uint8_t> data (16,i+1);
        dht.put(id, Dht::Value{data}, [i](bool ok) {
            std::cout << "Announce done ! %d" << std::hex << (i+1) << std::dec << std::endl;
        });

        i++;
        std::this_thread::sleep_for( std::chrono::milliseconds( 2000 ) );
    }
#else
    while (true)
    {
        std::string line;
        std::getline(std::cin, line);
        std::istringstream iss(line);
        std::string op, idstr, value;
        iss >> op >> idstr;

        if (op == "x") {
            break;
        }

        dht::InfoHash id {idstr};

        if (op == "g") {
            dht.get(id, [](const std::vector<std::shared_ptr<Value>>& values) {
                std::cout << "Get - found values : " << std::endl;
                for (const auto& a : values) {
                    std::cout << "\t" << *a << std::endl;
                }
                return true;
            });
        }
        else if (op == "p") {
            std::string v;
            iss >> v;
            dht.put(id, dht::Value {
                dht::ValueType::USER_DATA.id,
                std::vector<uint8_t> {v.begin(), v.end()}
            }, [](bool ok) {
                std::cout << "Put done !" << ok << std::endl;
            });
        }
        else if (op == "e") {
            std::string tostr;
            std::string v;
            iss >> tostr >> v;
            dht.putEncrypted(dht.getId(), tostr, dht::Value {
                dht::ValueType::USER_DATA.id,
                std::vector<uint8_t> {v.begin(), v.end()}
            }, [](bool ok) {
                std::cout << "Put encrypted done !" << ok << std::endl;
            });
        }
        else if (op == "a") {
            in_port_t port;
            iss >> port;
            dht.put(id, dht::Value {dht::ServiceAnnouncement::TYPE.id, dht::ServiceAnnouncement(port)}, [](bool ok) {
                std::cout << "Announce done !" << ok << std::endl;
            });
        }
    }
#endif

    dht.join();

    gnutls_global_deinit();

    return 0;
}
