SUBDIRS = common storage

all clean install:
	for i in $(SUBDIRS); do \
		(cd $$i && $(MAKE) -w $@) || break; \
	done
