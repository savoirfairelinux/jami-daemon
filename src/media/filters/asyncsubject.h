#pragma once
#include "asyncpublisher.h"
#include "reactive_streams.h"
template <class T> class AsyncSubject : public AsyncPublisher<T>, public  Subscriber<T> {
public:
    ~AsyncSubject();

    virtual void onSubscribe(std::shared_ptr<Subscription<T>>&& subscription) override { (void)subscription; }
    virtual void onNext(T t) override;
    virtual void onComplete() override;
};

template <class T>
void AsyncSubject<T>::onNext(T value){
    std::lock_guard<std::mutex> lock(this->mut);
    for(auto& weakSubscription : this->subscriptions){
        if (auto subscription = weakSubscription.lock()) {
            subscription->onNext(value);
        }
    }
}

template <class T>
void AsyncSubject<T>::onComplete(){
    std::lock_guard<std::mutex> lock(this->mut);
    for(auto& weakSubscription : this->subscriptions){
        if (auto subscription = weakSubscription.lock())
            subscription->onComplete();
    }
}

template <class T>
AsyncSubject<T>::~AsyncSubject(){
    onComplete();
}
