#include "plugin.h" 

::sflphone::Plugin::Plugin (void *handle, PluginInterface *interface)
    :_handlePtr(handle), _interface(interface)
{
}

::sflphone::Plugin::Plugin (PluginInterface *interface)
    :_interface(interface)
{
}

::sflphone::Plugin::~Plugin ()
{
}
