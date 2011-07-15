import os, sys, re

from io import BytesIO
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED

from path import path

jar_files = [
    "content/*",
    "content/*/*",
    "locale/*/*.dtd",
    "locale/*/*.properties",
    "skin/*.css",
    "skin/*.png"
    ]
xpi_files = [
    "install.rdf",
    "*.png",
    "LICENSE",
    "chrome/icons/default/*",
    "defaults/preferences/*",
    "lib/*.dll",
    "lib/*.so",
    "lib/SOURCES",
    "lib_win/tray/*.c",
    "lib_win/tray/*.cpp",
    "lib_win/tray/*.h",
    "lib_win/tray/*.def",
    "lib_win/tray/*.rc",
    "lib_win/tray/*.sln",
    "lib_win/tray/*.vcproj",
    "modules/*"
    ]

class ZipOutFile(ZipFile):
    def __init__(self, zfile, method=None):
        self.zfile = zfile
        zfile.seek(0,0)
        ZipFile.__init__(self, zfile, "a", method or ZIP_STORED)
    def __enter__(self):
        return self
    def __exit__(self, type, value, traceback):
        self.close()
        self.zfile.seek(0,0)
        del self.zfile


def zip_files(out, file_list, basedir):
    with ZipOutFile(out) as zp:
        for f in file_list:
            for fe in basedir.glob(f):
                if fe.isdir():
                    continue
                oe = fe[len(basedir)+1:]
                with open(fe, "rb") as fp:
                    zp.writestr(oe, fp.read())

def chromejar_line(line):
    pieces = re.split(r"\s+", line, 3)
    if pieces[0] in ["content"]:
        pieces[2] = pieces[2].replace("chrome/", "jar:chrome.jar!/")
    elif pieces[0] in ["skin", "locale"]:
        pieces[3] = pieces[3].replace("chrome/", "jar:chrome.jar!/")
    return " ".join(pieces)

def main():
    basedir = path(__file__).dirname()

    with BytesIO() as xpi:
        zip_files(xpi, xpi_files, basedir)
        with ZipOutFile(xpi) as zp:
            with BytesIO() as jar:
                zip_files(jar, jar_files, basedir / "chrome")
                zp.writestr("chrome.jar", jar.read())
            with open("chrome.manifest") as mf:
                manifest = "\n".join((chromejar_line(l.strip())
                                      for l in mf.readlines()
                                      ))
                zp.writestr("chrome.manifest", manifest)
        with open("mintrayr.xpi", "wb") as op:
            op.write(xpi.read())
    return 0

if __name__ == "__main__":
    sys.exit(main())
