@INTLTOOL_XML_NOMERGE_RULE@
gsettings_in_file = org.sflphone.SFLphone.gschema.xml.in
gsettings_SCHEMAS = $(gsettings_in_file:.xml.in=.xml)
@GSETTINGS_RULES@

CLEANFILES = $(gsettings_SCHEMAS)

MAINTAINERCLEANFILES = $(gsettings_SCHEMAS:.xml=.valid)


#FIXME: is there a way to avoid calling this every time we do make?
all-local: $(gsettings_SCHEMAS)
	$(MKDIR_P) glib-2.0/schemas
	$(GLIB_COMPILE_SCHEMAS) . --targetdir glib-2.0/schemas
