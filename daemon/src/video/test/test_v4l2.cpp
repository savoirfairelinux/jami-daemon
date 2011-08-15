/*
 *  Copyright (C) 2011 Savoir-Faire Linux Inc.
 *  Author: Rafaël Carré <rafael.carre@savoirfairelinux.com>
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

#include <iostream>
#include <vector>
#include <string>
#include <cassert>

using namespace std;

#include "video_v4l2_list.h"
//#include "video_v4l2.h"
using namespace sfl_video;

int main()
{
    VideoV4l2List list;
    std::vector<std::string> devlist = list.getDeviceList();

    for (size_t i=0; i<devlist.size(); i++) {
        const std::string &dev = devlist[i];
        cout << dev << endl;

        std::vector<std::string> channellist = list.getChannelList(dev);
        for (size_t i=0; i<channellist.size(); i++) {
            const std::string &chan = channellist[i];
            cout << '\t' << chan << endl;

            std::vector<std::string> sizelist = list.getSizeList(dev, chan);
            for (size_t i=0; i<sizelist.size(); i++) {
                const std::string &size = sizelist[i];
                cout << "\t\t" << size << endl;

                std::vector<std::string> ratelist = list.getRateList(dev, chan, size);
                for (size_t i=0; i<ratelist.size(); i++) {
                    const std::string &rate = ratelist[i];
                    cout << "\t\t\t" << rate << endl;
                }
                cout << endl;
            }
            cout << endl;
        }
        cout << endl;
    }
    cout << endl;

    return 0;
}
