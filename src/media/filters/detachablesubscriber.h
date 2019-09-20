#pragma once

#include "reactive_streams.h"

template <class T>
class DetachableSubscriber : public Subscriber<T>{

public:
    /**
     * @brief unsubscribe
     * Unsubscribes the plugin from a stream
     */
    virtual void unsubscribe(){
        if(this->subscription) {
            this->subscription->cancel();
        }
    }

protected:
    std::shared_ptr<Subscription<T>> subscription = nullptr;
};
