#pragma once
#include <string>

enum class StreamType { audio, video };

struct StreamData {
    StreamData(std::string i, bool d, StreamType t, std::string s) :
        id{i}, direction{d}, type{t}, source{s} {}
    const std::string id;
    const bool direction;
    const StreamType type;
    const std::string source;
};
