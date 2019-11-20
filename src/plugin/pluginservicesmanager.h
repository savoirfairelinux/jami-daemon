/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once
// Utils
#include "noncopyable.h"
// Plugin Manager
#include "pluginmanager.h"
#include "streamdata.h"
#include "mediahandler.h"
// STL
#include <list>

namespace jami {
using MediaHandlerPtr = std::unique_ptr<MediaHandler>;
using CallMediaHandlerPtr = std::unique_ptr<CallMediaHandler>;
using AVSubjectSPtr = std::weak_ptr<Observable<AVFrame*>>;

class PluginServicesManager{

public:

    PluginServicesManager(){
        registerServices();
    }

    /**
    *   unload all media handlers and their associated plugin.so
    **/
    ~PluginServicesManager(){
        /** Remove all media stream handlers BEFORE the plugins
        *   are destroyed (~DLPlugin), this way we avoid
        *   Segfaults and Heap-use after free faults
        **/
        callMediaHandlers.clear();
    }
    
    NON_COPYABLE(PluginServicesManager);

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
        for(auto it = callMediaHandlers.begin(); it != callMediaHandlers.end(); ++it) {
            // Remove all possible duplicates, before unloading a plugin
            if(it->first == pluginId) {
                callMediaHandlers.erase(it);
            }
        }
        pm.unload(pluginId);
    }

    /**
     * @brief togglePlugin
     * @param path: used as an id
     * @param toggle: if true, register a new instance of the plugin
     * else, remove the existing instance
     * N.B: before adding a new instance, remove any existing one
     */
    void togglePlugin(const std::string& path, bool toggle){
        // remove the previous plugin object if it was registered
        for(auto it = callMediaHandlers.begin(); it != callMediaHandlers.end();) {
            if(it->first == path) {
                callMediaHandlers.erase(it);
                break;
            } else {
                ++it;
            }
        }
        // If toggle, register a new instance of the plugin
        // function
        if(toggle){
            pm.callPluginInitFunction(path);
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
    void notifyAllAVSubject(const StreamData& data, AVSubjectSPtr& subject) {
        for(auto& pair : callMediaHandlers) {
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
        std::cout << "CREATED AV SUBJECT: DIRECTION " << data.direction << std::endl;
        callAVsubjects.push_back(std::make_pair(data, subject));
        auto inserted = callAVsubjects.back();
        notifyAllAVSubject(inserted.first, inserted.second);
    }


    /**
     * @brief cleanup
     *
     */
    void cleanup() {
        for(auto it=callAVsubjects.begin(); it != callAVsubjects.end();) {
            if(it->second.expired()) {
                it = callAVsubjects.erase(it);
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
    void notifyAVSubject(CallMediaHandlerPtr& plugin,
                              const StreamData& data,
                         AVSubjectSPtr& subject) {
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
    void listAvailableSubjects(CallMediaHandlerPtr& plugin) {
        for(auto it=callAVsubjects.begin(); it != callAVsubjects.end(); ++it) {
            notifyAVSubject(plugin, it->first, it->second);
        }
    }

    /**
     * @brief registerServices
     * Main Api, exposes functions to the plugins
     */
    void registerServices() {

        auto registerPlugin = [this](void* data) {
            CallMediaHandlerPtr ptr{(reinterpret_cast<CallMediaHandler*>(data))};

            if(ptr) {
                const std::string pluginId = ptr->id();

                for(auto it=callMediaHandlers.begin(); it!=callMediaHandlers.end(); ++it){
                    if(it->first ==  pluginId) {
                        return 1;
                    }
                }

                callMediaHandlers.push_back(std::make_pair(pluginId, std::move(ptr)));
                if(!callMediaHandlers.empty() && !callAVsubjects.empty()) {
                    listAvailableSubjects(callMediaHandlers.back().second);
                }
            }

            return 0;
        };


        pm.registerService("registerCallMediaHandler", registerPlugin);
    }

private:
    // Plugin Manager
    PluginManager pm;

    /**
     * @brief callMediaHandlers
     * Objects that a plugin can register through registerCallMediaHandler service
     * These objects can then be notified with notify notifyAVFrameSubject
     * whenever there is a new CallAVSubject like a video receive
     */
    std::list<std::pair<const std::string, CallMediaHandlerPtr>> callMediaHandlers;
    /**
     * @brief callAVsubjects
     * When there is a SIPCall, CallAVSubjects are created there
     * Here we keep a reference to them in order to make them interact with
     * CallMediaHandlers
     * It is pushed to this list list
     */
    std::list<std::pair<const StreamData, AVSubjectSPtr>> callAVsubjects;
};

}
