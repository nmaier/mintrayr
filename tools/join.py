# Public domain - copyright is disclaimed
import os, sys, re, shutil
from zipfile import ZipFile, ZIP_DEFLATED, ZIP_STORED
from xml.dom.minidom import parseString
from glob import glob

NS_EM = 'http://www.mozilla.org/2004/em-rdf#'
tarextract = re.compile(r'_(.+)\.xpi$')
components = re.compile(r'components/.')
components_xpt = re.compile(r'\.xpt$')
components_ex = re.compile(r'\.(?:xpt|list)$')
bcomp = re.compile('binary-component')
jarfiles = re.compile('chrome/mintrayr/')

def mapper(f):
    x = tarextract.search(f)
    return f, x.group(1)
    

xpis = map(mapper, glob("*_*.xpi"))
src = xpis[0][0]
for x,p in xpis:
    if p == 'WINNT_x86-msvc':
        src = x
print "src", src
tar = tarextract.sub('-xp.xpi', src)

sxpi = ZipFile(src, 'r')
txpi = ZipFile(tar, 'w', ZIP_DEFLATED)
jar = ZipFile("mintrayr.jar", 'w', ZIP_STORED)

for zf in sxpi.namelist():
    if components.match(zf) and not components_xpt.search(zf):
        print zf
        continue
    if zf == 'install.rdf':
        continue
    if zf == 'chrome.manifest':
        continue
    m = jarfiles.match(zf)
    if m:
        zff = jarfiles.sub('', zf)
        if (zff):
            jar.writestr(zff, sxpi.read(zf))
        continue
    txpi.writestr(zf, sxpi.read(zf))
jar.close()
txpi.write('mintrayr.jar', 'chrome/mintrayr.jar')
os.unlink('mintrayr.jar')
    
xml = parseString(sxpi.read('install.rdf'))

ot = xml.getElementsByTagNameNS(NS_EM, 'targetPlatform')[0]
n = ot.parentNode
comps = ()
for x, p in xpis:
    print x,p
    nt = xml.createElementNS(NS_EM, 'em:targetPlatform')
    nt.appendChild(xml.createTextNode(p))
    n.insertBefore(xml.createTextNode('\n'), n.firstChild)    
    n.insertBefore(nt, n.firstChild)
    n.insertBefore(xml.createTextNode('\n'), n.firstChild)    
    zf = ZipFile(x, 'r')
    for f in zf.namelist():
        if not components.match(f) or components_ex.search(f):
            continue
        print f
        nf = "platform/%s/components/%s" % (p, os.path.basename(f))
        comps += (nf,p),
        txpi.writestr(nf, zf.read(f))
    zf.close()

n.removeChild(ot)
txpi.writestr('install.rdf', xml.toxml('utf-8'))

cm = ()
for l in str.splitlines(sxpi.read('chrome.manifest')):
    if bcomp.match(l):
        continue
    cm += re.sub(r'(\s)chrome/mintrayr/', r'\1jar:chrome/mintrayr.jar!/', l.strip()),
for comp in comps:
    cm += ("binary-component %s ABI=%s" % comp),
txpi.writestr('chrome.manifest', "\n".join(sorted(set(cm))))

sxpi.close()
txpi.close()
