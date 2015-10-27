#
# specparser.py
#
# Reads in a spec document and generates pretty data structures from it.
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
import xml.dom.minidom

import xincludator

XMLNS_TP = 'http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0'

class UnknownAccess(Exception): pass
class UnknownDirection(Exception): pass
class UnknownType(Exception): pass
class UnnamedItem(Exception): pass
class UntypedItem(Exception): pass
class UnsupportedArray(Exception): pass

def getText(dom):
    try:
        if dom.childNodes[0].nodeType == dom.TEXT_NODE:
            return dom.childNodes[0].data
        else:
            return ''
    except IndexError:
        return ''

def getChildrenByName(dom, namespace, name):
    return filter(lambda n: n.nodeType == n.ELEMENT_NODE and \
                            n.namespaceURI == namespace and \
                            n.localName == name,
                  dom.childNodes)

def build_name(namespace, name):
    """Returns a name by appending `name' to the namespace of this object.
    """
    return '.'.join(
        filter(lambda n: n is not None and n != '',
               [namespace, name.replace(' ', '')])
        )

class Base(object):
    """The base class for any type of XML node in the spec that implements the
       'name' attribute.

       Don't instantiate this class directly.
    """
    devhelp_name = ""

    def __init__(self, parent, namespace, dom):
        self.short_name = name = dom.getAttribute('name')
        self.namespace = namespace
        self.name = build_name(namespace, name)
        self.parent = parent

        try:
            self.docstring = getChildrenByName(dom, XMLNS_TP, 'docstring')[0]
        except IndexError:
            self.docstring = None

        try:
            self.added = getChildrenByName(dom, XMLNS_TP, 'added')[0]
        except IndexError:
            self.added = None

        try:
            self.deprecated = getChildrenByName(dom, XMLNS_TP, 'deprecated')[0]
        except IndexError:
            self.deprecated = None

        self.changed = getChildrenByName(dom, XMLNS_TP, 'changed')

        self.validate()

    def validate(self):
        if self.short_name == '':
            raise UnnamedItem("Node %s of %s has no name" % (
                self.__class__.__name__, self.parent))

    def get_type_name(self):
        return self.__class__.__name__

    def get_spec(self):
        return self.parent.get_spec()

    def get_root_namespace(self):
        return self.get_interface().name

    def get_interface(self):
        return self.parent.get_interface()

    def get_url(self):
        return "%s#%s" % (self.get_interface().get_url(), self.name)

    def _get_generic_with_ver(self, nnode, htmlclass, txt):
        if nnode is None:
            return ''
        else:
            # make a copy of this node, turn it into a HTML <div> tag
            node = nnode.cloneNode(True)
            node.tagName = 'div'
            node.baseURI = None
            node.setAttribute('class', htmlclass)

            try:
                node.removeAttribute('version')

                span = xml.dom.minidom.parseString(
                    ('<span class="version">%s\n</span>' % txt) %
                            nnode.getAttribute('version')).firstChild
                node.insertBefore(span, node.firstChild)
            except xml.dom.NotFoundErr:
                print >> sys.stderr, \
                    'WARNING: %s was %s, but gives no version' % (self, htmlclass)

            self._convert_to_html(node)

            return node.toxml().encode('ascii', 'xmlcharrefreplace')

    def get_added(self):
        return self._get_generic_with_ver(self.added, 'added',
                                          "Added in %s.")

    def get_deprecated(self):
        return self._get_generic_with_ver(self.deprecated, 'deprecated',
                                          "Deprecated since %s.")

    def get_changed(self):
        return '\n'.join(map(lambda n:
            self._get_generic_with_ver(n, 'changed', "Changed in %s."),
            self.changed))

    def get_docstring(self):
        """Get the docstring for this node, but do node substitution to
           rewrite types, interfaces, etc. as links.
        """
        if self.docstring is None:
            return ''
        else:
            # make a copy of this node, turn it into a HTML <div> tag
            node = self.docstring.cloneNode(True)
            node.tagName = 'div'
            node.baseURI = None
            node.setAttribute('class', 'docstring')

            self._convert_to_html(node)

            return node.toxml().encode('ascii', 'xmlcharrefreplace')

    def _convert_to_html(self, node):
        spec = self.get_spec()
        namespace = self.get_root_namespace()

        # rewrite <tp:rationale>
        for n in node.getElementsByTagNameNS(XMLNS_TP, 'rationale'):
            n.tagName = 'div'
            n.namespaceURI = None
            n.setAttribute('class', 'rationale')

        # rewrite <tp:type>
        for n in node.getElementsByTagNameNS(XMLNS_TP, 'type'):
            t = spec.lookup_type(getText(n))
            n.tagName = 'a'
            n.namespaceURI = None
            n.setAttribute('href', t.get_url())

        # rewrite <tp:member-ref>
        for n in node.getElementsByTagNameNS(XMLNS_TP, 'member-ref'):
            key = getText(n)
            try:
                o = spec.lookup(key, namespace=namespace)
            except KeyError:
                print >> sys.stderr, \
                    "WARNING: Key '%s' not known in namespace '%s'" % (
                        key, namespace)
                continue

            n.tagName = 'a'
            n.namespaceURI = None
            n.setAttribute('href', o.get_url())
            n.setAttribute('title', o.get_title())

        # rewrite <tp:dbus-ref>
        for n in node.getElementsByTagNameNS(XMLNS_TP, 'dbus-ref'):
            namespace = n.getAttribute('namespace')
            key = getText(n)
            try:
                o = spec.lookup(key, namespace=namespace)
            except KeyError:
                print >> sys.stderr, \
                    "WARNING: Key '%s' not known in namespace '%s'" % (
                        key, namespace)
                continue

            n.tagName = 'a'
            n.namespaceURI = None
            n.setAttribute('href', o.get_url())
            n.setAttribute('title', o.get_title())

    def get_title(self):
        return '%s %s' % (self.get_type_name(), self.name)

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, self.name)

