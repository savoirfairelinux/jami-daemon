#ifndef __RING_DBUSVIDEOMANAGER_H__
#define __RING_DBUSVIDEOMANAGER_H__

#include <vector>
#include <map>
#include <string>
#include <restbed>

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef RING_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class RestVideoManager
{
    public:
        RestVideoManager();

        std::vector<std::shared_ptr<restbed::Resource>> getResources();

    private:
        // Attributes
        std::vector<std::shared_ptr<restbed::Resource>> resources_;

        // Methods
        std::map<std::string, std::string> parsePost(const std::string& post);
        void populateResources();
        void defaultRoute(const std::shared_ptr<restbed::Session> session);

        void getDeviceList(const std::shared_ptr<restbed::Session> session);
        void getCapabilities(const std::shared_ptr<restbed::Session> session);
        void getSettings(const std::shared_ptr<restbed::Session> session);
        void applySettings(const std::shared_ptr<restbed::Session> session);
        void setDefaultDevice(const std::shared_ptr<restbed::Session> session);
        void getDefaultDevice(const std::shared_ptr<restbed::Session> session);
        void startCamera(const std::shared_ptr<restbed::Session> session);
        void stopCamera(const std::shared_ptr<restbed::Session> session);
        void switchInput(const std::shared_ptr<restbed::Session> session);
        void hasCameraStarted(const std::shared_ptr<restbed::Session> session);
};

#endif // __RING_DBUSVIDEOMANAGER_H__
