/************************************************************************
 * Basic implementation of REACTIVE STREAMS                              *
 *                                                                       *
 * https://www.reactive-streams.org/                                     *
 ************************************************************************/

#ifndef REACTIVE_STREAMS_H
#define REACTIVE_STREAMS_H
//Std libs
#include <exception>
#include <iostream>
#include <memory>

template <class T>  class Subscription {
public:
    virtual void request(unsigned long long  n) = 0;
    virtual void cancel() = 0;

public:
    virtual ~Subscription() = default;
};

template <class T>  class Subscriber {
public:
    virtual void onSubscribe(std::shared_ptr<Subscription<T>>&& subscription) = 0;
    virtual void onNext(T t) = 0;
    virtual void onError(const std::exception& e) const { std::cerr <<  e.what() << std::endl;}
    virtual void onComplete() {}

public:
    virtual ~Subscriber() = default;
};

template <class T>  class Publisher {
public:
    virtual void subscribe(Subscriber<T>& subscriber) = 0;
public:
    virtual ~Publisher() = default;
};


#endif // REACTIVE_STREAMS_H
