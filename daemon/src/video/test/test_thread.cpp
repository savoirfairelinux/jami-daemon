/*
 *  Copyright (C) 2012 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

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
