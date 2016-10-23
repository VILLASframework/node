# Executables
TARGETS = $(BUILDDIR)/villas-node \
	  $(BUILDDIR)/villas-pipe  \
	  $(BUILDDIR)/villas-signal \
	  $(BUILDDIR)/villas-test \
	  $(BUILDDIR)/villas-hook

SRC_LDLIBS = $(LDLIBS) -pthread -lm -lvillas
SRC_CFLAGS = $(CFLAGS)
SRC_LDFLAGS = $(LDFLAGS) -Wl,-rpath,'$$ORIGIN'

# Enable VILLASfpga support when libxil is available
ifeq ($(shell pkg-config libxil; echo $$?),0)
	LIB_SRCS    += $(addprefix lib/nodes/, fpga.c)
	LIB_SRCS    += $(addprefix lib/kernel/, pci.c vfio.c)
	LIB_SRCS    += $(wildcard  lib/fpga/*.c)
	TARGETS     += $(BUILDDIR)/villas-fpga
	PKGS        += libxil

	# Add support for LAPACK / BLAS benchmarks / solvers
	ifeq ($(shell pkg-config blas lapack; echo $$?),0)
		PKGS        += blas lapack
		BENCH_OBJS  += fpga-bench-overruns.o
	endif
endif

src: $(TARGETS)

$(TARGETS): $(BUILDDIR)/villas-%: $(BUILDDIR)/src/%.o

# Some additional prereqs for individual binaries
$(BUILDDIR)/villas-fpga: $(addprefix $(BUILDDIR)/src/,fpga-tests.o fpga-bench.o $(BENCH_OBJS))


# Compile executable objects
$(BUILDDIR)/src/%.o: src/%.c | $$(dir $$@)
	$(CC) $(SRC_CFLAGS) -c $< -o $@

# Link target executables
$(TARGETS): | $(LIBS)
	$(CC) $(SRC_LDFLAGS) $^ $(SRC_LDLIBS) -o $@

# Install
install-src: src
	install -m 0755 $(TARGETS) $(PREFIX)/bin

clean-src:
	rm -rf $(BUILDDIR)/src $(TARGETS)

.PHONY: src src-tests src-tests