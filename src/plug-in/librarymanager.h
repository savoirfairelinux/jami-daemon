#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

#include "dlfcn.h"
#include <stdexcept> 

#include "global.h"

class LibraryManager {

    public:
        typedef void* LibraryHandle;
        typedef void* SymbolHandle;

        LibraryManager (const std::string &filename);
        ~LibraryManager (void);

        int resolveSymbol (const std::string &symbol, SymbolHandle *ptr);

        int unloadLibrary (void);

    protected:
        LibraryHandle loadLibrary (const std::string &filename);

    private:
        std::string _filename;
        LibraryHandle _handlePtr;
};

class LibraryManagerException : public std::runtime_error {

    public:

        typedef enum Reason {
            loadingFailed = 0,
            symbolNotFound
        }Reason;

        LibraryManagerException (const std::string &libraryName, const std::string &details, Reason reason);
        ~LibraryManagerException (void) throw() {}

        inline Reason getReason (void) { return _reason; } 

        const char* what () const throw();

    private:
        Reason _reason;
        std::string _details;
};

#endif // LIBRARY_MANAGER_H
