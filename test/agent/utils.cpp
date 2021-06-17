#include <cassert>
#include <vector>

#include "agent/utils.h"

/* Jami */
#include "account_const.h"
#include "dring.h"
#include "fileutils.h"
#include "manager.h"

void
wait_for_announcement_of(const std::vector<std::string> accountIDs,
                         std::chrono::seconds timeout)
{
    std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>> confHandlers;
    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
    std::vector<std::atomic_bool> accountsReady(accountIDs.size());

    size_t to_be_announced = accountIDs.size();

    confHandlers.insert(
        DRing::exportable_callback<DRing::ConfigurationSignal::VolatileDetailsChanged>(
            [&,
             accountIDs = std::move(accountIDs)](const std::string& accountID,
                                                 const std::map<std::string, std::string>& details) {
                for (size_t i = 0; i < accountIDs.size(); ++i) {
                    if (accountIDs[i] != accountID) {
                        continue;
                    }

                    try {
                        if ("true"
                            != details.at(DRing::Account::VolatileProperties::DEVICE_ANNOUNCED)) {
                            continue;
                        }
                    } catch (const std::out_of_range&) {
                        continue;
                    }

                    accountsReady[i] = true;
                    cv.notify_one();
                }
            }));

    JAMI_DBG("Waiting for %zu account to be announced...", to_be_announced);

    DRing::registerSignalHandlers(confHandlers);

    assert(cv.wait_for(lk, timeout, [&] {
        for (const auto& rdy : accountsReady) {
            if (not rdy) {
                return false;
            }
        }

        return true;
    }));

    DRing::unregisterSignalHandlers();

    JAMI_DBG("%zu account announced!", to_be_announced);
}

void
wait_for_announcement_of(const std::string& accountId,
                         std::chrono::seconds timeout)
{
    wait_for_announcement_of(std::vector<std::string> {accountId}, timeout);
}