class PossibleError(Base):
    def __init__(self, parent, namespace, dom):
        super(PossibleError, self).__init__(parent, namespace, dom)

    def get_error(self):
        spec = self.get_spec()
        try:
            return spec.errors[self.name]
        except KeyError:
            return External(self.name)

    def get_url(self):
        return self.get_error().get_url()

    def get_title(self):
        return self.get_error().get_title()

    def get_docstring(self):
        d = super(PossibleError, self).get_docstring()
        if d == '':
            return self.get_error().get_docstring()
        else:
            return d

class Method(Base):
    devhelp_name = "function"

    def __init__(self, parent, namespace, dom):
        super(Method, self).__init__(parent, namespace, dom)

        args = build_list(self, Arg, self.name,
                          dom.getElementsByTagName('arg'))

        # separate arguments as input and output arguments
        self.in_args = filter(lambda a: a.direction == Arg.DIRECTION_IN, args)
        self.out_args = filter(lambda a: a.direction == Arg.DIRECTION_OUT, args)

        for arg in args:
            if arg.direction == Arg.DIRECTION_IN or \
               arg.direction == Arg.DIRECTION_OUT:
                continue

            print >> sys.stderr, "WARNING: '%s' of method '%s' does not specify a suitable direction" % (arg, self)

        self.possible_errors = build_list(self, PossibleError, None,
                        dom.getElementsByTagNameNS(XMLNS_TP, 'error'))

    def get_in_args(self):
        return ', '.join(map(lambda a: a.spec_name(), self.in_args))

    def get_out_args(self):
        if len(self.out_args) > 0:
            return ', '.join(map(lambda a: a.spec_name(), self.out_args))
        else:
            return 'nothing'

class Typed(Base):
    """The base class for all typed nodes (i.e. Arg and Property).

       Don't instantiate this class directly.
    """

    def __init__(self, parent, namespace, dom):
        super(Typed, self).__init__(parent, namespace, dom)

        self.type = dom.getAttributeNS(XMLNS_TP, 'type')
        self.dbus_type = dom.getAttribute('type')

        # check we have a dbus type
        if self.dbus_type == '':
            raise UntypedItem("Node referred to by '%s' has no type" % dom.toxml())
    def get_type(self):
        return self.get_spec().lookup_type(self.type)

    def get_type_url(self):
        t = self.get_type()
        if t is None: return ''
        else: return t.get_url()

    def get_type_title(self):
        t = self.get_type()
        if t is None: return ''
        else: return t.get_title()

    def spec_name(self):
        return '%s: %s' % (self.dbus_type, self.short_name)

    def __repr__(self):
        return '%s(%s:%s)' % (self.__class__.__name__, self.name, self.dbus_type)

