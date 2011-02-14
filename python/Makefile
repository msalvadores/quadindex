include ../darwin.mk

IDIR =/usr/include/python2.6
SWIGFLAGS=-python `pkg-config --cflags $(pkgs)`
pkgs = kyotocabinet raptor2 glib-2.0 gthread-2.0 json-glib-1.0
CC=gcc
CFLAGS =  -D_GNU_SOURCE -fstrict-aliasing -Wstrict-aliasing=2 -O3 -I./ -I../  `pkg-config --cflags $(pkgs)` -I$(IDIR)
LDFLAGS = $(ldfdarwin) $(ldflinux) `pkg-config --libs  $(pkgs)` -I$(IDIR)

LIB_OBJS=quad_index_wrap.o
#OBJ_DEPS=../storage/crs-bind.o ../storage/crs-matrix.o ../storage/crs-matrix-io.o ../storage/rdf-kb.o ../common/umac.o ../common/rijndael-alg-fst.o ../common/hash.o ../common/error.o ../common/rdf-parser.o
OBJ_DEPS= ../storage/crs-bind.o ../storage/crs-matrix.o ../storage/crs-matrix-io.o ../storage/rdf-kb.o ../common/lib4shash.a 

CGENERATED=quad_index_wrap.c

all: quad_store.so

quad_store.o: quad_index_wrap.o

swig: quad_index.i
	swig $(SWIGFLAGS) quad_index.i 

quad_index_wrap.o: swig
	$(CC) -c -fPIC $(CGENERATED) $(CFLAGS)

quad_store.so: $(OBJS_DEP) $(LIB_OBJS) 
	$(CC) -dynamiclib -lpython $(LDFLAGS) $(OBJ_DEPS) $(LIB_OBJS) -o  _quad_storage.so

clean: 
	rm -f $(BINS) $(CGENERATED)
	rm -rf *.dSYM
	rm -rf *.o
	rm -rf *.pyc
	rm -rf *.so
	rm -f $(LIB_OBJS)
