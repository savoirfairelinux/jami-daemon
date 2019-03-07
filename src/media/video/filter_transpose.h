#pragma once

#include "../media_filter.h"

namespace ring {
namespace video {

std::unique_ptr<MediaFilter>
getTransposeFilter(int rotation, std::string inputName, int width, int height, int format, bool rescale);

}
}
