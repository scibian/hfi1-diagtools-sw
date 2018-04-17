# Copyright (C) 2006, 2008 QLogic Corporation, All rights reserved.
# Copyright (c) 2006 PathScale, Inc.  All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# USE OF A COPYRIGHT NOTICE DOES NOT IMPLY PUBLICATION OR DISCLOSURE.
# THIS SOFTWARE CONTAINS CONFIDENTIAL INFORMATION AND TRADE SECRETS OF
# PATHSCALE, INC.  USE, DISCLOSURE, OR REPRODUCTION IS PROHIBITED
# WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF PATHSCALE, INC.

MANFMT :=  /usr/bin/groff -mandoc -Tlatin1

TARGETS = hfi1_pkt_test.8.gz
TARGETS += hfi1_eprom.8.gz
TARGETS += hfi1_pkt_send.8.gz
TARGETS += hfi_pkt_files.5.gz
TARGETS += hfi1.4.gz
TARGETS += hfi1stats.8.gz
#TARGETS += ipath_checkout.8.gz
#TARGETS += ipathbug-helper.8.gz
#TARGETS += ipath_diags.8.gz
TARGETS += hfi1_control.8.gz
#TARGETS += ipath_trace.8.gz 
#TARGETS += ipath_pkt_test.8.gz
#TARGETS += ipath_mtrr.8.gz
#TARGETS += ipath_pkt_send.8.gz
#TARGETS += ipath_pkt_files.5.gz
#TARGETS += ipath_exprom.8.gz
#TARGETS += ipath_perf_tuning.8.gz

# somewhat ugly...
.SUFFIXES: .4 .5 .8 8.gz .5.gz 4.gz

all .DEFAULT: $(TARGETS)

install: all
	test -z ${DESTDIR}/usr/share/man/man8 || mkdir -p ${DESTDIR}/usr/share/man/man8
	install -m 0444 *.8.gz ${DESTDIR}/usr/share/man/man8
	test -z ${DESTDIR}/usr/share/man/man5 || mkdir -p ${DESTDIR}/usr/share/man/man5
	install -m 0444 *.5.gz ${DESTDIR}/usr/share/man/man5
	test -z ${DESTDIR}/usr/share/man/man4 || mkdir -p ${DESTDIR}/usr/share/man/man4
	install -m 0444 *.4.gz ${DESTDIR}/usr/share/man/man4

# don't actually format them, just gzip them.  Otherwise SuSE tries to
# format them twice, causing unreadable output.  Most of the other man
# pages aren't installed preformated, so we won't either.  Bug 7407
%.8.gz: %.8
	gzip < $^ > $@

%.5.gz: %.5
	gzip < $^ > $@

%.4.gz: %.4
	gzip < $^ > $@

clean:
	rm -f *.gz
