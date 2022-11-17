#pragma once

#include "opendht/infohash.h"
#include "connectivity/multiplexed_socket.h"

using NodeId = dht::PkId;

std::vector<NodeId> nodeTestIds1({

    NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"),
    NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8"),
    NodeId("105ba3496ecaa41ad360b45afcefab63ce1c458527ac128d41c791d40e8c62b8"),
    NodeId("1b8cc1705ede1abfdf3de98cf94b62d1482ef87ff8206aff483379ad2ff8a3a5"),
    NodeId("1bd92a8aab91e63267fd91c6ff4d88896bca4b69e422b11894881cd849fa1467"),
    NodeId("28f4c7e34eb4310b2e1ea3b139ee6993e6b021770ee98895a54cdd1e372bd78e"),
    NodeId("2dd1dd976c7dc234ca737c85e4ea48ad09423067a77405254424c4cdd845720d"),
    NodeId("30e177a56bd1a7969e1973ad8b210a556f6a2b15debc972661a8f555d52edbe2"),
    NodeId("312226d8fa653704758a681c8c21ec81cec914d0b8aa19e1142d3cf900e3f3b4"),
    NodeId("33f280d8208f42ac34321e6e6871aecd100c2bfd4f1848482e7a7ed8ae895414")

});

std::vector<NodeId> nodeTestIds2({

    NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"), // 0
    NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8"), // 1
    NodeId("77a9fba2c5a65812d9290c567897131b20a723e0ca2f65ef5c6b421585e4da2b"), // 2
    NodeId("6110cda4bc6f5465acab2e779fad607bf4edcf6114c7333968a2d197aa7a0a63"), // 3
    NodeId("6aed0ef602f5c676e7afd337e3984a211c385f38230fa894cfa91e1cbd592b5c"), // 4
    NodeId("e633ca67cc8801ec141b2f7eb55b78886e9266ed60c7e4bc12c232ab60d7317e"), // 5
    NodeId("84b59e42e8156d18794d263baae06344871b9f97d5006e1f99e8545337c37c37"), // 6
    NodeId("a3b1b35be59ed62926569479d72718ccca553710f2a22490d1155d834d972525"), // 7
    NodeId("4f76e769061f343b2caf9eea35632d28cde8d7a67e5e0f59857733cabc538997"), // 8
    NodeId("fda3516c620bf55511ed0184fc3e32fc346ea0f3a2c6bc19257bd98e19734307")  // 9

});

std::vector<NodeId> nodeTestIds3({

    NodeId("41a05179e4b3e42c3409b10280bb448d5bbd5ef64784b997d2d1663457bb6ba8"),
    NodeId("053927d831827a9f7e606d4c9c9fe833922c0d35b3960dd2250085f46c0e4f41"),
    NodeId("271c3365b92f249597be69e4fde318cb13abd1059fb3ad88da52d7690083ffb0"),
    NodeId("6aed0ef602f5f676e7afd337e3984a211c385f38230fa894cfa91e1cbd592b5c"),
    NodeId("821bc564703ba2fc147f12f1ec30a2bd39bd8ad9fe241da3b47d391cfcc87519"),
    NodeId("84b59e42e815dd18794d263baae06344871b9f97d5006e1f99e8545337c37c37"),
    NodeId("897b3ff45a8e1c1fa7168b2fac0f32b6cfa3bf430685b36b6f2d646a9125164e"),
    NodeId("a3b1b35be59ed62926569479d72718ccca553710f2a22490d1155d834d972525"),
    NodeId("f04d8116705f692677cb7cb519c078341fe8aaae3f792904a7be3a8ae0bfa1ea"),
    NodeId("f3a0b932602befe4c00b8bf7d2101f60a712bb55b0f62a023766250044b883a8")

});

std::vector<std::shared_ptr<jami::ChannelSocketTest>> nodeTestChannels1(
    {std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(0), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(1), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(2), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(3), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(4), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(5), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(6), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(7), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(8), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds1.at(9), "test1", 0)

    });

std::vector<std::shared_ptr<jami::ChannelSocketTest>> nodeTestChannels2(
    {std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(0), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(1), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(2), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(3), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(4), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(5), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(6), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(7), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(8), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds2.at(9), "test1", 0)

    });

std::vector<std::shared_ptr<jami::ChannelSocketTest>> nodeTestChannels3(
    {std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(0), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(1), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(2), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(3), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(4), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(5), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(6), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(7), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(8), "test1", 0),
     std::make_shared<jami::ChannelSocketTest>(nodeTestIds3.at(9), "test1", 0)

    });
