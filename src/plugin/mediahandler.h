#pragma once
#include <string>
#include "media/filters/detachablesubscriber.h"
#include "media/filters/syncsubject.h"

#include "streamdata.h"
//FFMPEG
#include <libavutil/frame.h>

namespace jami {

using avSubjectPtr = std::weak_ptr<SyncSubject<AVFrame*>>;

/**
 * @brief The MediaHandler class
 * Is the main object of the plugin
 */
class MediaHandler{

public:
    virtual ~MediaHandler() = default;

    std::string id() const { return id_;}
    void setId(const std::string& id) {id_ = id;}
    virtual void unregister() = 0;
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
