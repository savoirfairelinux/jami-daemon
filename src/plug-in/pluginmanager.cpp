#include <dirent.h>
#include <dlfcn.h>

#include "pluginmanager.h"

::sflphone::PluginManager::PluginManager():_loadedPlugins()
{
    _instance = this;
}

::sflphone::PluginManager::~PluginManager()
{
    _instance = 0;
}

::sflphone::PluginManager* ::sflphone::PluginManager::instance()
{
    if(! _instance ){
        _instance = new PluginManager();
    }
    return _instance;
}   


int ::sflphone::PluginManager::loadPlugins( const std::string &path )
{
    std::string pluginDir, current;
    ::sflphone::Plugin *plugin;
    DIR *dir;
    dirent *dirStruct;
    int result=0;
    
    const std::string pDir = "..";
    const std::string cDir = ".";

    /* The directory in which plugins are dropped. Default: /usr/lib/sflphone/plugins/ */
    ( path == "" )? pluginDir = std::string(PLUGINS_DIR).append("/"):pluginDir = path;
    _debug("Loading plugins from %s...\n", pluginDir.c_str());

    dir = opendir( pluginDir.c_str() );
    /* Test if the directory exists or is readable */
    if( dir ){
        /* Read the directory */
        while( (dirStruct=readdir(dir)) ){
            /* Get the name of the current item in the directory */
            current = dirStruct->d_name;
            /* Test if the current item is not the parent or the current directory */
            if( current != pDir && current != cDir ){
                loadDynamicLibrary( current );
                result++;
	        }
	    }
    }
    /* Close the directory */
    closedir( dir );

    return result;
}

::sflphone::Plugin* ::sflphone::PluginManager::isPluginLoaded( const std::string &name )
{
    if(_loadedPlugins.empty())    return NULL;  

    /* Use an iterator on the loaded plugins map */
    pluginMap::iterator iter;

    iter = _loadedPlugins.begin();
    while( iter != _loadedPlugins.end() ) {
        if ( iter->first == name ) {
            /* Return the associated plugin */
            return iter->second;
        }
        iter++;
    }

    /* If we are here, it means that the plugin we were looking for has not been loaded */
    return NULL;
}

void* ::sflphone::PluginManager::loadDynamicLibrary( const std::string& filename ) {

    void *pluginHandlePtr = NULL;
    const char *error;

    _debug("Loading dynamic library %s\n", filename.c_str());

#if defined(Q_OS_UNIX)
    /* Load the library */
    pluginHandlePtr = dlopen( filename.c_str(), RTLD_LAZY );
    if( !pluginHandlePtr ) {
        error = dlerror();
        _debug("Error while opening plug-in: %s\n", error);
    }
    dlerror();
#endif

    return pluginHandlePtr;
}

void ::sflphone::PluginManager::unloadDynamicLibrary( void * pluginHandlePtr ) {
    
    dlclose( pluginHandlePtr );
    dlerror();
}

::sflphone::PluginManager* ::sflphone::PluginManager::_instance = 0;

