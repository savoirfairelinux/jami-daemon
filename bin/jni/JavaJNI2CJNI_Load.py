#!/usr/bin/python
# -*- coding: latin-1 -*-
#
#  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
#
#  Author: Emeric Vigier <emeric.vigier@savoirfairelinux.com>
#          Alexandre Lision <alexandre.lision@savoirfairelinux.com>
#          Adrien Béraud <adrien.beraud@savoirfairelinux.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

import getopt, sys
import re
from string import Template

def type_to_signature(itype):
	if len(itype) > 2:
		if itype[-2:] == '[]':
			return "[%s" % type_to_signature(itype[:-2])
	if itype == "int":
		return "I"
	if itype == "long":
		return "J"
	if itype == "void":
		return "V"
	if itype == "boolean":
		return "Z"
	if itype == "byte":
		return "B"
	if itype == "char":
		return "C"
	if itype == "short":
		return "S"
	if itype == "float":
		return "F"
	if itype == "double":
		return "D"
	if itype == "String":
		return "Ljava/lang/String;"
	if itype == "Object" or itype == "java.lang.Object":
		return "Ljava/lang/Object;"
	return "Lnet/jami/daemon/%s;" % itype.replace('.', '$')

def parse_java_file(input_stream, package, module):
	outputs = []
	package_prefix = "Java_%s_%sJNI" % (package.replace(".", "_"), module)
	for line in input_stream:
		definition = re.match(r'.*public final static native ([^\( ]*) ([^\)]*)\(([^)]*)\).*',line)
		if definition is not None:
			retour = definition.group(1)
			name = definition.group(2)
			args = definition.group(3)
			args_sigs = []
			args_frags = args.split(',')
			for args_frag in args_frags:
				argf = re.match(r'(\b)?([^ ]+) .*', args_frag.strip())
				if argf is not None:
					args_sigs.append(type_to_signature(argf.group(2)))
			sig = "(%s)%s" % (''.join(args_sigs), type_to_signature(retour))
			outputs.append("{\"%s\", \"%s\", (void*)& %s_%s}" % (name, sig, package_prefix, name.replace('_', '_1')))
	return outputs

def render_to_template(defs, template_string):
	template = Template(template_string)
	return template.substitute(defs= ",\r\n".join(defs) )


if __name__ == "__main__":
	try:
		opts, args = getopt.getopt(sys.argv[1:], "i:o:t:m:p:", ["input=", "output=", "template=", "module=", "package="])
	except getopt.GetoptError as err:
		# print help information and exit:
		print(str(err)) # will print something like "option -a not recognized"
		sys.exit(2)
	input_stream = None
	output_file = None
	template_string = None
	package = ""
	module = ""
	for o, a in opts:
		if o in ("-i", "--input"):
			input_stream = open(a)
		if o in ("-o", "--output"):
			output_file = open(a, "w")
		if o in ("-t", "--template"):
			template_string = open(a).read()
		if o in ("-m", "--module"):
			module = a
		if o in ("-p", "--package"):
			package = a

	defs = parse_java_file(input_stream, package, module)
	output_file.write(render_to_template(defs, template_string))
	output_file.close()
	input_stream.close()
