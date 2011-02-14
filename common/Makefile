include ../darwin.mk

pkgs = glib-2.0 raptor2

LIB_OBJS = umac.o rijndael-alg-fst.o hash.o error.o rdf-parser.o

#CFLAGS = -Wall -g -std=gnu99 -O2 -I../ `pkg-config --cflags $(pkgs)`
CFLAGS = -Wall -g -std=gnu99 -I../ `pkg-config --cflags $(pkgs)`
LDFLAGS = $(ldfdarwin) $(ldflinux) `pkg-config --libs  $(pkgs)`

HEADERS = hash.h error.h params.h rdf-parser.h

all: lib4shash.a

lib4shash.a: $(LIB_OBJS) $(HEADERS)
	ar rvu lib4shash.a $(LIB_OBJS)
	ranlib lib4shash.a

clean: 
	rm -f $(LIB_OBJS)
	rm -rf *.dSYM
	rm -rf *.a	
	rm -rf *.o


