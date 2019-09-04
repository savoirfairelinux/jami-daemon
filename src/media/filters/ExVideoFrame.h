#pragma once

#include <cstdint>
#include <memory>

namespace jami{
class ExVideoFrame{
public:
    ExVideoFrame(uint8_t* data, int f, int w, int h);
    int width() const;

    int height() const;

    int format() const;

    uint8_t* pointer();
private:
    int width_;
    int height_;
    int format_;

    uint8_t* ptr;
private:
    void setGeometry(int format, int width, int height);
};
}
