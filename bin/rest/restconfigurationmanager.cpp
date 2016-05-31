#include "restconfigurationmanager.h"

std::map<std::string, std::string>
RestConfigurationManager::getAccountDetails(const std::string& accountID)
{
	return DRing::getAccountDetails(accountID);
}

std::map<std::string, std::string>
RestConfigurationManager::getVolatileAccountDetails(const std::string& accountID)
{
	return DRing::getVolatileAccountDetails(accountID);
}

void
RestConfigurationManager::setAccountDetails(const std::string& accountID, const std::map<std::string, std::string>& details)
{
	DRing::setAccountDetails(accountID, details);
}

void
RestConfigurationManager::setAccountActive(const std::string& accountID, const bool& active)
{
	DRing::setAccountActive(accountID, active);
}

std::map<std::string, std::string>
RestConfigurationManager::getAccountTemplate(const std::string& accountType)
{
	return DRing::getAccountTemplate(accountType);
}

std::string
RestConfigurationManager::addAccount(const std::map<std::string, std::string>& details)
{
	return DRing::addAccount(details);
}

void
RestConfigurationManager::removeAccount(const std::string& accountID)
{
	DRing::removeAccount(accountID);
}

std::vector<std::string>
RestConfigurationManager::getAccountList()
{
	return DRing::getAccountList();
}

void
RestConfigurationManager::sendRegister(const std::string& accountID, const bool& enable)
{
	DRing::sendRegister(accountID, enable);
}

void
RestConfigurationManager::registerAllAccounts(void)
{
	DRing::registerAllAccounts();
}
