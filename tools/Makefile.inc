TOOLS = $(BUILDDIR)/conf2json

TOOLS_CFLAGS = $(CFLAGS)
TOOLS_LDLIBS = -lconfig -ljansson -lvillas

# Compile executable objects
$(BUILDDIR)/tools/%.o: tools/%.c | $$(dir $$@)
	$(CC) $(TOOLS_CFLAGS) -c $< -o $@

# Link target executables
$(TOOLS): $(BUILDDIR)/%: $(BUILDDIR)/tools/%.o | $(LIBS)
	$(CC) $(TOOLS_LDFLAGS) $^ $(TOOLS_LDLIBS) -o $@

tools: $(TOOLS)

clean-tools:
	rm -rf $(BUILDDIR)/tools $(TOOLS)

install-tools: $(TOOLS)
	install -m 0755 tools/villas.sh $(PREFIX)/bin/villas
	install -m 0755 -D -t $(DESTDIR)$(PREFIX)/bin $(TOOLS)

.PHONY: tools clean-tools install-tools