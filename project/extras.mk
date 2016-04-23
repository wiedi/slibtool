CFLAGS_SHARED_ATTR	+= -DSLBT_PRE_ALPHA -DSLBT_BUILD
CFLAGS_STATIC_ATTR	+= -DSLBT_PRE_ALPHA -DSLBT_STATIC
CFLAGS_APP_ATTR		+= -DSLBT_APP

CFLAGS_MACHINE		:= -DSLBT_MACHINE=\"$(shell $(CC) $(CFLAGS) -dumpmachine)\"

src/driver/slbt_driver_ctx.o:	CFLAGS += $(CFLAGS_MACHINE)
src/driver/slbt_driver_ctx.lo:	CFLAGS += $(CFLAGS_MACHINE)

install-app-extras:	app

install-app-extras:
	mkdir -p $(DESTDIR)$(BINDIR)

	rm -f bin/$(PACKAGE)-shared$(OS_APP_SUFFIX).tmp
	rm -f bin/$(PACKAGE)-static$(OS_APP_SUFFIX).tmp

	ln -s ./$(PACKAGE)$(OS_APP_SUFFIX) bin/$(PACKAGE)-shared$(OS_APP_SUFFIX).tmp
	ln -s ./$(PACKAGE)$(OS_APP_SUFFIX) bin/$(PACKAGE)-static$(OS_APP_SUFFIX).tmp

	mv bin/$(PACKAGE)-shared$(OS_APP_SUFFIX).tmp $(DESTDIR)$(BINDIR)/$(PACKAGE)-shared$(OS_APP_SUFFIX)
	mv bin/$(PACKAGE)-static$(OS_APP_SUFFIX).tmp $(DESTDIR)$(BINDIR)/$(PACKAGE)-static$(OS_APP_SUFFIX)
