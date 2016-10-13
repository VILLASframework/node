# Executables
TARGETS = $(BUILDDIR)/node \
	  $(BUILDDIR)/pipe  \
	  $(BUILDDIR)/signal \
	  $(BUILDDIR)/test \
	  $(BUILDDIR)/fpga

SRC_LDLIBS = $(LDLIBS) -pthread -lm -lvillas -lxil
SRC_CFLAGS = $(CFLAGS)
SRC_LDFLAGS = $(LDFLAGS)

src: $(TARGETS)

# Add support for LAPACK / BLAS benchmarks / solvers
ifeq ($(shell pkg-config blas lapack; echo $$?),0)
	PKGS        += blas lapack
	BENCH_OBJS  += fpga-bench-overruns.o
endif

src: $(TARGETS)

# Dependencies for individual binaries
$(BUILDDIR)/fpga:   $(addprefix $(BUILDDIR)/src/,fpga.o fpga-tests.o fpga-bench.o $(BENCH_OBJS))
$(BUILDDIR)/node:   $(BUILDDIR)/src/node.o
$(BUILDDIR)/pipe:   $(BUILDDIR)/src/pipe.o
$(BUILDDIR)/signal: $(BUILDDIR)/src/signal.o
$(BUILDDIR)/test:   $(BUILDDIR)/src/test.o

# Compile executable objects
$(BUILDDIR)/src/%.o: src/%.c | $$(dir $$@)
	$(CC) $(SRC_CFLAGS) -c $< -o $@

# Link target executables
$(TARGETS): | $(LIBS)
	$(CC) $(SRC_LDFLAGS) $^ $(SRC_LDLIBS) -o $@

# Install
install-src:
	install -m 0755 $(BUILDDIR)/node	-T $(PREFIX)/bin/villas-node
	install -m 0755 $(BUILDDIR)/fpga	-T $(PREFIX)/bin/villas-fpga
	install -m 0755 $(BUILDDIR)/signal	-T $(PREFIX)/bin/villas-signal
	install -m 0755 $(BUILDDIR)/pipe	-T $(PREFIX)/bin/villas-pipe
	install -m 0755 $(BUILDDIR)/test	-T $(PREFIX)/bin/villas-test

clean-src:
	rm -rf $(BUILDDIR)/src $(TARGETS)

.PHONY: src src-tests src-tests