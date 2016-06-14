
default: build

check:
	@cd src;		$(MAKE) -s $@

docs:
	@cd doc;		$(MAKE)
build:
	@cd src;		$(MAKE) $@

install:
	@cd src;		$(MAKE) $@

clean :
	@cd src;		$(MAKE) $@
	@cd doc;		$(MAKE) $@
	@cd examples;	$(MAKE) $@
	@cd doc;		$(MAKE) $@
	@cd tests;		$(MAKE) $@

cleanall: clean

backup: clean
