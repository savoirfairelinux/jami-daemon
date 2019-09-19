#pragma once

// Reactive Streams
#include "media/filters/syncsubject.h"
//FFMPEG
#include <libavutil/frame.h>
// Plugin Manager
#include "pluginmanager.h"
#include "customplugin.h"
// Std library
#include <map>

namespace jami {
using PluginPtr = std::unique_ptr<CustomPlugin<AVFrame*>>;

class PluginStreamManager{

public:

    PluginStreamManager(){
        registerServices();
    }

    // Subjects
    SyncSubject<AVFrame*> videoSubject;
    SyncSubject<AVFrame*> audioSubject;

public:

    /**
     * @brief loadPlugin
     * @param path of the plugin .so file
     */
    void loadPlugin(const std::string& path){
        pm.load(path);
    }

    /**
     * @brief unloadPlugin
     * @param pluginId
     */
    void unloadPlugin(const std::string& pluginId){
        std::map<std::string, std::vector<PluginPtr>>::iterator it = plugins.find(pluginId);
        if(it != plugins.end()) {
            std::vector<PluginPtr>& subscribers = it->second;
            for(const auto& subscriber:subscribers ){
                subscriber->unsubscribe();
            }
            plugins.erase(it);
            pm.unload(pluginId);
        }
    }

private:

    /**
     * @brief registerServices
     * Main Api, exposes functions to the plugins
     */
    void registerServices() {
        auto attachInputVideoSubscriber = [this](void* data) {
            PluginPtr ptr{(reinterpret_cast<CustomPlugin<AVFrame*>*>(data))};
            const std::string pluginId = ptr->id();

            if(ptr && !pluginId.empty()) {
                std::map<std::string, std::vector<PluginPtr>>::iterator it = plugins.find(pluginId);
                // If not inserted before, create empty pair
                if(it == plugins.end()) {
                    plugins.insert(std::make_pair(pluginId,std::vector<PluginPtr>()));
                    // Find the index of the plugin if inserted properly
                    it = plugins.find(pluginId);
                }

                if(it != plugins.end()){
                    std::vector<PluginPtr>& pluginSubscribers = it->second;
                    pluginSubscribers.push_back(std::move(ptr));
                    PluginPtr& insertedPlugin = pluginSubscribers.back();
                    videoSubject.subscribe(*insertedPlugin);
                }
            }

            return 0;
        };

        pm.registerService("attachInputVideoSubscriber", attachInputVideoSubscriber);
    }

private:
    // Plugin Manager
    PluginManager pm;

    /**
     * @brief plugins
     * Each plugin can hold multiple subscribers
     * E.g: one for the audio stream and one for the video stream
     * We need to know which to which plugin they belong to
     * In order to unsubscribe them when the plugin is detached
     */
    std::map<const std::string, std::vector<PluginPtr>> plugins;
};

}
