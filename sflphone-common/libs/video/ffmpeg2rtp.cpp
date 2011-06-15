#include <iostream>
#include <fstream>
#include <signal.h>
#include <cstdlib>
#include <string>
#include <map>
#include <cc++/thread.h>
#include "video_rtp_session.h"

static volatile int interrupted = 0;

void signal_handler(int sig) { (void)sig; interrupted = 1; }
void attach_signal_handlers() { signal(SIGINT, signal_handler); }

int main(int argc, char *argv[])
{
    attach_signal_handlers();
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] <<  " <filename> (OPTIONAL) <codec> <bitrate> <destination>" << std::endl;
        return 1;
    }

    std::map<std::string, std::string> args;
    args["input"] = argv[1];
    args["codec"] = "mpeg4";
    args["bitrate"] = "1000000";
    args["destination"] = "rtp://127.0.0.1:5000";

    switch (argc)
    {
        case 5:
            args["destination"] = argv[4];
            /* fallthrough */
        case 4:
            args["bitrate"] = argv[3];
            /* fallthrough */
        case 3:
            args["codec"] = argv[2];
            /* fallthrough */
        case 2:
        default:
            break;
    }

    sfl_video::VideoRtpSession session(args["input"], args["codec"],
            atoi(args["bitrate"].c_str()), args["destination"]);
    session.start();
    while (not interrupted)
        ost::Thread::sleep(1000);
    session.stop();
    std::cout << "Exitting..." << std::endl;
    return 0;
}
