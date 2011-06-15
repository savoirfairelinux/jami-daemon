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
        }
        virtual void final()
        {
            std::cout << "final" << std::endl;
        }

        /* terminate() should always be called at the start of any
         * destructor of a class derived from Thread to assure the remaining
         * part of the destructor is called without the thread still executing.
         */
        virtual ~FakeThread()
        {
            terminate();
            std::cout << "destructor" << std::endl;
        }
    private:
        int x_;
};

int main()
{
    FakeThread *th = new FakeThread;
    th->start();
    th->join();
    delete th;
    return 0;
}