class Property(Typed):
    ACCESS_READ     = 1
    ACCESS_WRITE    = 2

    ACCESS_READWRITE = ACCESS_READ | ACCESS_WRITE

    def __init__(self, parent, namespace, dom):
        super(Property, self).__init__(parent, namespace, dom)

        access = dom.getAttribute('access')
        if access == 'read':
            self.access = self.ACCESS_READ
        elif access == 'write':
            self.access = self.ACCESS_WRITE
        elif access == 'readwrite':
            self.access = self.ACCESS_READWRITE
        else:
            raise UnknownAccess("Unknown access '%s' on %s" % (access, self))

    def get_access(self):
        if self.access & self.ACCESS_READ and self.access & self.ACCESS_WRITE:
            return 'Read/Write'
        elif self.access & self.ACCESS_READ:
            return 'Read only'
        elif self.access & self.ACCESS_WRITE:
            return 'Write only'

class AwkwardTelepathyProperty(Typed):
    def get_type_name(self):
        return 'Telepathy Property'

class Arg(Typed):
    DIRECTION_IN, DIRECTION_OUT, DIRECTION_UNSPECIFIED = range(3)

    def __init__(self, parent, namespace, dom):
        super(Arg, self).__init__(parent, namespace, dom)

        direction = dom.getAttribute('direction')
        if direction == 'in':
            self.direction = self.DIRECTION_IN
        elif direction == 'out':
            self.direction = self.DIRECTION_OUT
        elif direction == '':
            self.direction = self.DIRECTION_UNSPECIFIED
        else:
            raise UnknownDirection("Unknown direction '%s' on %s" % (
                                    direction, self.parent))

class Signal(Base):
    def __init__(self, parent, namespace, dom):
        super(Signal, self).__init__(parent, namespace, dom)

        self.args = build_list(self, Arg, self.name,
                               dom.getElementsByTagName('arg'))

        for arg in self.args:
            if arg.direction == Arg.DIRECTION_UNSPECIFIED:
                continue

            print >> sys.stderr, "WARNING: '%s' of signal '%s' does not specify a suitable direction" % (arg, self)

    def get_args(self):
        return ', '.join(map(lambda a: a.spec_name(), self.args))

class External(object):
    """External objects are objects that are referred to in another spec.

       We have to attempt to look them up if at all possible.
    """

    def __init__(self, name):
        self.name = self.short_name = name

    def get_url(self):
        return None

    def get_title(self):
        return 'External %s' % self.name

    def get_docstring(self):
        return None

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, self.name)

class Interface(Base):
    def __init__(self, parent, namespace, dom, spec_namespace):
        super(Interface, self).__init__(parent, namespace, dom)

        # If you're writing a spec with more than one top-level namespace, you
        # probably want to replace spec_namespace with a list.
        if self.name.startswith(spec_namespace + "."):
            self.short_name = self.name[len(spec_namespace) + 1:]
        else:
            self.short_name = self.name

        # build lists of methods, etc., in this interface
        self.methods = build_list(self, Method, self.name,
                                  dom.getElementsByTagName('method'))
        self.properties = build_list(self, Property, self.name,
                                     dom.getElementsByTagName('property'))
        self.signals = build_list(self, Signal, self.name,
                                  dom.getElementsByTagName('signal'))
        self.tpproperties = build_list(self, AwkwardTelepathyProperty,
                self.name, dom.getElementsByTagNameNS(XMLNS_TP, 'property'))
        self.handler_capability_tokens = build_list(self,
                HandlerCapabilityToken, self.name,
                dom.getElementsByTagNameNS(XMLNS_TP,
                    'handler-capability-token'))
        self.contact_attributes = build_list(self, ContactAttribute, self.name,
                dom.getElementsByTagNameNS(XMLNS_TP, 'contact-attribute'))

        # build a list of types in this interface
        self.types = parse_types(self, dom, self.name)

        # find out if this interface causes havoc
        self.causes_havoc = dom.getAttributeNS(XMLNS_TP, 'causes-havoc')
        if self.causes_havoc == '': self.causes_havoc = None

        # find out what we're required to also implement
        self.requires = map(lambda n: n.getAttribute('interface'),
                             getChildrenByName(dom, XMLNS_TP, 'requires'))

    def get_interface(self):
        return self

    def get_requires(self):
        spec = self.get_spec()

        def lookup(r):
            try:
                return spec.lookup(r)
            except KeyError:
                return External(r)

        return map(lookup, self.requires)

    def get_url(self):
        return '%s.html' % self.name

