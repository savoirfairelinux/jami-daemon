#pragma once
#include "reactive_streams.h"
//Std libs
#include <vector>
#include <memory>
#include <atomic>
//================ Subscriber
template <class T>  class BasicSubscriber: public Subscriber<T>{
public:
    virtual void onSubscribe(std::shared_ptr<Subscription<T>>&& subscription) override;
    virtual void onNext(T& t) override;
    virtual void onComplete() override;

public:
    BasicSubscriber(){}
    virtual ~BasicSubscriber();

private:
    std::shared_ptr<Subscription<T>> subscription = nullptr;

    const int batch = 2;
    std::atomic_int counter{0};
};


template <class T>
void BasicSubscriber<T>::onSubscribe(std::shared_ptr<Subscription<T>>&& sub){
    subscription = sub;
    std::cout << "  Subscribed ! " << std::endl;
    subscription->request(batch);
}

template <class T>
void BasicSubscriber<T>::onNext(T& t) {
    std::cout << "  OnNext: " << t << std::endl;
    counter++;
    if(counter == 2) {
        counter = 0;
        subscription->request(batch);

    }
}

template <class T>
void BasicSubscriber<T>::onComplete() {
    std::cout << "  OnComplete called" << std::endl;
}

template <class T>
BasicSubscriber<T>::~BasicSubscriber(){
}
