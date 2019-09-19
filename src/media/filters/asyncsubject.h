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
    typename std::list<std::weak_ptr<AsyncSubscription<T>>> ::iterator it;
    for (it = this->subscriptions.begin(); it != this->subscriptions.end(); ) {
        auto subscription = it->lock();
        if (subscription) {
            subscription->onNext(value);
            ++it;
        } else {
            // Erase returns the it++
            it = this->subscriptions.erase(it);
        }
    }
}

template <class T>
void AsyncSubject<T>::onComplete(){
    std::lock_guard<std::mutex> lock(this->mut);
    typename std::list<std::weak_ptr<AsyncSubscription<T>>> ::iterator it;
    for (it = this->subscriptions.begin(); it != this->subscriptions.end(); ) {
        auto subscription = it->lock();
        if (subscription) {
            subscription->onComplete();
            ++it;
        } else {
            // Erase returns the it++
            it = this->subscriptions.erase(it);
        }
    }
}

template <class T>
AsyncSubject<T>::~AsyncSubject(){
    onComplete();
}
