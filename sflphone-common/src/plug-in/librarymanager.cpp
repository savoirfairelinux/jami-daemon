/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "librarymanager.h"

LibraryManager::LibraryManager (const std::string &filename)
        : _filename (filename), _handlePtr (NULL)
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

    _debug ("Loading dynamic library %s", filename.c_str());

    /* Load the library */
    pluginHandlePtr = dlopen (filename.c_str(), RTLD_LAZY);

    if (!pluginHandlePtr) {
        error = dlerror();
        _debug ("Error while opening plug-in: %s", error);
        return NULL;
    }

    dlerror();

    return pluginHandlePtr;
}

int LibraryManager::unloadLibrary ()
{
    if (_handlePtr == NULL)
        return 1;

    _debug ("Unloading dynamic library ...");

    dlclose (_handlePtr);

    if (dlerror()) {
        _debug ("Error unloading the library : %s...", dlerror());
        return 1;
    }

    return 0;
}

int LibraryManager::resolveSymbol (const std::string &symbol, SymbolHandle *symbolPtr)
{
    SymbolHandle sy = 0;

    if (_handlePtr) {
        try {
            sy = dlsym (_handlePtr, symbol.c_str());

            if (sy != NULL) {
                *symbolPtr = sy;
                return 0;
            }
        } catch (...) {}

        throw LibraryManagerException (_filename, symbol, LibraryManagerException::symbolNotFound);
    } else
        return 1;
}


/************************************************************************************************/

LibraryManagerException::LibraryManagerException (const std::string &libraryName, const std::string &details, Reason reason) :
        _reason (reason), _details (""), std::runtime_error ("")

{
    if (_reason == loadingFailed)
        _details = "Error when loading " + libraryName + "" + details;
    else
        _details = "Error when resolving symbol " + details + " in " + libraryName;
}

const char* LibraryManagerException::what () const throw()
{
    return _details.c_str();
}
