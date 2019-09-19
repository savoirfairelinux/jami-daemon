#pragma once
#include <string>
#include "media/filters/reactive_streams.h"

namespace jami {

/**
 * @brief The CustomPlugin class
 * Is a subscriber that can be detached using unsubscribe
 * Is linked to a specific plugin, hence the plugin id
 */
template<class T>
class CustomPlugin : public Subscriber<T>{

public:
    /**
     * @brief unsubscribe
     * Unsubscribes the plugin from a stream
     */
    void unsubscribe(){
        this->subscription->cancel();
    }

    std::string id() const { return id_;}
    void setId(const std::string &id) {id_ = id;}

protected:
    std::shared_ptr<Subscription<T>> subscription = nullptr;

private:
    std::string id_;
    //Subscription

};
}
