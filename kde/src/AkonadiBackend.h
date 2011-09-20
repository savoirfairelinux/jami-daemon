#ifndef AKONADI_BACKEND_H
#define AKONADI_BACKEND_H

#include <QObject>

class AkonadiBackend : public QObject {
public:
   AkonadiBackend* getInstance();
private:
   AkonadiBackend(QObject* parent);
   virtual ~AkonadiBackend();
   static bool init();
   
   //Attributes
   AkonadiBackend* instance;
};

#endif