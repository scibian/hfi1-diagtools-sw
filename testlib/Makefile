# Copyright (c) 2008 QLogic, Inc. All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# Use of a copyright notice does not imply publication or disclosure.
# This software contains confidential information and trade secrets of
# Qlogic, Inc.  Use, disclosure, or reproduction is prohibited
# without the prior express written permission of Qlogic, Inc.

# Library of functions typically needed by low-level tools

TARGLIB := libipathtest

testlib_build_dir := $(build_dir)/testlib

_ipath_idstr:="$$""Id: ${rpm_extra_description} $$"" $$""Date: $(shell date +%F-%R)"" $$"

CFLAGS += $(BASE_FLAGS)
LDFLAGS += $(BASE_FLAGS)
ASFLAGS += $(BASE_FLAGS)

CFLAGS += ${IPATH_CFLAGS}
CFLAGS += $(LCFLAGS) -O2 -g3 -funwind-tables -Wall -Werror
CFLAGS += -I../include -I$(top_builddir)/include -I/usr/include/uapi -I$(install_top_dir)/usr/include
CFLAGS += -D_IPATH_VERSION='${_ipath_idstr}'
CFLAGS += -I$(PSMDIR)/include/hfi1diag -I$(PSMDIR)/include/hfi1diag/linux-$(targ_arch)

DEPFILES:=$(wildcard $(testlib_build_dir)/*.d)

LDFLAGS += -L$(top_libdir)
LDFLAGS += -L. 
LDFLAGS += -Wl,--export-dynamic
LDLIBS += -linfinipath -Wl,-Bstatic,-lreadline,-Bdynamic -lncurses

${TARGLIB}-objs = pkt_parse.o test_pkt.o

${TARGLIB}-objs-reloc = $(patsubst %.o,$(testlib_build_dir)/%.o,$(${TARGLIB}-objs))

all .DEFAULT : ${testlib_build_dir}/${TARGLIB}.a

${testlib_build_dir}/${TARGLIB}.a : ${${TARGLIB}-objs-reloc}
	ar rcs  $@ ${${TARGLIB}-objs-reloc}

${testlib_build_dir}/%.o: %.c $(testlib_build_dir)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(testlib_build_dir)/%.d
$(testlib_build_dir)/%.d: %.c
	$(CC) $(CFLAGS) -M -MT $(testlib_build_dir)/$*.o -MF $@ $(CPPFLAGS) $<

# Only need dependancy for .o file.  The library will be built always
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif

install:
	@echo "Nothing to do for install."
