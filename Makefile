
default: build

build install uninstall where:
	@cd src;		$(MAKE) $@
	#@cd src;		$(MAKE) -s $@

clean :
	@cd src;		$(MAKE) $@
	@cd doc;		$(MAKE) $@
	@cd examples/c_api;		$(MAKE) $@

docs:
	@cd doc;		$(MAKE)

cleanall: clean

backup: clean
