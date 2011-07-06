#include <iostream>
#include <fstream>
#include <signal.h>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include "video_rtp_session.h"

static volatile sig_atomic_t interrupted = 0;

void signal_handler(int sig) { (void)sig; interrupted = 1; }
void attach_signal_handlers() { signal(SIGINT, signal_handler); }

int main(int argc, char *argv[])
{
    using std::string;
    using std::map;
    attach_signal_handlers();
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] <<  " <filename> (OPTIONAL) <codec> <bitrate> <destination>" << std::endl;
        return 1;
    }

    map<string, string> args;
    args["input"] =  argv[1];
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

    sfl_video::VideoRtpSession session(args);
    session.test();
    while (not interrupted)
        sleep(1);
    session.stop();
    std::cout << "Exitting..." << std::endl;
    return 0;
}
