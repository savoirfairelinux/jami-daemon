#include "../plugin.h" 

namespace sflphone {

    class PluginTest : public Plugin {
    
        public:
            PluginTest( const std::string &name )
                :Plugin( name ) {
                }

            virtual int initFunc (int i)
            {
                return i;
            }

    };
}

extern "C" ::sflphone::Plugin* create (void){
    return new ::sflphone::PluginTest("mytest");
}

extern "C" void destroy (::sflphone::Plugin *p){
    delete p;
}
