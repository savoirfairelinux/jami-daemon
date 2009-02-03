#include "librarymanager.h"
    
    LibraryManager::LibraryManager (const std::string &filename)
    : _filename(filename), _handlePtr(NULL)
{
    _handlePtr = loadLibrary (filename);
}

LibraryManager::~LibraryManager (void)
{
    unloadLibrary ();
}

LibraryManager::LibraryHandle LibraryManager::loadLibrary (const std::string &filename)
{
    LibraryHandle pluginHandlePtr = NULL;
    const char *error;

    _debug("Loading dynamic library %s\n", filename.c_str());

    /* Load the library */
    pluginHandlePtr = dlopen( filename.c_str(), RTLD_LAZY );
    if( !pluginHandlePtr ) {
        error = dlerror();
        _debug("Error while opening plug-in: %s\n", error);
        return NULL;
    }
    dlerror();
    return pluginHandlePtr;
}

int LibraryManager::unloadLibrary ()
{
    if (_handlePtr == NULL)
        return 1;

    _debug("Unloading dynamic library ...\n");
    dlclose( _handlePtr );
    if (dlerror())
    {
        _debug("Error unloading the library : %s\n...", dlerror());
        return 1;
    }
    return 0;
}

int LibraryManager::resolveSymbol (const std::string &symbol, SymbolHandle *symbolPtr)
{
    SymbolHandle sy = 0;

    if (_handlePtr){
        try {
            sy = dlsym(_handlePtr, symbol.c_str());
            if(sy != NULL) {
                *symbolPtr = sy;
                return 0;
            }
        }
        catch (...) {}
        
        throw LibraryManagerException ( _filename, symbol, LibraryManagerException::symbolNotFound);
    }
    else
        return 1;
}


/************************************************************************************************/

LibraryManagerException::LibraryManagerException (const std::string &libraryName, const std::string &details, Reason reason) :
    _reason (reason), _details (""), std::runtime_error ("")
    
{
    if (_reason == loadingFailed)
        _details = "Error when loading " + libraryName + "\n" + details;
    else
        _details = "Error when resolving symbol " + details + " in " + libraryName;
}

const char* LibraryManagerException::what () const throw()
{
    return _details.c_str();
}
