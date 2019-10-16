#pragma once

// Plugin Manager
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
// STL
#include <list>

namespace jami {
using MediaHandlerPtr = std::unique_ptr<MediaHandler>;
using MediaStreamHandlerPtr = std::unique_ptr<MediaStreamHandler>;
using AVSubjectSPtr = std::weak_ptr<Observable<AVFrame*>>;

class PluginServicesManager{

public:

    PluginServicesManager(){
        registerServices();
    }

public:

    /**
     * @brief loadPlugin
     * @param path of the plugin .so file
     */
    void loadPlugin(const std::string& path){
        std::cout << "LOAD PLUGIN" << std::endl;
        pm.load(path);
    }

    /**
     * @brief unloadPlugin
     * @param pluginId
     */
    void unloadPlugin(const std::string& pluginId){
        std::cout << "UNLOAD PLUGIN" << std::endl;
        for(auto it = plugins.begin(); it != plugins.end();) {
            if(it->first == pluginId) {
                plugins.erase(it);
                pm.unload(pluginId);
                break;
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief notifyAllAVSubject
     * @param subject
     * @param av
     * @param local
     * @param peerId
     * This function is called whenever there is a new AVFrame subject available
     */
    void notifyAllAVSubject(StreamData data, AVSubjectSPtr subject) {
        for(auto& pair : plugins) {
            auto& pluginPtr = pair.second;
            notifyAVSubject(pluginPtr, data, subject);
        }
    }

    /**
     * @brief createAVSubject
     * @param data
     * Creates an av frame subject with properties StreamData
     */
    void createAVSubject(const StreamData& data, AVSubjectSPtr subject){
        // This guarantees unicity of subjects by id
        std::cout << "CREATED AV SUBJECT DIRECTION " << data.direction << std::endl;
        avsubjects.push_back(std::make_pair(data, subject));
        auto inserted = avsubjects.back();
        std::cout << "CREATED AV SUBJECT INSERTED DIRECTION " << inserted.first.direction << std::endl;
        notifyAllAVSubject(inserted.first, inserted.second);
    }


    /**
     * @brief cleanup
     *
     */
    void cleanup() {
        for(auto it=avsubjects.begin(); it != avsubjects.end();) {
            if(it->second.expired()) {
                it = avsubjects.erase(it);
            } else {
                ++it;
            }
        }
    }

private:

    /**
     * @brief notifyAVSubject
     * @param plugin
     * @param data
     * @param subject
     */
    void notifyAVSubject(MediaStreamHandlerPtr& plugin,
                              const StreamData& data,
                         AVSubjectSPtr subject) {
        std::cout<< "NOTIFYING NEW SUBJECT " << std::endl;
        if(auto soSubject = subject.lock()) {
            std::cout<< "LOCK SUCCESSFUL NOTIFYING NEW SUBJECT " << std::endl;
            plugin->notifyAVFrameSubject(data, soSubject);
        }

    }

    /**
     * @brief listAvailableSubjects
     * @param plugin
     * This functions lets the plugin know which subjects are available
     */
    void listAvailableSubjects(MediaStreamHandlerPtr& plugin) {
        for(auto it=avsubjects.begin(); it != avsubjects.end(); ++it) {
            notifyAVSubject(plugin, it->first, it->second);
        }
    }

    /**
     * @brief registerServices
     * Main Api, exposes functions to the plugins
     */
    void registerServices() {

        auto registerPlugin = [this](void* data) {
            MediaStreamHandlerPtr ptr{(reinterpret_cast<MediaStreamHandler*>(data))};

            if(ptr) {
                const std::string pluginId = ptr->id();

                for(auto it=plugins.begin(); it!=plugins.end(); ++it){
                    if(it->first ==  pluginId) {
                        return 1;
                    }
                }

                plugins.push_back(std::make_pair(pluginId, std::move(ptr)));
                if(!plugins.empty() && !avsubjects.empty()) {
                    listAvailableSubjects(plugins.back().second);
                }
            }

            return 0;
        };


        pm.registerService("registerPlugin", registerPlugin);
    }

private:
    // Plugin Manager
    PluginManager pm;

    /**
     * @brief plugins
     * Map of type (plugin Id, plugin pointer)
     * The id here is the path (it provides uniqueness)
     */

    std::list<std::pair<const std::string, MediaStreamHandlerPtr>> plugins;
    std::list<std::pair<const StreamData, AVSubjectSPtr>> avsubjects;
};

}
