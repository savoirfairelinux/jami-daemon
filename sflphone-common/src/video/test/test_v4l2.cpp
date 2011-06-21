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
#include "video_v4l2.h"
using namespace sfl_video;

int main()
{
    unsigned idx;
    VideoV4l2List list;
    std::vector<std::string> v = list.getDeviceList();
    idx = list.getDeviceIndex();
    list.setDevice(idx);

    size_t i, n = v.size();
    assert(idx < n);
    for (i=0; i<n; i++) {
        cout << ((idx == i) ? " * " : "   ");
        cout << v[i] << endl;
    }

    VideoV4l2Device &dev = list.getDevice();
    idx = dev.getChannelIndex();
    dev.setChannel(idx);
    v = dev.getChannelList();

    n = v.size();
    assert(idx < n);
    for (i=0; i<n; i++) {
        cout << ((idx == i) ? " * " : "   ");
        cout << "\t" << v[i] << endl;
    }

    VideoV4l2Channel &chan = dev.getChannel();
    VideoV4l2Size &size = chan.getSize();
    idx = chan.getSizeIndex();
    chan.setSize(idx);
    v = chan.getSizeList();

    n = v.size();
    assert(idx < n);
    for (i=0; i<n; i++) {
        cout << ((idx == i) ? " * " : "   ");
        cout << "\t\t" << v[i] << endl;
    }

    //VideoV4l2Rate &rate = chan.getRate();
    idx = size.getRateIndex();
    size.setRate(idx);
    v = size.getRateList();

    n = v.size();
    assert(idx < n);
    for (i=0; i<n; i++) {
        cout << ((idx == i) ? " * " : "   ");
        cout << "\t\t\t" << v[i] << endl;
    }

    return 0;
}
