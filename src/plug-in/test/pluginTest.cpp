#include "../plugininterface.h" 
#include "../plugin.h"

namespace sflphone {

    class PluginTest : public PluginInterface {
    
        public:
            PluginTest( const std::string &name ):PluginInterface( name ){
            }

            virtual int initFunc (void)
            {
                return 0;
            }

            virtual int registerFunc (Plugin **plugin)
            {
                Plugin *ret;

                ret = new Plugin(this);
                
                ret->_name = getInterfaceName();
                ret->_required = 1;
                ret->_version_major=1;
                ret->_version_minor=0;

                *plugin = ret;
                return 0;
            }
    };

}
extern "C" ::sflphone::PluginInterface* create (void){
    return new ::sflphone::PluginTest("test");
}

extern "C" void* destroy( ::sflphone::PluginInterface *p ){
    delete p;
}
