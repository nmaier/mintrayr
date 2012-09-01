import zlib

class StreamPositionRestore(object):
    '''Stream position restore contextmanager helper'''

    def __init__(self, stream):
        self.stream = stream

    def __enter__(self):
        self.__pos = None
        try:
            self.__pos = self.stream.tell()
        except:
            pass
        return self

    def __exit__(self, type, value, traceback):
        if self.__pos is not None:
            try:
                self.stream.seek(self.__pos, 0)
            except:
                pass
        return False


class ZipFileMinorCompression(object):
    '''Minor compression zipfile contextmanager helper'''

    __orig_compressobj = zlib.compressobj

    @classmethod
    def __minor_compressobj(cls, compression, type, hint):
        # always use a compression level of 2 for xpi optimized compression
        return cls.__orig_compressobj(2, type, hint)

    def __init__(self, minor_compression=True):
        self.__minor_compression = minor_compression

    def __enter__(self):
        if self.__minor_compression:
            zlib.compressobj = self.__minor_compressobj
        return self

    def __exit__(self, type, value, traceback):
        if self.__minor_compression:
            zlib.compressobj = self.__orig_compressobj
        return False