class Error(Base):
    def get_url(self):
        return 'errors.html#%s' % self.name

    def get_root_namespace(self):
        return self.namespace

class DBusList(object):
    """Stores a list of a given DBusType. Provides some basic validation to
       determine whether or not the type is sane.
    """
    def __init__(self, child):
        self.child = child

        if isinstance(child, DBusType):
            self.ultimate = child
            self.depth = 1

            if self.child.array_name == '':
                raise UnsupportedArray("Type '%s' does not support being "
                        "used in an array" % self.child.name)
            else:
                self.name = build_name(self.child.namespace,
                                       self.child.array_name)
                self.short_name = self.child.array_name

        elif isinstance(child, DBusList):
            self.ultimate = child.ultimate
            self.depth = child.depth + 1
            self.name = self.child.name + '_List'
            self.short_name = self.child.short_name + '_List'

            # check that our child can operate at this depth
            maxdepth = int(self.ultimate.array_depth)
            if self.depth > maxdepth:
                raise TypeError("Type '%s' has exceeded its maximum depth (%i)" % (self, maxdepth))

        else:
            raise TypeError("DBusList can contain only a DBusType or DBusList not '%s'" % child)

        self.dbus_type = 'a' + self.child.dbus_type

    def get_url(self):
        return self.ultimate.get_url()

    def get_title(self):
        return "Array of %s" % self.child.get_title()

    def __repr__(self):
        return 'Array(%s)' % self.child

class DBusType(Base):
    """The base class for all D-Bus types referred to in the spec.

       Don't instantiate this class directly.
    """

    devhelp_name = "typedef"

    def __init__(self, parent, namespace, dom):
        super(DBusType, self).__init__(parent, namespace, dom)

        self.dbus_type = dom.getAttribute('type')
        self.array_name = dom.getAttribute('array-name')
        self.array_depth = dom.getAttribute('array-depth')

    def get_root_namespace(self):
        return self.namespace

    def get_breakdown(self):
        return ''

    def get_url(self):
        if isinstance(self.parent, Interface):
            html = self.parent.get_url()
        else:
            html = 'generic-types.html'

        return '%s#%s' % (html, self.name)

class SimpleType(DBusType):
    def get_type_name(self):
        return 'Simple Type'

class ExternalType(DBusType):
    def __init__(self, parent, namespace, dom):
        super(ExternalType, self).__init__(parent, namespace, dom)

        # FIXME: until we are able to cross reference external types to learn
        # about their array names, we're just going to assume they work like
        # this
        self.array_name = self.short_name + '_List'

    def get_type_name(self):
        return 'External Type'

class StructLike(DBusType):
    """Base class for all D-Bus types that look kind of like Structs

       Don't instantiate this class directly.
    """

    class StructMember(Typed):
        def get_root_namespace(self):
            return self.parent.get_root_namespace()

    def __init__(self, parent, namespace, dom):
        super(StructLike, self).__init__(parent, namespace, dom)

        self.members = build_list(self, StructLike.StructMember, None,
                        dom.getElementsByTagNameNS(XMLNS_TP, 'member'))

    def get_breakdown(self):
        str = ''
        str += '<ul>\n'
        for member in self.members:
            # attempt to lookup the member up in the type system
            t = member.get_type()

            str += '<li>%s &mdash; %s' % (member.name, member.dbus_type)
            if t: str += ' (<a href="%s" title="%s">%s</a>)' % (
                            t.get_url(), t.get_title(), t.short_name)
            str += '</li>\n'
            str += member.get_docstring()
        str += '</ul>\n'

        return str

