#pragma once

#include <vector>
#include <map>
#include <string>

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef RING_VIDEO
#include "dring/videomanager_interface.h"
#endif

#pragma GCC diagnostic warning "-Wignored-qualifiers"
#pragma GCC diagnostic warning "-Wunused-parameter"

#if __GNUC__ >= 5 || (__GNUC__ >=4 && __GNUC_MINOR__ >= 6)
/* This warning option only exists for gcc 4.6.0 and greater. */
#pragma GCC diagnostic warning "-Wunused-but-set-variable"
#endif

class RestConfigurationManager
{
	public:
		RestConfigurationManager() {}

		// Account related methods
		std::map<std::string, std::string> getAccountDetails(const std::string& accountID);
        std::map<std::string, std::string> getVolatileAccountDetails(const std::string& accountID);
        void setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details);
        void setAccountActive(const std::string& accountID, const bool& active);
        std::map<std::string, std::string> getAccountTemplate(const std::string& accountType);
        std::string addAccount(const std::map<std::string, std::string>& details);
        void removeAccount(const std::string& accoundID);
        std::vector<std::string> getAccountList();
        void sendRegister(const std::string& accoundID, const bool& enable);
		void registerAllAccounts(void);

};
