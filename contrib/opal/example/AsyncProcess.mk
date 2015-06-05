# ----------------------------------------------------------------------------#
# Specify program name
PROGRAM = AsyncProcess

TARGET_RTLAB_ROOT ?= /usr/opalrt
RTLAB_ROOT = $(TARGET_RTLAB_ROOT)

# ----------------------------------------------------------------------------#
# QNX v6.x and Linux specifics
#

# Support for QNX 6.3
ifeq "$(SYSNAME)" "nto"

	CC = gcc
	# Support for QNX 6.3.0
	OS_VERSION = $(shell uname -r)
	ifeq "$(OS_VERSION)" "6.3.0"
	export QNX_HOST=/usr/qnx630/host/qnx6/x86
	export QNX_TARGET=/usr/qnx630/target/qnx6/x86
	endif
  
	LIB_TARGET = -lsocket -lm

else ## linux

	# Intel is the default compiler
	RTLAB_INTEL_COMPILER ?= 1
	ifeq ($(RTLAB_INTEL_COMPILER),1)
		CC = opicc
	else
		CC = gcc
	endif
	LIB_TARGET = -lpthread -lrt -lm
	# Add Intel C library for compilation with Gcc
	ifeq ($(RTLAB_INTEL_COMPILER),0)
		LIB_TARGET += -lirc
	else
		LD_OPTS += -diag-disable remark,warn
	endif
endif
# ----------------------------------------------------------------------------#

LD := $(CC)

ifeq ($(DEBUG),1)
	CC_OPTS = -g -D_DEBUG
	LD_OPTS = -g
else
	CC_OPTS = -O
endif

INCPATH = -I. -I$(RTLAB_ROOT)/common/include_target
LIBPATH  = -L.

#The required libraries are transfered automatically in the model directory before compilation
LIBS := -lOpalAsyncApiCore -lOpalCore -lOpalUtils $(LIB_TARGET)

CFLAGS	= $(CC_OPTS) $(INCPATH)
LDFLAGS = $(LD_OPTS) $(LIBPATH) 

MAKEFILE = $(PROGRAM).mk
OBJS = ${PROGRAM}.o

all: $(PROGRAM)

install:
	\mkdir -p $(RTLAB_ROOT)/local
	\chmod 755 $(RTLAB_ROOT/local
	\cp -f $(PROGRAM) $(RTLAB_ROOT)/local

clean:
	\rm -f $(OBJS) $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
