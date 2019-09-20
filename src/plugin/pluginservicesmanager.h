#pragma once

// Reactive Streams
#include "media/filters/syncsubject.h"
// Plugin Manager
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
// Std library
#include <map>

namespace jami {
using MediaHandlerPtr = std::unique_ptr<MediaHandler>;
using MediaStreamHandlerPtr = std::unique_ptr<MediaStreamHandler>;
using AVSubjectSPtr = std::shared_ptr<SyncSubject<AVFrame*>>;

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
        pm.load(path);
    }

    /**
     * @brief unloadPlugin
     * @param pluginId
     */
    void unloadPlugin(const std::string& pluginId){
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
    void notifyAllAVSubject(StreamData data, std::weak_ptr<SyncSubject<AVFrame*>> subject) {
        for(auto& pair : plugins) {
            auto& pluginPtr = pair.second;
            notifyAVSubject(pluginPtr, data, subject);
        }
    }

    AVSubjectSPtr getAVSubject(const std::string& id) {
        for(auto& avsubject : avsubjects) {
            if(avsubject.first.id == id) {
                return avsubject.second;
            }
        }

        return nullptr;
    }
    /**
     * @brief createAVSubject
     * @param data
     * Creates an av frame subject with properties StreamData
     */
    void createAVSubject(const StreamData& data){
        decltype(avsubjects.begin()) it;
        for(it = avsubjects.begin(); it != avsubjects.end(); ++it) {
            if(it->first.id == data.id) {
                break;
            }
        }
        // This guarantees unicity of subjects by id
        if(it == avsubjects.end()) {
            avsubjects.push_back(std::make_pair(data, std::make_shared<SyncSubject<AVFrame*>>()));
        }
    }


    /**
     * @brief removeAVSubject
     * @param id
     * Removes a subject
     */
    void removeAVSubject(const std::string& id) {
        for(auto it=avsubjects.begin(); it != avsubjects.end();) {
            if(it->first.id == id) {
                avsubjects.erase(it);
                break;
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
                              std::weak_ptr<SyncSubject<AVFrame*>> subject) {
        if (auto avsubject = subject.lock()) {
            plugin->notifyAVFrameSubject(data, avsubject);
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
                if(!plugins.empty()) {
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
