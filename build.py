import sys

from build import build

try:
    xpi = sys.argv[1]
except:
    from xml.dom.minidom import parse
    dom = parse("install.rdf")
    version = dom.getElementsByTagName("em:version")[0].firstChild.nodeValue
    dom.unlink()
    xpi = "mintrayr-{}.xpi".format(version)
sys.exit(build(".", xpi))
