#include "../plugin_api.h" 

namespace sflphone {

    class PluginTest : public Plugin {
        public:
            PluginTest( const std::string &name ):Plugin( name ){
            }

            virtual int getCoreVersion() const{
                return 1;
            }
    };

}
extern "C" ::sflphone::Plugin* create_t( void * ){
    return new ::sflphone::PluginTest("test");
}

extern "C" void* destroy_t( ::sflphone::Plugin *p ){
    delete p;
}
