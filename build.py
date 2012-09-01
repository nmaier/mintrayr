import sys

from build import build

try:
    xpi = sys.argv[1]
except:
    xpi = "mintrayr.xpi"
sys.exit(build(".", xpi))
