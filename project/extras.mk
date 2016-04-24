CFLAGS_SHARED_ATTR	+= -DSLBT_PRE_ALPHA -DSLBT_BUILD
CFLAGS_STATIC_ATTR	+= -DSLBT_PRE_ALPHA -DSLBT_STATIC
CFLAGS_APP_ATTR		+= -DSLBT_APP

CFLAGS_MACHINE		:= -DSLBT_MACHINE=\"$(shell $(CC) $(CFLAGS) -dumpmachine)\"

src/driver/slbt_driver_ctx.o:	CFLAGS += $(CFLAGS_MACHINE)
src/driver/slbt_driver_ctx.lo:	CFLAGS += $(CFLAGS_MACHINE)

install-app-extras:	app

install-app-extras:
	mkdir -p $(DESTDIR)$(BINDIR)

	rm -f bin/$(NICKNAME)-shared$(OS_APP_SUFFIX).tmp
	rm -f bin/$(NICKNAME)-static$(OS_APP_SUFFIX).tmp

	ln -s ./$(NICKNAME)$(OS_APP_SUFFIX) bin/$(NICKNAME)-shared$(OS_APP_SUFFIX).tmp
	ln -s ./$(NICKNAME)$(OS_APP_SUFFIX) bin/$(NICKNAME)-static$(OS_APP_SUFFIX).tmp

	mv bin/$(NICKNAME)-shared$(OS_APP_SUFFIX).tmp $(DESTDIR)$(BINDIR)/$(NICKNAME)-shared$(OS_APP_SUFFIX)
	mv bin/$(NICKNAME)-static$(OS_APP_SUFFIX).tmp $(DESTDIR)$(BINDIR)/$(NICKNAME)-static$(OS_APP_SUFFIX)
