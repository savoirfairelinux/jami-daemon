#pragma once
#include "reactive_streams.h"
#include <climits>

template <class T>
class SyncSubscription : public Subscription<T>{
public:
    void request(unsigned long long  n);
    void cancel() {cancelled = true;}

public:
    SyncSubscription(Subscriber<T>& s):
        subscriber{s}{}
    virtual ~SyncSubscription() = default;
    void onNext(T value);
    void onComplete();

private:
    void terminateDueTo(const std::exception& e);

private:
    Subscriber<T>& subscriber; // We need a reference to the `Subscriber` so we can talk to it

    unsigned long long demand = 0; // Here we track the current demand, i.e. what has been requested but not yet delivered
    // cancelled is set to true when a Cancel signal is received in the queue and processed by processor
    bool cancelled = false;
    // completed is set to true when the publisher has finished sending its data
    bool completed = false;

};


template <class T>
void SyncSubscription<T>::onNext(T value){
    if(!cancelled && !completed && demand > 0) {
        try{
            subscriber.onNext(value);
            demand--;
        } catch(const std::exception& e) {
            cancelled = true;
            std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onNext. " << std::endl;
            std::cerr << e.what() << std::endl;
        }
    }
}


template <class T>
void SyncSubscription<T>::request(unsigned long long  n){
    if (n < 1)
        terminateDueTo(std::invalid_argument("subscriber violated the Reactive Streams rule 3.9 by requesting a non-positive number of elements."));
    else if (demand + n < demand) {
        // As governed by rule 3.17, when demand overflows `Long.MAX_VALUE` we treat the signalled demand as "effectively unbounded"
        demand = ULLONG_MAX;  // Here we protect from the overflow and treat it as "effectively unbounded"
    } else {
        demand += n; // Here we record the downstream demand
    }
}


template <class T>
void SyncSubscription<T>::terminateDueTo(const std::exception& e){
    cancelled = true; // When we signal onError, the subscription must be considered as cancelled, as per rule 1.6
    try {
        subscriber.onError(e); // Then we signal the error downstream, to the `Subscriber`
    } catch(const std::exception& e2) { // If `onError` throws an exception, this is a spec violation according to rule 1.9, and all we can do is to log it.
        std::string e2description = e2.what();
        std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onError. " << std::endl;
        throw e2;
    }
}

template <class T>
void SyncSubscription<T>::onComplete(){
    if(!completed) {
        completed = true;
        try{
            subscriber.onComplete();
        } catch(const std::exception& e) {
            cancelled = true;
            std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onComplete. " << std::endl;
            std::cerr << e.what() << std::endl;
        }
    }
}
