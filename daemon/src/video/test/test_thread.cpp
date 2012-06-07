#include "cc_thread.h"
#include <memory>
#include <iostream>
#include "noncopyable.h"

class CancellableBusyThread : public ost::Thread {
    public:
        CancellableBusyThread() : started_(true), x_(0)
        {}

        virtual void run()
        {
            x_ = new int(0);
            while (started_) {
                (*x_) += 1;
                yield();
            }
        }

        /* terminate() should always be called at the start of any
         * destructor of a class derived from Thread to assure the remaining
         * part of the destructor is called without the thread still executing.
         */
        virtual ~CancellableBusyThread()
        {
            std::cout << __PRETTY_FUNCTION__ << std::endl;
            ost::Thread::terminate();
            delete x_;
            x_ = 0;
        }

        bool started_;
    private:
        int *x_;
        NON_COPYABLE(CancellableBusyThread);
};

class EventThread : public ost::Thread {
    public:
        EventThread() : ost::Thread(), x_(0), event_()
        {}

        virtual void run()
        {
            event_.signal();
        }

        // called from other threads
        void waitForEvent()
        {
            event_.wait();
        }

        /* terminate() should always be called at the start of any
         * destructor of a class derived from Thread to assure the remaining
         * part of the destructor is called without the thread still executing.
         */
        virtual ~EventThread()
        {
            ost::Thread::terminate();
            std::cout << __PRETTY_FUNCTION__ << std::endl;
        }
    private:
        int x_;
        ost::Event event_;
};

int main()
{
    EventThread *th = new EventThread;
    th->start();
    th->waitForEvent();
    std::cout << "event has happened..." << std::endl;
    th->join();
    delete th;

    std::auto_ptr<CancellableBusyThread> busy(new CancellableBusyThread);
    std::cout << "Starting busy thread" << std::endl;
    busy->start();
    busy->started_ = false;
    busy.reset();
    std::cout << "Finished busy thread" << std::endl;

    return 0;
}
