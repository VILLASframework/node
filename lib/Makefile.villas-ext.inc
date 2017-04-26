LIBEXT_NAME = libvillas-ext
LIBEXT_ABI_VERSION = 1
LIBEXT = $(BUILDDIR)/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)

LIBEXT_SRCS += $(addprefix lib/, sample.c queue.c queue_signalled.c \
                 memory.c log.c shmem.c utils.c kernel/kernel.c list.c \
                 timing.c \
               )

LIBEXT_LDFLAGS = -shared
LIBEXT_LDLIBS = -ldl -lrt -Wl,-soname,$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)

LIBEXT_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(LIBEXT_SRCS))

$(LIBEXT_NAME): $(LIBEXT)

$(LIBEXT): $(LIBEXT_OBJS)
	$(CC) $(LIBEXT_LDFLAGS) -o $@ $^ $(LIBEXT_LDLIBS)
	ln -srf $@ $(BUILDDIR)/$(LIBEXT_NAME).so

install-libvillas-ext: libvillas-ext
	install -m 0755 -D -T $(LIBEXT) $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION)
	ln -srf $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so.$(LIBEXT_ABI_VERSION) $(DESTDIR)$(PREFIX)/lib/$(LIBEXT_NAME).so

clean-libvillas-ext:
	rm -rf $(BUILDDIR)/lib $(LIBEXT)

.PHONY: install-libvillas-ext clean-libvillas-ext $(LIBEXT_NAME)
