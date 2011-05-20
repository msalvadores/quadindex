include ../darwin.mk

pkgs = kyotocabinet raptor2 glib-2.0 gthread-2.0 json-glib-1.0

LIB_OBJS = crs-matrix-list.o crs-matrix.o crs-matrix-io.o crs-bind.o rdf-kb.o

CFLAGS =  -D_GNU_SOURCE -fstrict-aliasing -Wstrict-aliasing=2 -std=c99 -O3 -I./ -I../  `pkg-config --cflags $(pkgs)` -ltcmalloc
LDFLAGS = $(ldfdarwin) $(ldflinux) `pkg-config --libs  $(pkgs)` 

BINS = rdf-import rdf-dump rdf-query test_bind

all: $(BINS)

rdf-import: crs-matrix-list.o crs-bind.o crs-matrix.o crs-matrix-io.o rdf-kb.o ../common/lib4shash.a
rdf-dump: crs-matrix-list.o crs-bind.o crs-matrix.o crs-matrix-io.o rdf-kb.o ../common/lib4shash.a
rdf-query: crs-matrix-list.o crs-bind.o crs-matrix.o crs-matrix-io.o rdf-kb.o ../common/lib4shash.a 
test_bind: crs-matrix-list.o crs-bind.o crs-matrix.o crs-matrix-io.o rdf-kb.o ../common/lib4shash.a 

clean: 
	rm -f $(BINS)
	rm -rf *.dSYM
	rm -rf *.o
