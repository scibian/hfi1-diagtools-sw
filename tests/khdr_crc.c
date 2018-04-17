/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
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
 * Copyright (c) 2005 PathScale, Inc.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

// This is a quick and dirty program to check crc's from packet
// dumps, to see if they are good or not.

int main(int cnt, char **args)
{
	char buf[128];

	printf("enter space/tab separated hex: lrh.lnh lrh.len ipathhdr pktflags; end with ^D\n");

	while(1) {
		uint8_t lnh;
		uint32_t ihdr;
		uint16_t flags, sum, len;
		char *nxt;

		if(fgets(buf, sizeof buf, stdin) == NULL)
			break;
		if(!(nxt = strtok(buf, " \t"))) {
			printf("invalid input line (%s)\n", buf);
			continue;
		}
		lnh = strtoul(nxt, NULL, 16);

		if(!(nxt = strtok(NULL, " \t"))) {
			printf("invalid input line (%s)\n", buf);
			continue;
		}
		len = strtoul(nxt, NULL, 16);

		if(!(nxt = strtok(NULL, " \t"))) {
			printf("invalid input line (%s)\n", buf);
			continue;
		}
		ihdr = strtoul(nxt, NULL, 16);

		if(!(nxt = strtok(NULL, " \t"))) {
			printf("invalid input line (%s)\n", buf);
			continue;
		}
		flags = strtoul(nxt, NULL, 16);

		sum = lnh + len - ((ihdr>>16)&0xffff) - (ihdr&0xffff) - flags;

		printf("chksum of lnh=%x len=%x ipathhdr %x flags %x: %x\n",
			lnh, len, ihdr, flags, sum);
		if(((lnh + len - (ihdr>>16) - (ihdr&0xffff) - flags) ^
			((((uint32_t)flags)<<16) | sum)) & 0xffff)
		    printf("strange, XORed value is %x\n",
				((lnh + len - (ihdr>>16) - (ihdr&0xffff) - flags) ^
					((((uint32_t)flags)<<16) | sum)) & 0xffff);
	}
	return 0;
}
