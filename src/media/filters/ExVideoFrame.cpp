#include "ExVideoFrame.h"

namespace jami {
ExVideoFrame::ExVideoFrame(uint8_t* data, int f ,int w, int h){
    ptr = data;
    setGeometry(f,w,h);
}

int ExVideoFrame::width() const
{
    return width_;
}

int ExVideoFrame::height() const
{
    return height_;
}

int ExVideoFrame::format() const
{
    return format_;
}

void ExVideoFrame::setGeometry(int f, int w, int h){
    format_ = f;
    width_ = w;
    height_ = h;
}

std::uint8_t* ExVideoFrame::pointer(){
    return ptr;
}
}

