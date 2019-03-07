#pragma once

#include "../media_filter.h"

namespace ring {
namespace video {

std::unique_ptr<MediaFilter>
getTransposeFilter(int rotation, std::string inputName, int width, int height, int format, bool rescale)
{
    RING_WARN("Rotation set to %d", rotation);
    if (!rotation) {
        return {};
    }

    std::stringstream ss;
    ss << "[" << inputName << "] ";

    switch (rotation) {
        case 90 :
        case -270 :
            ss << "transpose=2";
            if (rescale)
              ss << ",scale=w=-1:h=" << width;
            break;
        case 180 :
        case -180 :
            ss << "rotate=PI";
            break;
        case 270 :
        case -90 :
            ss << "transpose=1";
            if (rescale)
              ss << ",scale=w=-1:h=" << width;
            break;
        default :
            ss << "null";
    }

    //const auto format = AV_PIX_FMT_RGB32;
    const auto one = rational<int>(1);
    std::vector<MediaStream> msv;
    msv.emplace_back(inputName, format, one, width, height, one, one);

    std::unique_ptr<MediaFilter> filter(new MediaFilter);
    auto ret = filter->initialize(ss.str(), msv);
    if (ret < 0) {
        RING_ERR() << "filter init fail";
        return {};
    }
    return filter;
}

}
}
