#include <dirent.h>
#include <dlfcn.h>

#include "pluginmanager.h"

::sflphone::PluginManager* ::sflphone::PluginManager::_instance = 0;

    ::sflphone::PluginManager* 
::sflphone::PluginManager::instance()
{
    if(!_instance){
        return new PluginManager();
    }
    return _instance;
}

::sflphone::PluginManager::PluginManager()
    :_loadedPlugins()
{
    _instance = this;
}

::sflphone::PluginManager::~PluginManager()
{
    _instance = 0;
}

    int 
::sflphone::PluginManager::loadPlugins (const std::string &path)
{
    std::string pluginDir, current;
    ::sflphone::PluginInterface *interface;
    DIR *dir;
    dirent *dirStruct;
    int result=0;
    void *handle;
    createFunc* createPlugin;
    
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
                handle = loadDynamicLibrary( pluginDir + current );
                
                if(instanciatePlugin (handle, &interface) != 0)       	        
                {
                    _debug("Error instanciating the plugin ...\n");
                    return 1;
                }
                
                if(registerPlugin (handle, interface) != 0)
                    _debug("Error registering the plugin ...\n");
                    return 1;
            }
	    }
    }
    else
        return 1;

    /* Close the directory */
    closedir( dir );

    return 0;
}

    ::sflphone::Plugin* 
::sflphone::PluginManager::isPluginLoaded (const std::string &name)
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


    void* 
::sflphone::PluginManager::loadDynamicLibrary (const std::string& filename) 
{

    void *pluginHandlePtr = NULL;
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

    int 
::sflphone::PluginManager::instanciatePlugin (void *handlePtr, ::sflphone::PluginInterface **plugin)
{
    createFunc *createPlugin;

    createPlugin = (createFunc*)dlsym(handlePtr, "create");
    if( dlerror() )
    {
        _debug("Error creating the plugin: %s\n", dlerror());
        return 1;
    }
    *plugin = createPlugin();
    return 0;
}

    int 
::sflphone::PluginManager::registerPlugin (void *handlePtr, PluginInterface *interface)
{
    Plugin *myplugin;
    std::string name;

    if( !( handlePtr && interface!=0 ) )
        return 1;
    
    /* Fetch information from the loaded plugin interface */
    if(interface->registerFunc (&myplugin) != 0)
        return 1;
    /* Creation of the plugin wrapper */
    myplugin = new Plugin (handlePtr, interface);

    /* Add the data in the loaded plugin map */
    _loadedPlugins[ myplugin->_name ] = myplugin;
    return 0;
}

    void 
::sflphone::PluginManager::unloadDynamicLibrary (void * pluginHandlePtr) 
{
    dlclose( pluginHandlePtr );
    dlerror();
}


