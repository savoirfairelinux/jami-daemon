
/* File : callmanager.i */
%module sflphoneservice

%include "typemaps.i"
%include "std_string.i" /* std::string typemaps */
%include "enums.swg"
%include "arrays_java.i";
%include "carrays.i";

/* void* shall be handled as byte arrays */
%typemap(jni) void * "void *"
%typemap(jtype) void * "byte[]"
%typemap(jstype) void * "byte[]"
%typemap(javain) void * "$javainput"
%typemap(in) void * %{
	$1 = $input;
%}
%typemap(javadirectorin) void * "$jniinput"
%typemap(out) void * %{
	$result = $1;
%}
%typemap(javaout) void * {
	return $jnicall;
}

/* not parsed by SWIG but needed by generated C files */
%{
#include <managerimpl.h>
#include <dbus/callmanager.h>

namespace Manager {
extern ManagerImpl& instance();
}
%}

/* parsed by SWIG to generate all the glue */
/* %include "../managerimpl.h" */
/* %include <dbus/callmanager.h> */

/* %nodefaultctor ManagerImpl;
%nodefaultdtor ManagerImpl; */

class ManagerImpl {
public:
    void init(const std::string &config_file);
    void setPath(const std::string &path);
    bool outgoingCall(const std::string&, const std::string&, const std::string&, const std::string& = "");
    void refuseCall(const std::string& id);
    bool answerCall(const std::string& id);
    void hangupCall(const std::string& id);
};

//%rename(Manager_instance) Manager::instance;

namespace Manager {

ManagerImpl& Manager::instance()
{
    // Meyers singleton
    static ManagerImpl instance_;
    return instance_;
}

}

//class CallManager {
//public:
//    /* Manager::instance().outgoingCall */
//    void placeCall(const std::string& accountID,
//                   const std::string& callID,
//                   const std::string& to);
//    /* Manager::instance().refuseCall */
//    void refuse(const std::string& callID);
//    /* Manager::instance().answerCall */
//    void accept(const std::string& callID);
//    /* Manager::instance().hangupCall */
//    void hangUp(const std::string& callID);
//};

#ifndef SWIG
/* some bad declarations */
#endif
