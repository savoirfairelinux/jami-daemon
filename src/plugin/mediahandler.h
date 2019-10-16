#pragma once
#include "streamdata.h"
#include "observer.h"
//FFMPEG
#include <libavutil/frame.h>
// STL
#include <string>
#include <memory>

namespace jami {

using avSubjectPtr = std::shared_ptr<Observable<AVFrame*>>;

/**
 * @brief The MediaHandler class
 * Is the main object of the plugin
 */
class MediaHandler{

public:
    virtual ~MediaHandler() = default;

    std::string id() const { return id_;}
    void setId(const std::string& id) {id_ = id;}
private:
    std::string id_;
};

/**
 * @brief The MediaStreamHandler class
 * It can hold multiple streams of data, and do processing on them
 */
class MediaStreamHandler: public MediaHandler {
public:
    virtual void notifyAVFrameSubject(const StreamData& data, avSubjectPtr subject) = 0;
};
}
