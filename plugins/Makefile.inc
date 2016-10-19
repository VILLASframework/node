# Plugins
PLUGINS = $(BUILDDIR)/simple_circuit.so \
	  $(BUILDDIR)/example_hook.so

PLUGIN_CFLAGS = -fPIC -DVILLAS -I../include/villas

# Dependencies for plugins
$(BUILDDIR)/example_hook.so:   $(BUILDDIR)/plugins/hooks/example_hook.o
$(BUILDDIR)/simple_circuit.so: $(BUILDDIR)/plugins/models/simple_circuit.o

plugins: $(PLUGINS)

# Compile
$(BUILDDIR)/plugins/%.o: plugins/%.c | $$(dir $$@)
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -c $< -o $@

# Link
$(PLUGINS):
	$(CC) $(LIB_LDFLAGS) -o $@ $^ $(LIB_LDLIBS)

# Plugins are not installed to the system for now...
install-plugins:
	
clean-plugins:
	rm -rf $(BUILDDIR)/plugins $(PLUGINS)

.PHONY: plugins install-plugins clean-plugins