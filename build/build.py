import os, sys, re

from datetime import datetime
from io import BytesIO
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED, ZIP_STORED

try:
    from path import path
except ImportError:
    print "Install path.py (via pip)"
    sys.exit(1)

try:
    from cmp_listed_locales import cmp_listed_locales
except ImportError, ex:
    from warnings import warn
    warn("cannot compare locales ({})".format(ex))
    def cmp_listed_locales(dirname):
        pass

from context import ZipFileMinorCompression

class ZipOutFile(ZipFile):
    def __init__(self, zfile, method=None):
        self.zfile = zfile
        zfile.seek(0, 0)
        ZipFile.__init__(self, zfile, "a", method or ZIP_STORED)
    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()
        self.zfile.seek(0, 0)
        del self.zfile

class ZipOutInfo(ZipInfo):
    def __init__(self, name):
        super(ZipOutInfo, self).__init__(name,
                                         (1980, 1, 1, 0, 0, 0)
                                         )


def zip_files(zp, file_list, basedir):
    for f in file_list:
        for fe in basedir.glob(f):
            if fe.isdir():
                continue
            oe = path(fe.replace("\\", "/"))[len(basedir):]
            with open(fe, "rb") as fp:
                zp.writestr(ZipOutInfo(oe), fp.read())

def chromejar_line(line):
    pieces = re.split(r"\s+", line, 3)
    if pieces[0] in ["content"]:
        pieces[2] = pieces[2].replace("chrome/", "jar:chrome.jar!/")
    elif pieces[0] in ["skin", "locale"]:
        pieces[3] = pieces[3].replace("chrome/", "jar:chrome.jar!/")
    return " ".join(pieces)

def build(basedir, outfile):
    basedir = path(basedir)
    if not basedir.endswith("/"):
        basedir += "/"
    jar_files = (path(__file__).dirname() / "jar.files").lines(retain=False)
    xpi_files = (path(__file__).dirname() / "xpi.files").lines(retain=False)

    print "verifying locales"
    try:
        cmp_listed_locales(basedir)
    except:
        print >> sys.stderr, "WARN: locales did not verify"

    with ZipFileMinorCompression(), BytesIO() as xpi:
        with ZipOutFile(xpi, method=ZIP_DEFLATED) as zp:
            print "zipping regular files"
            zip_files(zp, xpi_files, basedir)

            print "creating inner jar"
            with BytesIO() as jar:
                with ZipOutFile(jar) as jp:
                    zip_files(jp, jar_files, basedir / "chrome/")
                zp.writestr(ZipOutInfo("chrome.jar"), jar.read())

            print "writing manifest"
            with open("chrome.manifest") as mf:
                manifest = "\n".join((chromejar_line(l.strip())
                                      for l in mf.readlines()
                                      ))
                zp.writestr(ZipOutInfo("chrome.manifest"), manifest)

        print "writing xpi"
        with open(outfile, "wb") as op:
            op.write(xpi.read())
    print "done!"
    return 0
