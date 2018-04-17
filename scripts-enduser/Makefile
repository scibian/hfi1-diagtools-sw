# Copyright (C) 2014 Intel Corporation, All rights reserved.
# Copyright (C) 2006, 2008 QLogic Corporation, All rights reserved.
# Copyright (c) 2006 PathScale, Inc.  All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# USE OF A COPYRIGHT NOTICE DOES NOT IMPLY PUBLICATION OR DISCLOSURE.
# THIS SOFTWARE CONTAINS CONFIDENTIAL INFORMATION AND TRADE SECRETS OF
# PATHSCALE, INC.  USE, DISCLOSURE, OR REPRODUCTION IS PROHIBITED
# WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF PATHSCALE, INC.
#

TARGETS= hfi1_control

all .DEFAULT: $(TARGETS)

install: all
	install -m 755 $(TARGETS) ${DESTDIR}/usr/bin

clean:
	@echo Nothing to clean
