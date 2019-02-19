# Copyright (C) 2006, 2007, 2008 QLogic Corporation, All rights reserved.
# Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc.  All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# USE OF A COPYRIGHT NOTICE DOES NOT IMPLY PUBLICATION OR DISCLOSURE.
# THIS SOFTWARE CONTAINS CONFIDENTIAL INFORMATION AND TRADE SECRETS OF
# PATHSCALE, INC.  USE, DISCLOSURE, OR REPRODUCTION IS PROHIBITED
# WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF PATHSCALE, INC.

_ipath_idstr:="$$""Id: ${rpm_extra_description} $$"" $$""Date: $(shell date +%F-%R)"" $$"
CFLAGS += -D_IPATH_VERSION='${_ipath_idstr}'

test_build_dir := $(build_dir)/tests
testlib_build_dir := $(build_dir)/testlib

TARGETS= $(test_build_dir)/hfi1_pkt_send

CFLAGS += $(BASE_FLAGS)
LDFLAGS += $(BASE_FLAGS)
ASFLAGS += $(BASE_FLAGS)

ifeq (,${BUILDING_32BIT})
# stuff that is only supported in 64 bit mode.
endif

CFLAGS += ${IPATH_CFLAGS}
CFLAGS += $(LCFLAGS) -Wall -Werror -g3 -funwind-tables -O3
# Note: the build will fail if -D_HFI_DEBUGGING is not included in CFLAGS.
CFLAGS += -I$(top_builddir)/include -I../include -D_HFI_DEBUGGING
CFLAGS += -I$(PSMDIR)/include/hfi1diag -I$(PSMDIR)/include/hfi1diag/linux-$(targ_arch) -I$(PSMDIR)/include/hfi1diag/ptl_ips
CFLAGS += -I/usr/include/uapi

LDFLAGS += -Wl,--export-dynamic

DEPFILES=$(wildcard $(test_build_dir)/*.d)

PKTFILES=$(shell find ../tests/test_packets -maxdepth 1 -type f)

all: $(TARGETS)

clean:
	rm -rf $(TARGETS)
	rm -rf $(DEPFILES)
	rm -rf $(DEPFILES:.d=.o)

install: all
	test -z ${DESTDIR}/usr/bin || mkdir -p ${DESTDIR}/usr/bin
	install -m 0755 -D $(test_build_dir)/hfi1_pkt_send ${DESTDIR}/usr/bin/hfi1_pkt_send
	test -z ${DESTDIR}/usr/share/hfi-diagtools-sw || mkdir -p ${DESTDIR}/usr/share/hfi-diagtools-sw/test_packets
	install -D $(PKTFILES) ${DESTDIR}/usr/share/hfi-diagtools-sw/test_packets

$(test_build_dir)/hfi1_pkt_send: $(test_build_dir)/hfi1_pkt_send.o \
  $(testlib_build_dir)/libipathtest.a


$(TARGETS): LDFLAGS += -L$(PSMDIR)/lib64
$(TARGETS): LDFLAGS += -lpsm2

$(TARGETS):
# LDFLAGS contains libraries that should be linked after prerequisites.
# RHEL gcc does recursive linking and does not care about library order.
# Ubuntu gcc however does care.
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

${test_build_dir}/efuse_7322.o: efuse_7322.c $(test_build_dir)/efuse_7322.d
	$(CC) $(CFLAGS) -DEFUSE_WRITE=1 -c $< -o $@

${test_build_dir}/%.o: %.c $(test_build_dir)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(test_build_dir)/%.d
$(test_build_dir)/%.d: %.c
	$(CC) $(CFLAGS) -M -MT $(test_build_dir)/$*.o -MF $@ $(CPPFLAGS) $<

# if we have already built stuff, include the dependency files.
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif
