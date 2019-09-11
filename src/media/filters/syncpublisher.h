#pragma once
#include "reactive_streams.h"
#include "syncsubscription.h"
//Std libs
#include <vector>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <list>


template <class T>  class SyncPublisher : public Publisher<T>{
public:
    void subscribe(Subscriber<T>& subscriber) override;

protected:
    std::mutex mut;
    // List of subscriptions
    std::list<std::weak_ptr<SyncSubscription<T>>> subscriptions;
};


template <class T>
void SyncPublisher<T>::subscribe(Subscriber<T>& subscriber){
    auto subscription = std::make_shared<SyncSubscription<T>>(subscriber);
    std::lock_guard<std::mutex> lock(mut);
    std::cout << "Publisher: " << "New Subscriber"<<std::endl;
    subscriptions.emplace_back(subscription);
    try {
        subscriber.onSubscribe(subscription);
    } catch(const std::exception& e) { // Due diligence to obey 2.13
        std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onSubscribe." << std::endl;

        subscription->cancel();

        try {
            subscriber.onError(e); // Then we signal the error downstream, to the `Subscriber`
        } catch(const std::exception& e2) { // If `onError` throws an exception, this is a spec violation according to rule 1.9, and all we can do is to log it.
            std::string e2description = e2.what();
            std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onError. " << std::endl;
            throw e2;
        }
    }
}
