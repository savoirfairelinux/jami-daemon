#
# library
#

env = WengoGetEnvironment()

env.ParseConfig('pkg-config --cflags --libs dbus-1')

libs = [
	'expat'
]
lib_path = []
include_path = [
	'include'
]
defines = {
	'DBUS_API_SUBJECT_TO_CHANGE':1,
	'DEBUG':1
}
headers = []
sources = [
	'src/connection.cpp',
	'src/debug.cpp',
	'src/dispatcher.cpp',
	'src/error.cpp',
	'src/eventloop.cpp',
	'src/interface.cpp',
	'src/introspection.cpp',
	'src/property.cpp',
	'src/message.cpp',
	'src/object.cpp',
	'src/pendingcall.cpp',
	'src/server.cpp',
	'src/types.cpp',
	'src/xml.cpp'
]

env.WengoAddDefines(defines)
env.WengoAddIncludePath(include_path)
env.WengoUseLibraries(libs)
env.WengoStaticLibrary('dbus-c++', sources)

#
# tools
#

tools_env = WengoGetEnvironment()

tools_libs = [
	'dbus-c++'
]
tools_defines = {
	'DBUS_API_SUBJECT_TO_CHANGE':1,
}
introspect_sources = [
	'tools/introspect.cpp',
]

xml2cpp_sources = [
	'tools/xml2cpp.cpp'
]

#tools_env.Append(LINKFLAGS = '-z origin')
#tools_env.Append(RPATH = env.Literal('\\$$ORIGIN\.'))

tools_env.WengoAddDefines(tools_defines)
tools_env.WengoAddIncludePath(include_path)
tools_env.WengoUseLibraries(tools_libs)

dbusxx_introspect = tools_env.WengoProgram('dbusxx-introspect', introspect_sources)
dbusxx_xml2cpp = tools_env.WengoProgram('dbusxx-xml2cpp', xml2cpp_sources)

#
# xml translator
#

def dbusxx_xml2cpp_emitter(target, source, env):
	env.Depends(target, dbusxx_xml2cpp)
	return (target, source)

dbusxx_xml2cpp_builder = Builder(action = dbusxx_xml2cpp[0].abspath + ' $SOURCE --adaptor=$TARGET',
	emitter = dbusxx_xml2cpp_emitter,
	suffix = '.h', src_suffix = '.xml')

Export('dbusxx_xml2cpp_builder')
