
#include <QCoreApplication>

#include <TelepathyQt/BaseConnectionManager>
#include <TelepathyQt/Debug>

#include "ProtocolSession.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QLatin1String("telepathy-hanging"));

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);

    Tp::BaseProtocolPtr proto = Tp::BaseProtocol::create<Protocol>(
                                    QDBusConnection::sessionBus(), QLatin1String("hangouts"));
    Tp::BaseConnectionManagerPtr cm = Tp::BaseConnectionManager::create(
                                          QDBusConnection::sessionBus(), QLatin1String("hanging"));
    cm->addProtocol(proto);
    cm->registerObject();

    return app.exec();
}