class Mapping(StructLike):
    def __init__(self, parent, namespace, dom):
        super(Mapping, self).__init__(parent, namespace, dom)

        # rewrite the D-Bus type
        self.dbus_type = 'a{%s}' % ''.join(map(lambda m: m.dbus_type, self.members))

class Struct(StructLike):

    devhelp_name = "struct"

    def __init__(self, parent, namespace, dom):
        super(Struct, self).__init__(parent, namespace, dom)

        # rewrite the D-Bus type
        self.dbus_type = '(%s)' % ''.join(map(lambda m: m.dbus_type, self.members))

class EnumLike(DBusType):
    """Base class for all D-Bus types that look kind of like Enums

       Don't instantiate this class directly.
    """
    class EnumValue(Base):
        def __init__(self, parent, namespace, dom):
            super(EnumLike.EnumValue, self).__init__(parent, namespace, dom)

            # rewrite self.name
            self.short_name = dom.getAttribute('suffix')
            self.name = build_name(namespace, self.short_name)

            self.value = dom.getAttribute('value')

            super(EnumLike.EnumValue, self).validate()

        def validate(self):
            pass

        def get_root_namespace(self):
            return self.parent.get_root_namespace()

    def get_breakdown(self):
        str = ''
        str += '<ul>\n'
        for value in self.values:
            # attempt to lookup the member.name as a type in the type system
            str += '<li>%s (%s)</li>\n' % (value.short_name, value.value)
            str += value.get_added()
            str += value.get_changed()
            str += value.get_deprecated()
            str += value.get_docstring()
        str += '</ul>\n'

        return str

class Enum(EnumLike):

    devhelp_name = "enum"

    def __init__(self, parent, namespace, dom):
        super(Enum, self).__init__(parent, namespace, dom)

        self.values = build_list(self, EnumLike.EnumValue, self.name,
                        dom.getElementsByTagNameNS(XMLNS_TP, 'enumvalue'))

class Flags(EnumLike):
    def __init__(self, parent, namespace, dom):
        super(Flags, self).__init__(parent, namespace, dom)

        self.values = build_list(self, EnumLike.EnumValue, self.name,
                        dom.getElementsByTagNameNS(XMLNS_TP, 'flag'))
        self.flags = self.values # in case you're looking for it

class TokenBase(Base):

    devhelp_name = "macro"      # it's a constant, which is near enough...
    separator = '/'

    def __init__(self, parent, namespace, dom):
        super(TokenBase, self).__init__(parent, namespace, dom)
        self.name = namespace + '/' + self.short_name

class ContactAttribute(TokenBase, Typed):

    def get_type_name(self):
        return 'Contact Attribute'

class HandlerCapabilityToken(TokenBase):

    def get_type_name(self):
        return 'Handler Capability Token'

    def __init__(self, parent, namespace, dom):
        super(HandlerCapabilityToken, self).__init__(parent, namespace, dom)

        is_family = dom.getAttribute('is-family')
        assert is_family in ('yes', 'no', '')
        self.is_family = (is_family == 'yes')

class SectionBase(object):
    """A SectionBase is an abstract base class for any type of node that can
       contain a <tp:section>, which means the top-level Spec object, or any
       Section object.

       It should not be instantiated directly.
    """

    def __init__(self, dom, spec_namespace):

        self.items = []

        def recurse(nodes):
            # iterate through the list of child nodes
            for node in nodes:
                if node.nodeType != node.ELEMENT_NODE: continue

                if node.tagName == 'node':
                    # recurse into this level for interesting items
                    recurse(node.childNodes)
                elif node.namespaceURI == XMLNS_TP and \
                     node.localName == 'section':
                    self.items.append(Section(self, None, node,
                        spec_namespace))
                elif node.tagName == 'interface':
                    self.items.append(Interface(self, None, node,
                        spec_namespace))

        recurse(dom.childNodes)

class Section(Base, SectionBase):
    def __init__(self, parent, namespace, dom, spec_namespace):
        Base.__init__(self, parent, namespace, dom)
        SectionBase.__init__(self, dom, spec_namespace)

    def get_root_namespace(self):
        return None

