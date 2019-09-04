#ifndef REACTIVE_STREAMS_SIGNALS_H
#define REACTIVE_STREAMS_SIGNALS_H
// These represent the protocol of the `AsyncIterablePublishers` SubscriptionImpls
class Signal {
public :
    virtual ~Signal();
};
class Cancel : public Signal {};
class Subscribe : public Signal {};
class Request : public Signal {
public:
    Request(const unsigned long long n) : n{n} {}
    const unsigned long long n;
    ~Request();
};
#endif // REACTIVE_STREAMS_SIGNALS_H
