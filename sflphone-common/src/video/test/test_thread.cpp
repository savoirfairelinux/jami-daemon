#include <cc++/thread.h>
#include <iostream>

class FakeThread : public ost::Thread {
    public:
        FakeThread() : ost::Thread(), x_(0)
        {
            std::cout << "constructor" << std::endl;
        }

        virtual void run()
        {
            std::cout << "I'm a thread's execution " << x_ << std::endl;
            event_.signal();
        }
        virtual void final()
        {
            std::cout << "final" << std::endl;
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
        virtual ~FakeThread()
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
    FakeThread *th = new FakeThread;
    th->start();
    th->waitForEvent();
    std::cout << "event has happened..." << std::endl;
    th->waitForEvent(); // this should not block since it's been signalled and not reset
    std::cout << "yup, event has happened..." << std::endl;
    th->join();
    delete th;
    return 0;
}
