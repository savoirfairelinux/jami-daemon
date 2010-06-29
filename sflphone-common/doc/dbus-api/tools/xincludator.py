#!/usr/bin/python

from sys import argv, stdout, stderr
import codecs, locale
import os
import xml.dom.minidom

stdout = codecs.getwriter('utf-8')(stdout)

NS_XI = 'http://www.w3.org/2001/XInclude'

def xincludate(dom, base, dropns = []):
    remove_attrs = []
    for i in xrange(dom.documentElement.attributes.length):
        attr = dom.documentElement.attributes.item(i)
        if attr.prefix == 'xmlns':
            if attr.localName in dropns:
                remove_attrs.append(attr)
            else:
                dropns.append(attr.localName)
    for attr in remove_attrs:
        dom.documentElement.removeAttributeNode(attr)
    for include in dom.getElementsByTagNameNS(NS_XI, 'include'):
        href = include.getAttribute('href')
        # FIXME: assumes Unixy paths
        filename = os.path.join(os.path.dirname(base), href)
        subdom = xml.dom.minidom.parse(filename)
        xincludate(subdom, filename, dropns)
        if './' in href:
            subdom.documentElement.setAttribute('xml:base', href)
        include.parentNode.replaceChild(subdom.documentElement, include)

if __name__ == '__main__':
    argv = argv[1:]
    dom = xml.dom.minidom.parse(argv[0])
    xincludate(dom, argv[0])
    xml = dom.toxml()
    stdout.write(xml)
    stdout.write('\n')
