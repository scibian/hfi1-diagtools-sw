# Copyright (c) 2006, 2007 QLogic Coporation. All rights reserved.
# Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc.  All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# USE OF A COPYRIGHT NOTICE DOES NOT IMPLY PUBLICATION OR DISCLOSURE.
# THIS SOFTWARE CONTAINS CONFIDENTIAL INFORMATION AND TRADE SECRETS OF
# PATHSCALE, INC.  USE, DISCLOSURE, OR REPRODUCTION IS PROHIBITED
# WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF PATHSCALE, INC.

CFLAGS += $(BASE_FLAGS)
LDFLAGS += $(BASE_FLAGS)
ASFLAGS += $(BASE_FLAGS)

util_build_dir := $(build_dir)/utils

CFLAGS += ${IPATH_CFLAGS}
CFLAGS += -I../lib -I../include
CFLAGS += -I$(top_builddir)/include
# Note: the build will fail if -D_HFI_DEBUGGING is not included in CFLAGS
CFLAGS += -Wall -Werror -O2 -fno-strict-aliasing -g3 -D_HFI_DEBUGGING
CFLAGS += -I$(PSMDIR) -I$(PSMDIR)/include/hfi1diag -I$(PSMDIR)/include/hfi1diag/linux-$(targ_arch) -I$(PSMDIR)/include/hfi1diag/ptl_ips
CFLAGS += -I/usr/include/uapi

LDLIBS +=  -L$(PSMDIR)/lib64
LDLIBS +=  -lpsm2
LDLIBS +=  -lm

DEPFILES:=$(wildcard $(util_build_dir)/*.d)

ifeq (${CCARCH},pathcc)
${util_build_dir}/hfi1_pkt_test.o: CFLAGS += -CG:use_xortozero=1 -Os -OPT:Ofast
else
${util_build_dir}/hfi1_pkt_test.o: CFLAGS += -Os
endif

all: $(util_build_dir)/hfi1_pkt_test $(util_build_dir)/hfi1stats $(util_build_dir)/hfi1_eprom

install: all
	test -z ${DESTDIR}/usr/bin || mkdir -p ${DESTDIR}/usr/bin
	install -m 0755 -D $(util_build_dir)/hfi1_pkt_test ${DESTDIR}/usr/bin/hfi1_pkt_test
	install -m 0755 -D $(util_build_dir)/hfi1_eprom ${DESTDIR}/usr/bin/hfi1_eprom
	install -m 0755 -D $(util_build_dir)/hfi1stats ${DESTDIR}/usr/bin/hfi1stats

$(util_build_dir)/hfi1stats: LDLIBS += -lm
$(util_build_dir)/hfi1stats: $(util_build_dir)/hfi1stats.o
	$(CC) $(LDFLAGS) $@.o -o $@ $(LDLIBS)

$(util_build_dir)/hfi1_eprom: LDLIBS = -lrt

${util_build_dir}/%.o: %.c $(util_build_dir)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(util_build_dir)/%.d
$(util_build_dir)/%.d: %.c
	$(CC) $(CFLAGS) -M -MT $(util_build_dir)/$*.o -MF $@ $(CPPFLAGS) $<

$(util_build_dir)/%.d: %.S
	$(CC) $(CFLAGS) -M -MT $(util_build_dir)/$*.o -MF $@ $(CPPFLAGS) $<

# if we have already built stuff, include the dependency files.
ifneq ($(DEPFILES),)
-include $(DEPFILES)
endif
