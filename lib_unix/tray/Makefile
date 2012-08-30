CC = gcc
deps = gdk-2.0 gdk-pixbuf-2.0 atk
platform = $(shell uname -p)

CFLAGS += -Os -fPIC -g -mtune=generic $(shell pkg-config --cflags $(deps))
LDFLAGS += -shared -rdynamic -Wl,--version-script -Wl,export-versionscript
LIBS += $(shell pkg-config --libs $(deps))

libs = tray_$(platform)-gcc3.so

all: $(libs)

tray_$(platform)-gcc3.so: tray.o
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

strip: $(libs)
	strip $<

clean:
	rm -rf $(libs) *.o

cp:
	cp $(libs) ../../lib/

install: clean all strip cp

.PHONY: all clean strip cp install
