#pragma once

#include <cstdint>
#include <memory>

namespace jami {
class ExVideoFrame {
public:
  ExVideoFrame(uint8_t *data, int f, int w, int h, int ls) {
      ptr = data;
      linesize_ = ls;
      setGeometry(f, w, h);
  }

  int width() const { return width_; }

  int height() const { return height_; }

  int format() const { return format_; }

  int linesize() const { return linesize_; }

  uint8_t *pointer() { return ptr; }

private:
  int width_;
  int height_;
  int format_;
  int linesize_;

  uint8_t *ptr;

private:
    void setGeometry(int f, int w, int h){
        format_ = f;
        width_ = w;
        height_ = h;
    }
};

} // namespace jami

