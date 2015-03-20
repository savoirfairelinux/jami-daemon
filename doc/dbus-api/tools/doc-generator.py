#!/usr/bin/env python
#
# doc-generator.py
#
# Generates HTML documentation from the parsed spec using Cheetah templates.
#
# Copyright (C) 2009 Collabora Ltd.
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or (at
# your option) any later version.
#
# This library is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
# for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Authors: Davyd Madeley <davyd.madeley@collabora.co.uk>
#

import sys
import os
import os.path
import shutil

try:
    from Cheetah.Template import Template
except ImportError, e:
    print >> sys.stderr, e
    print >> sys.stderr, "Install `python-cheetah'?"
    sys.exit(-1)

import specparser

program, spec_file, output_path, project, namespace = sys.argv

template_path = os.path.join(os.path.dirname(program), '../doc/templates')

# make the output path
try:
    os.mkdir(output_path)
except OSError:
    pass
# copy in the CSS
shutil.copy(os.path.join(template_path, 'style.css'), output_path)

def load_template(filename):
    try:
        file = open(os.path.join(template_path, filename))
        template_def = file.read()
        file.close()
    except IOError, e:
        print >> sys.stderr, "Could not load template file `%s'" % filename
        print >> sys.stderr, e
        sys.exit(-1)

    return template_def

spec = specparser.parse(spec_file, namespace)

# write out HTML files for each of the interfaces

# Not using render_template here to avoid recompiling it n times.
namespace = {}
template_def = load_template('interface.html')
t = Template(template_def, namespaces = [namespace])
for interface in spec.interfaces:
    namespace['interface'] = interface

    # open the output file
    out = open(os.path.join(output_path, '%s.html' % interface.name), 'w')
    print >> out, unicode(t).encode('utf-8')
    out.close()

def render_template(name, namespaces, target=None):
    if target is None:
        target = name

    namespace = { 'spec': spec }
    template_def = load_template(name)
    t = Template(template_def, namespaces=namespaces)
    out = open(os.path.join(output_path, target), 'w')
    print >> out, unicode(t).encode('utf-8')
    out.close()

namespaces = { 'spec': spec }

render_template('generic-types.html', namespaces)
render_template('errors.html', namespaces)
render_template('interfaces.html', namespaces)
render_template('fullindex.html', namespaces)

dh_namespaces = { 'spec': spec, 'name': project }
render_template('devhelp.devhelp2', dh_namespaces,
    target=('%s.devhelp2' % project))

# write out the TOC last, because this is the file used as the target in the
# Makefile.
render_template('index.html', namespaces)
