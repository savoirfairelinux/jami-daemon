#pragma once
#include "reactive_streams.h"
#include "reactive_streams_signals.h"
//Std libs
#include <queue>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <climits>

template <class T>
class AsyncSubscription : public Subscription<T>{
public:
    void request(unsigned long long  n);
    void cancel();
public:
    AsyncSubscription(Subscriber<T>& subscriber/*, AsyncPublishSubject<T>& publisher*/):
        subscriber{subscriber},
        processor{[this]{process();}} {}

    virtual ~AsyncSubscription();

    void onNext(T value);
    //void onError(const std::exception& e);
    void onComplete();

public:
    void terminateDueTo(const std::exception& e);
private:
    void signal(std::unique_ptr<Signal>&& signal);
    void process();

    void doRequest(unsigned long long n);
    void doCancel() {cancelled = true;}
    void doSend();

    void printProcessorTerminate();

private:
    // Locking variables
    std::mutex mut;
    std::condition_variable processorWakeUpCondition;
    std::condition_variable publisherPushCondition;

    Subscriber<T>& subscriber; // We need a reference to the `Subscriber` so we can talk to it
    //AsyncPublishSubject<T>& publisher; // Optionnal reference to the publisher
    std::queue<std::unique_ptr<Signal>> signalsQueue; // This `Queue` will track signals that are sent to this `Subscription`, like `request` and `cancel`
    unsigned long long demand = 0; // Here we track the current demand, i.e. what has been requested but not yet delivered

    // State variables

    // cancelled is set to true when a Cancel signal is received in the queue and processed by processor
    bool cancelled = false;
    // completed is set to true when the publisher has finished sending its data
    bool completed = false;
    // Threads
    std::thread processor;

    // Last Value
    std::queue<T> values;
};


template <class T>
void AsyncSubscription<T>::request(unsigned long long  n){
    signal(std::make_unique<Request>(n));
}

template <class T>
void AsyncSubscription<T>::cancel(){
    signal(std::make_unique<Cancel>());
}

template <class T>
void AsyncSubscription<T>::signal(std::unique_ptr<Signal>&& signal){
    // Check if the signal is not null
    if(signal){
        std::lock_guard<std::mutex> lk(mut);
        signalsQueue.push(std::move(signal));
        processorWakeUpCondition.notify_all();
    }
}

template <class T>
void AsyncSubscription<T>::onNext(T value){
    {
        std::lock_guard<std::mutex> lk(mut);
        values.push(std::move(value));
        if (values.size() > demand) {
            values.pop();
        }
    }
    publisherPushCondition.notify_all();
}

template <class T>
void AsyncSubscription<T>::onComplete(){
    std::lock_guard<std::mutex> lk(mut);
    completed = true;
    publisherPushCondition.notify_all();
}

template <class T>
void AsyncSubscription<T>::process(){
    auto waitConditionLambda = [this]{ return (!signalsQueue.empty()) or cancelled;};
    while (true){
        decltype(signalsQueue) signals;

        { // Necessary for the lifetime of the mutex
            std::unique_lock<std::mutex> lk(mut);
            processorWakeUpCondition.wait(lk, waitConditionLambda);
            if(cancelled){
                printProcessorTerminate();
                break;
            }
            signals = std::move(signalsQueue);
        }

        while (!signals.empty() && !cancelled) {
            if (cancelled){
                printProcessorTerminate();
                return;
            }
            std::cout << "New Signal received"<<std::endl;
            // Read the front value
            Signal* currentSignal = signals.front().get();
            // Below we simply unpack the `Signal`s and invoke the corresponding methods
            if (const Request* t = dynamic_cast<Request*>(currentSignal)) {
                // Get the value before popping
                const unsigned long long n = t->n;
                std::cout << "Processing: " << "Request("<<t->n<<")"<<std::endl;
                doRequest(n);
                std::cout << "Finished Processing: " << "Request("<<n<<")"<<std::endl;
            }
            else if (const Cancel* t = dynamic_cast<Cancel*>(currentSignal)) {
                doCancel();
            } else {
                std::cout << "Processing: " << "Unkown signal"<<std::endl;
            }
            // Pop the signals queue
            signals.pop();
        }
    }
}

template <class T>
void AsyncSubscription<T>::doRequest(unsigned long long n){
    if (n < 1)
        terminateDueTo(std::invalid_argument("subscriber violated the Reactive Streams rule 3.9 by requesting a non-positive number of elements."));
    else if (demand + n < demand) {
        // As governed by rule 3.17, when demand overflows `Long.MAX_VALUE` we treat the signalled demand as "effectively unbounded"
        demand = ULLONG_MAX;  // Here we protect from the overflow and treat it as "effectively unbounded"
        doSend(); // Then we proceed with sending data downstream
    } else {
        demand += n; // Here we record the downstream demand
        doSend(); // Then we can proceed with sending data downstream
    }
}


template <class T>
void AsyncSubscription<T>::doSend() {
    decltype(values) tmpValues {};
    { // Necessary for the lifetime of the mutex
        std::unique_lock<std::mutex> lock(mut);
        publisherPushCondition.wait(lock,  [this]{ return not values.empty() or completed;});
        tmpValues = std::move(values);
    }

    try {
        while (not tmpValues.empty() and demand > 0) {
            subscriber.onNext(std::move(tmpValues.front()));
            tmpValues.pop();
            if(demand != ULLONG_MAX) {
               demand--;
            }
        }

        if (completed) {
            JAMI_DBG() << "Video Stream complete";
            subscriber.onComplete();
        }

    } catch (const std::exception& e) {
        std::cerr << "subscriber violated the Reactive Streams rule 2.13 by throwing an exception from onNext or onComplete." << std::endl;
        terminateDueTo(e);
    }
}

template <class T>
void AsyncSubscription<T>::terminateDueTo(const std::exception& e){
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
AsyncSubscription<T>::~AsyncSubscription(){
    // cancell the subscription and stop everything
    {
        std::lock_guard<std::mutex> lk(mut);
        cancelled = true;
    }
    processorWakeUpCondition.notify_all();
    processor.join();
    std::cout<< "------------------------------" << std::endl;
}


template <class T>
void AsyncSubscription<T>::printProcessorTerminate(){
    std::cout<< "------------------------------" << std::endl;
    std::cout << "Processor: Received Cancel " << std::endl;
}
