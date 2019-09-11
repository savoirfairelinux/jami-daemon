#include <iostream>
#include <string.h>
#include <thread>
#include <memory>
#include "plugin/pluginmanager.h"
#include "media_processor.h"

extern "C" {

RING_PLUGIN_EXIT(pluginExit) {

}

RING_PluginExitFunc RING_dynPluginInit(const RING_PluginAPI *api){
    std::cout << "VIDEO PLUGIN"<< std::endl;
    std::cout << "======================" << std::endl << std::endl;
    auto bs = std::make_unique<jami::MediaProcessor>();
    api->invokeService(api,"attachInputVideoSubscriber", bs.release());
    return pluginExit;
}
}
