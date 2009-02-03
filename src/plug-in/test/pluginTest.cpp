#include "../plugin.h" 

#define MAJOR_VERSION   1
#define MINOR_VERSION   0

class PluginTest : public Plugin {

    public:
        PluginTest( const std::string &name )
            :Plugin( name ) {
            }

        virtual int initFunc (PluginInfo **info) {

            (*info)->_plugin = this;
            (*info)->_major_version = MAJOR_VERSION;
            (*info)->_minor_version = MINOR_VERSION;
            (*info)->_name = getPluginName();

            return 0;
        }
};

extern "C" Plugin* createPlugin (void){
    return new PluginTest("mytest");
}

extern "C" void destroyPlugin (Plugin *p){
    delete p;
}