class Spec(SectionBase):
    def __init__(self, dom, spec_namespace):
        # build a dictionary of errors in this spec
        try:
            errorsnode = dom.getElementsByTagNameNS(XMLNS_TP, 'errors')[0]
            self.errors = build_dict(self, Error,
                        errorsnode.getAttribute('namespace'),
                        errorsnode.getElementsByTagNameNS(XMLNS_TP, 'error'))
        except IndexError:
            self.errors = {}

        # build a list of generic types
        self.generic_types = reduce (lambda a, b: a + b,
                map(lambda l: parse_types(self, l),
                        dom.getElementsByTagNameNS(XMLNS_TP, 'generic-types')),
                [])

        # create a top-level section for this Spec
        SectionBase.__init__(self, dom.documentElement, spec_namespace)

        # build a list of interfaces in this spec
        self.interfaces = []
        def recurse(items):
            for item in items:
                if isinstance(item, Section): recurse(item.items)
                elif isinstance(item, Interface): self.interfaces.append(item)
        recurse(self.items)

        # build a giant dictionary of everything (interfaces, methods, signals
        # and properties); also build a dictionary of types
        self.everything = {}
        self.types = {}

        for type in self.generic_types: self.types[type.short_name] = type

        for interface in self.interfaces:
                self.everything[interface.name] = interface

                for method in interface.methods:
                    self.everything[method.name] = method
                for signal in interface.signals:
                    self.everything[signal.name] = signal
                for property in interface.properties:
                    self.everything[property.name] = property
                for property in interface.tpproperties:
                    self.everything[property.name] = property
                for token in interface.contact_attributes:
                    self.everything[token.name] = token
                for token in interface.handler_capability_tokens:
                    self.everything[token.name] = token

                for type in interface.types:
                    self.types[type.short_name] = type

        # get some extra bits for the HTML
        node = dom.getElementsByTagNameNS(XMLNS_TP, 'spec')[0]
        self.title = getText(getChildrenByName(node, XMLNS_TP, 'title')[0])

        try:
            self.version = getText(getChildrenByName(node, XMLNS_TP, 'version')[0])
        except IndexError:
            self.version = None

        self.copyrights = map(getText,
                              getChildrenByName(node, XMLNS_TP, 'copyright'))

        try:
            license = getChildrenByName(node, XMLNS_TP, 'license')[0]
            license.tagName = 'div'
            license.namespaceURI = None
            license.setAttribute('class', 'license')
            self.license = license.toxml()
        except IndexError:
            self.license = ''

        # FIXME: we need to check all args for type correctness

    def get_spec(self):
        return self

    def lookup(self, name, namespace=None):
        key = build_name(namespace, name)
        return self.everything[key]

    def lookup_type(self, type_):
        if type_.endswith('[]'):
            return DBusList(self.lookup_type(type_[:-2]))

        if type_ == '': return None
        elif type_ in self.types:
            return self.types[type_]

        raise UnknownType("Type '%s' is unknown" % type_)

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, self.title)

def build_dict(parent, type_, namespace, nodes):
    """Build a dictionary of D-Bus names to Python objects representing that
       name using the XML node for that item in the spec.

       e.g. 'org.freedesktop.Telepathy.Channel' : Interface(Channel)

       Works for any Python object inheriting from 'Base' whose XML node
       implements the 'name' attribute.
    """

    def build_tuple(node):
        o = type_(parent, namespace, node)
        return(o.name, o)

    return dict(build_tuple(n) for n in nodes)

def build_list(parent, type_, namespace, nodes):
    return map(lambda node: type_(parent, namespace, node), nodes)

def parse_types(parent, dom, namespace = None):
    """Parse all of the types of type nodes mentioned in 't' from the node
       'dom' and insert them into the dictionary 'd'.
    """
    t = [
        (SimpleType,    'simple-type'),
        (Enum,          'enum'),
        (Flags,         'flags'),
        (Mapping,       'mapping'),
        (Struct,        'struct'),
        (ExternalType,  'external-type'),
    ]

    types = []

    for (type_, tagname) in t:
        types += build_list(parent, type_, namespace,
                    dom.getElementsByTagNameNS(XMLNS_TP, tagname))

    return types

def parse(filename, spec_namespace):
    dom = xml.dom.minidom.parse(filename)
    xincludator.xincludate(dom, filename)

    spec = Spec(dom, spec_namespace)

    return spec

if __name__ == '__main__':
    parse(sys.argv[1])
