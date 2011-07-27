#include <cc++/thread.h>
#include <memory>
#include <iostream>

class CancellableBusyThread : public ost::Thread {
    public:
        CancellableBusyThread()
        {
            setCancel(cancelImmediate);
        }

        virtual void run()
        {
            x_ = new int(0);
            while (true)
            {
                (*x_) += 1;
                yield();
            }
        }

        virtual void final()
        {
            std::cout << __PRETTY_FUNCTION__ << std::endl;
            delete x_;
            x_ = 0;
        }

        /* terminate() should always be called at the start of any
         * destructor of a class derived from Thread to assure the remaining
         * part of the destructor is called without the thread still executing.
         */
        virtual ~CancellableBusyThread()
        {
            terminate();
        }

    private:
        int *x_;
};

class EventThread : public ost::Thread {
    public:
        EventThread() : ost::Thread(), x_(0)
        {}

        virtual void run()
        {
            event_.signal();
        }

        virtual void final()
        {
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
            terminate();
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
    th->waitForEvent(); // this should not block since it's been signalled and not reset
    std::cout << "yup, event has happened..." << std::endl;
    th->join();
    delete th;

    std::auto_ptr<CancellableBusyThread> busy(new CancellableBusyThread);
    busy->start();
    busy.reset();
    std::cout << "Finished busy thread" << std::endl;

    return 0;
}
