#include "shm_sink.h"
#include "shm_src.h"
#include <thread>
#include <signal.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <atomic>
#include <cstring>
#include <cassert>

namespace {

std::atomic<bool> done(false);

void signal_handler(int /*sig*/)
{
    done = true;
}

const char test_data[] = "abcdefghijklmnopqrstuvwxyz";

void sink_thread()
{
    SHMSink sink("bob");;
    if (!sink.start())
        return;
    while (!done) {
        sink.render((char*) test_data, sizeof(test_data));
        usleep(1000);
    }
    sink.stop();
    std::cerr << std::endl;
    std::cerr << "Exitting sink thread" << std::endl;
}

void run_client()
{
    SHMSrc src("bob");;
    bool started = false;
    while (not done and not started) {
        sleep(1);
        if (src.start())
            started = true;
    }
    // we get here if the above loop was interupted by our signal handler
    if (!started)
        return;

    // initialize destination string to 0's
    std::string dest(sizeof(test_data), 0);
    const std::string test_data_str(test_data, sizeof(test_data));
    assert(test_data_str.size() == 27);
    assert(dest.size() == test_data_str.size());
    while (not done and dest != test_data_str) {
        src.render(&(*dest.begin()), dest.size());
        usleep(1000);
    }
    src.stop();
    std::cerr << "Got characters, exitting client process" << std::endl;
}

void run_daemon()
{
    std::thread bob(sink_thread);
    /* Wait for child process. */
    int status;
    int pid;
    if ((pid = wait(&status)) == -1) {
        perror("wait error");
    } else {
        // Check status.
        if (WIFSIGNALED(status) != 0)
            std::cout << "Child process ended because of signal " <<
                    WTERMSIG(status) << std::endl;
        else if (WIFEXITED(status) != 0)
            std::cout << "Child process ended normally; status = " <<
                    WEXITSTATUS(status) << std::endl;
        else
            std::cout << "Child process did not end normally" << std::endl;
    }
    std::cout << "Finished waiting for child" << std::endl;
    done = true;
    // wait for thread
    bob.join();
}

} // end anonymous namespace

int main()
{
    signal(SIGINT, signal_handler);
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Failed to fork" << std::endl;
        return 1;
    } else if (pid == 0) {
        // child code only
        run_client();
    } else {
        // parent code only
        run_daemon();
    }
    return 0;
}
