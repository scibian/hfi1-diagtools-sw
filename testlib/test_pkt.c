/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (C) 2006, 2007, 2008 QLogic Corporation, All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (C) 2006, 2007, 2008 QLogic Corporation, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  - Neither the name of Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * test_pkt.c -- interface to kernel (diag) interface for sending arbitrary
 * packets. Must be root to use.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <opa_user.h>
#include <hfitest.h>

/*
 * Open the diag packet device.
 * Returns fd on success, -errno on failure.
 */
int testpkt_open()
{
    int ret;

    ret = open("/dev/hfi1_diagpkt", O_RDWR);
    if(ret < 0) {
	ret = -errno;
        printf("Failed to open the diags packet device "
               "/dev/hfi1_diagpkt: %s\n", strerror(errno));
    }

    return ret;
}

// Similar to hfi_send_pkt(), but for diags.
// The packet needs to be pre-built, in IB network order, etc.

/*
 * Diagnostic packet interface into the kernel.
 */
#define TEST_PKT_VERS 1 /* error if doesn't match driver value */
struct test_pkt {
	uint16_t version;
	uint16_t unit;
	uint16_t context;
	uint16_t len;
	uint16_t port;
	uint16_t unused;
	uint32_t flags;
	uint64_t data;
	uint64_t pbc;
};

#define F_DIAGPKT_WAIT 0x1     /* wait until packet is sent */


static void dump_pkt(const uint8_t *buf, unsigned cnt, uint64_t pbc)
{
   int idx;
   printf("\npbc: %"PRIx64", cnt: %d (0x%X)\n", pbc, cnt, cnt);
   for (idx = 0; idx < cnt; ++idx) {
	printf("%02X%c", buf[idx], ((idx & 0xF) == 0xF) ? '\n' : ' ');
   }
   if (idx & 0xF)
    printf("\n");
}

int testpkt_send(int fd, unsigned unit, unsigned context, const void *buf,
		 unsigned cnt, uint64_t pbc, int wait)
{
    struct test_pkt dp;
    int ret, serrno;

    if (hfi_debug & __HFI_PKTDBG)
	dump_pkt(buf, cnt, pbc);

    memset(&dp, 0, sizeof(struct test_pkt));
    dp.version = TEST_PKT_VERS;
    dp.unit = unit;
    dp.context = context;
    dp.data = (uint64_t)(ptrdiff_t)buf;
    dp.len = cnt;
    dp.pbc = pbc;
    dp.flags = wait ? F_DIAGPKT_WAIT : 0;
    dp.port = 1;	/* only 1 port */
    ret = write(fd, &dp, sizeof(dp));
    serrno = errno;

    if (ret < 0) {
        _HFI_DBG("Failed to send packet on unit %d: %s\n", unit,
                   strerror(errno));
    } else {
        ret = cnt;
    }
    errno = serrno;
    return ret;
}

/* Use testpkt "tap" to see recent packets delivered to kreceive */
int testpkt_snoop(int fd, void *buf, unsigned cnt)
{
    int ret;

    ret = read(fd, buf, cnt);
    if (ret < 0) {
	perror("testpkt_tap");
    }
    return ret;
}

