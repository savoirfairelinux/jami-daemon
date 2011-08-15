#include <QtCore/QtGlobal>
 int main()
 {
 #ifdef QT_VISIBILITY_AVAILABLE 
 return 0;
 #else 
 return 1; 
 #endif 
 }
