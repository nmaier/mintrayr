import os, sys, re
# public domain - copyright is disclaimed
from os.path import basename, dirname

from codecs import open as copen
from glob import glob
from xml.dom import XMLNS_NAMESPACE
from xml.dom.minidom import parse as XML
from xml.dom.minidom import parseString as XMLString

BASEPROPS = ['name', 'description', 'creator', 'homepageURL', 'developer', 'translator', 'contributor']

class PropDict(dict):
    def add(self,k,v):
        try:
            self[k] += v,
        except:
            self[k] = [v]
    def mergene(self, rhs):
        for k, v in rhs.items():
            if not k in self:
                self[k] = v

def parseProps(fn):
    rv = PropDict()
    with copen(fn, 'rU', 'utf-8') as fp:
        for line in fp:
            try:
                k,v = map(lambda x: x.strip(), line.split('=', 1))
                k = k.split('.')
                if len(k[-1]) < 3:
                    k = k[-2]
                else:
                    k = k[-1]
                if not k or not v:
                    continue
                rv.add(k, v)
            except Exception,e:
                print e
                pass
    return rv

try:
    install = XML('install.rdf')
except:
    with open('install.rdf', 'rb') as f:
        lines = f.readlines()[1:]
        install = XMLString('\n'.join(lines))
    
for e in install.getElementsByTagName('em:localized'):
    e.parentNode.removeChild(e)

baseprops = PropDict()
for p in BASEPROPS:
    for e in install.getElementsByTagName('em:' + p):
        baseprops.add(e.tagName[3:], e.firstChild.data)
    
document = XMLString("""<?xml version="1.0"?>
<locales xmlns="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:em="http://www.mozilla.org/2004/em-rdf#">
</locales>""")

def getFiles():
    for f in glob('*/*/*/description.properties'):
        yield f
    for f in glob('*/*/*/localized.properties'):
        yield f

for f in getFiles():
    locale = basename(dirname(f))
    props = parseProps(f)
    if not props:
        continue
    props.add('locale', locale)
    props.mergene(baseprops)
    print props
    localized = document.createElement('em:localized')
    desc = document.createElement('Description')
    localized.appendChild(desc)
    for k,vv in props.items():
        for v in vv:
            n = document.createElement('em:' + k)
            n.appendChild(document.createTextNode(v))
            desc.appendChild(n)
    document.documentElement.appendChild(localized)

document.documentElement.appendChild(document.createTextNode('\n'))

with copen('localized.xml', 'wb', 'utf-8') as op:
    op.write(document.toxml())

