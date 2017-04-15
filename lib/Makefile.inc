SONAMES = villas villas-ext

LIBS = $(patsubst %, $(BUILDDIR)/lib%.so, $(SONAMES))

LIB_CFLAGS  = $(CFLAGS) -fPIC

-include lib/hooks/Makefile.inc
-include lib/apis/Makefile.inc

-include $(patsubst %, lib/Makefile.%.inc, $(SONAMES))

# Add flags by pkg-config
LIB_CFLAGS += $(shell pkg-config --cflags ${LIB_PKGS})

# Compile
$(BUILDDIR)/lib/%.o: lib/%.c $(BUILDDIR)/defines | $$(dir $$@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

lib: $(patsubst %, lib%, $(SONAMES))

install-lib: $(patsubst %, install-lib%, $(SONAMES))
	
clean-lib: $(patsubst %, clean-lib%, $(SONAMES))

.PHONY: lib install-lib clean-lib
