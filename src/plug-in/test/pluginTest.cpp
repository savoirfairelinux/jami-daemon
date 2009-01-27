#include "../plugin_api.h" 

class PluginTest : public Plugin {
    public:
        PluginTest():Plugin(){
        }
};

extern "C" Plugin* create_t( void * ){
    return new PluginTest();
}

extern "C" void* destroy_t( Plugin *p ){
    delete p;
}
