#include <iostream>
#include <fstream>
#include <signal.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "video_rtp_session.h"

#if 0
static volatile int interrupted = 0;

void signal_handler(int sig) { (void)sig; interrupted = 1; }
void attach_signal_handlers() { signal(SIGINT, signal_handler); }
#endif

int main(int argc, char *argv[])
{
    //attach_signal_handlers();
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] <<  " <filename> <codec>" << std::endl;
        return 1;
    }
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(argv[i]);
    if (argc < 3)
        args.push_back("mpeg4");
    if (argc < 4)
        args.push_back("1000000");
    if (argc < 5)
        args.push_back("rtp://127.0.0.1:5000");

    sfl_video::VideoRtpSession session(args[0] /*input*/, args[1] /*codec*/,
            atoi(args[2].c_str()) /* bitrate */, args[3] /* uri */);
    session.start();
    std::cout << "Exitting..." << std::endl;
    return 0;
}
