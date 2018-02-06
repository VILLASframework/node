# Makefile.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

SONAMES = villas villas-ext

LIBS = $(patsubst %, lib%, $(SONAMES))

LIB_CFLAGS  += $(CFLAGS) -fPIC

ifeq ($(shell uname),Darwin)
	 LIB_CFLAGS += -D_DARWIN_C_SOURCE=1
endif

include $(patsubst %, lib/Makefile.%.inc, $(SONAMES))

$(BUILDDIR)/include/villas/config.h: include/villas/config.h.in Makefile.config | $$(dir $$@)
	cp $< $@
	
	echo -e "\n" >> $@
	echo "#define BUILDID \"$(BUILDID)\"" >> $@
	echo "#define V $(V)" >> $@
	echo "#define PREFIX \"$(PREFIX)\"" >> $@

	echo -e "\n" >> $@
	echo "/* Available Features */" >> $@
	
	if (( $(WITH_WEB) ));                      then echo "#define WITH_WEB    1"            >> $@; fi
	if (( $(WITH_API) ));                      then echo "#define WITH_API    1"            >> $@; fi
	if (( $(WITH_HOOKS) ));                    then echo "#define WITH_HOOKS  1"            >> $@; fi
	if (( $(WITH_IO) ));                       then echo "#define WITH_IO     1"            >> $@; fi
	if (( $(WITH_CONFIG) ));                   then echo "#define WITH_CONFIG 1"            >> $@; fi
	
	for NODE   in $(call escape,$(LIB_NODES));   do echo "#define WITH_NODE_$${NODE} 1"     >> $@; done
	for FORMAT in $(call escape,$(LIB_FORMATS)); do echo "#define WITH_FORMAT_$${FORMAT} 1" >> $@; done
	for PKG    in $(call escape,$(LIB_PKGS));    do echo "#define WITH_$${PKG} 1"           >> $@; done
	
# Compile
$(BUILDDIR)/lib/%.o: lib/%.c | $$(dir $$@)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

lib: $(BUILDDIR)/include/villas/config.h $(patsubst %, lib%, $(SONAMES))

install-lib: $(patsubst %, install-lib%, $(SONAMES))

clean-lib: $(patsubst %, clean-lib%, $(SONAMES))
	rm -rf $(BUILDDIR)/lib
	rm -rf $(BUILDDIR)/include

.PHONY: lib install-lib clean-lib
