/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (c) 2007, 2008, 2009 Qlogic Corporations.  All rights reserved.
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
 * Copyright (c) 2007, 2008, 2009 Qlogic Corporations.  All rights reserved.`
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

/* hfi1_pkt_send - Send packets with arbitrary content. */

/*
 * included header files
 */
#include "netinet/in.h"
#include "sys/time.h"
#include "fcntl.h"
#include "stdio.h"
#include "unistd.h"
#include <signal.h>
#include "assert.h"
#include <sched.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>		// for copy timing test
#include <ctype.h>

#include "opa_user.h"
#include "opa_service.h"
#include "ipserror.h"

#include <linux/stddef.h>

#include "hfitest.h"

/*
 * CONST
 */
#define ERR_TID HFI_RHF_TIDERR
#define LAST_RHF_SEQNO 13

//Request a send credit update from the hardware when fewer than
// this many credits are available.
#define SEND_CREDIT_UPDATE_THRESH      128
#define SEND_CREDIT_UPDATE_RETRIES     10000

#ifndef HFI_RCVEGR_ENTRIES_DFLT
#define HFI_RCVEGR_ENTRIES_DFLT 1024
#endif

#define OK 0


/*
 * IB - LRH header consts
 */
#define HFI_LRH_GRH 0x0003	/* 1. word of IB LRH - next header: GRH */
#define HFI_LRH_BTH 0x0002	/* 1. word of IB LRH - next header: BTH */
#define HFI_LRH_IP6 0x0001    /* 1. word of non-IB IPV6 - next header IPV6 */
#define HFI_LRH_RAW 0x0000	/* 1. word of raw-LRH - "no" next header */
#define HFI_LRH_LNH_MASK 3

#define LRH_LEN 8
#define GRH_LEN 40
#define BTH_LEN 12
#define BTH_OP_UD_SEND 0x64	/* Opcode in BTH for UD Send, requires DETH */
#define BTH_OP_UD_SEND_IMM 0x65	/* Opcode in BTH for UD Send, requires DETH */
#define DETH_LEN 8
#define KDETH_LEN 8
#define TEST_LEN 36
#define MAD_LEN 24 /* length of a base MAD packet, in bytes */
#define BYPASS8_LEN 8
#define BYPASS10_LEN 10
#define BYPASS16_LEN 16

/*
 * BTH header consts
 */
#define HFI_BTH_OPCODE 0xC0    /* First manufacturer-reserved opcode */

/*
 * misc.
 */
#define HFI_PROTO_VERSION 0x1 //WFR Kver KDETH field value

#define HFI_EAGER_TID_ID HFI_KHDR_TID_MASK

#define min(a,b) ((a) < (b) ? (a) : (b))


struct credit_return {
	uint16_t	Counter:11;
	uint16_t	Status:1;
	uint16_t	CreditReturnDueToPbc:1;
	uint16_t	CreditReturnDueToThreshold:1;
	uint16_t	CreditReturnDueToErr:1;
	uint16_t	CreditReturnDueToForce:1;
	uint16_t	pad0;
	uint32_t	pad1;
};


struct rh_flags {
    uint64_t PktLen:12;
    uint64_t RcvType:3;
    uint64_t UseEgrBfr:1;
    uint64_t EgrIndex:11;
    uint64_t DcInfo:1;
    uint64_t RcvSeq:4;
    uint64_t EgrOffset:12;
    uint64_t HdrqOffset:9;
    uint64_t Err:11;
};


struct _stack_ctrl {
	int file_desc;

	uint16_t j_key;
	uint32_t qp;                    /* QP value indicating KDETH hdr */
	uint32_t runtime_flags;

        /* Receive side */
	uint32_t* header_queue_base;
        volatile uint64_t* header_queue_head; //Pointer to RcvHdrHead register
                                              // Value is in units of dwords
        uint64_t* header_queue_tail;          //Pointer to RcvHdrTail register
                                              // Value is in units of dwords

	uint32_t header_queue_rhf_offset;
        uint32_t header_queue_seqnum;    //Next expected sequence num (1-13)
        uint32_t header_queue_index;     //HQ slot for next expected arrival
	uint32_t header_queue_size;
	uint32_t header_queue_elem_size;

	void *eager_queue_base;
        volatile uint64_t* eager_queue_head; //Pointer to RcvEgrIndexHead reg

	int eager_queue_elem_size;

        /* Send side */
        volatile uint64_t *spio_buffer_base;
        volatile uint64_t *spio_buffer_base_sop;
        volatile uint64_t *spio_buffer_end;

        int spio_total_send_credits;
        int spio_avail_send_credits;
        int spio_fill_send_credits;
        int spio_cur_slot; // Index into send PIO ring buffer to write next
};


/* Bundle the various bits of "boilerplate"
 * associated with a set of packets into
 * a struct, instead of scattering them in
 * multiple variables. This allows us to init
 * en-masse, and to use a single function
 * to prepare a header, without needing a huge
 * number of parameters.
 */

typedef struct _pkt_bp {
	uint32_t qp;
	uint32_t mlid;
	uint32_t rlid;
	uint16_t my_ctxt; 
        uint16_t remote_ctxt;
	uint8_t vl;
	uint8_t sl;
} pkt_bp;

static pkt_bp default_pkt_bp = {
	0xFFFFFFFF,		/* QP is only 24-bits so this means "not set" */
	0, 0,			/* My LID, Remote LID, fill in later */
	0, 0,			/* My/remote ctxt, fill in later */
	0, 0			/* Default VL and SL of 0 */
};

/* To work with the existing raw_correctness.c structure,
 * we will read an entire file of packets into memory at the
 * start, and access them sequentially. First cut will malloc
 * each individually, to ensure alignment without MachDep code
 * and for simplicity.
 * pbc_wd was added to support "pre-rendering" packets from the file.
 * if we need "test mode", we not only set it in the pbc_wd, but
 * we compute the needed CRCs and append them to the buffer, simplifying
 * the PIO-write process.
 */
struct fpkt {
	struct fpkt *next;
	uint8_t *bufp;   //This contains all packet data to write.
        union {
            uint64_t pbc_wd;
            struct hfi_pbc pbc;
        };
	int plen;        //Length in bytes to send/write
};


/*
 * global VARS
 */

static struct _hfi_ctrl *hfi_ctrl = NULL;

static struct _stack_ctrl stack_ctrl;
static uint16_t ctxt_p_key = HFI_DEFAULT_P_KEY;


/*
 * Structure for saving deferred trigger words, for when user want a set
 * of packets to be all-but-sent, and then all trigger words written as
 * fast as possible.
 */
static struct def_trig {
	volatile uint64_t *addr; //Address to store qword for triggering launch
	uint64_t value;          //Value to store at that address
} *def_trig_p;


/*
 * Below is set to -1 by -D switch, to indicate user desire for
 * deferred trigger, otherwise works as normal "next index to use"
 */
static int def_trig_idx = 0;



void hfi_pkt_signal(int param)
{
	(void) param;
	if (stack_ctrl.file_desc) {
		close(stack_ctrl.file_desc);
	}
	exit(1);
}


/* 
 * Update available send PIO credits.
 */

static int update_send_credits(void)
{
    volatile struct credit_return* free_credits = 
        (volatile struct credit_return*)
        hfi_ctrl->base_info.sc_credits_addr;

    if(free_credits->Status != 0) {
        _HFI_ERROR("Credit return (%d available) indicated errors: "
                "Status %d DueToPBC %d DueToThreshold %d DueToErr %d "
                "DueToForce %d\n",
                stack_ctrl.spio_avail_send_credits, free_credits->Status,
                free_credits->CreditReturnDueToPbc,
                free_credits->CreditReturnDueToThreshold,
                free_credits->CreditReturnDueToErr,
                free_credits->CreditReturnDueToForce);
        exit(-1);
        return -1;
    }

    //Can this be optimized down to two cycles?
    //Reversing avail_send_credits might help performance.
    // Count only used credits here, then compare to total credits.
    stack_ctrl.spio_avail_send_credits = stack_ctrl.spio_total_send_credits -
        ((stack_ctrl.spio_fill_send_credits - free_credits->Counter) & 0x7FF);

    return 0;
}


/*
 * Write pre-formatted packet data to the provided PIO send buffer.
 */

static void write_pio_data(volatile uint64_t* pio_dest,
                           uint64_t* src_data, int length)
{
    int i;
    int remaining_length = length;

    /* This loop only copies whole 64-byte blocks. */
    for(; remaining_length >= 64; remaining_length -= 64) {
        assert(((uintptr_t)pio_dest & (uintptr_t)0x3F) == 0);

        /* Copy 8 quadwords (64 bytes) of payload at a time. */
        for(i = 0; i < 8; i++) {
            *pio_dest++ = *src_data++;
        }

        if(pio_dest == stack_ctrl.spio_buffer_end) {
            pio_dest = stack_ctrl.spio_buffer_base;
        }
    }

    /* Handle the last credit block: <64 bytes to copy. */
    /* Copy as many whole quadwords as possible */
    while(remaining_length >= 8) {
        *pio_dest++ = *src_data++;
        remaining_length -= 8;
    }

    assert(remaining_length == 4 || remaining_length == 0);

    /* If a dword remains, pack it into a quadword and copy. */
    if(remaining_length == 4) {
        uint64_t qword = *(uint32_t*)src_data;
        *pio_dest++ = qword;
    }

    /* Finally, write 0 to remaining quadwords. */
    /* Write 0-value qwords until addr is aligned to a 64-byte multiple. */
    while(((uintptr_t)pio_dest & (uintptr_t)0x3F) != 0) {
        *pio_dest++ = (uint64_t)0;
    }
}


/* pre-fab-packet equivalent of previous _transfer_frame.
 */

int
send_prefab_frame(struct fpkt *wp)
{
    struct timeval tv;
    uint64_t usecs_start=0ULL, usecs_end;
    int length = wp->plen;
    int num_credits = ((length + 8) / 64) + ((length + 8) % 64 ? 1 : 0);
    int credits_needed = num_credits > SEND_CREDIT_UPDATE_THRESH ? 
        num_credits : SEND_CREDIT_UPDATE_THRESH;

    /* length has to be a multiple of a DW (4 bytes) */
    assert(length % 4 == 0);
    assert((num_credits) * 64 >= length + 8);

    int i;

    update_send_credits();

    for(i = 0; i < SEND_CREDIT_UPDATE_RETRIES &&
            stack_ctrl.spio_avail_send_credits < credits_needed; i++) {
        usleep(1);
        update_send_credits();
    }

    if(i == SEND_CREDIT_UPDATE_RETRIES) {
        _HFI_ERROR("no credits available for PIO send: cur_slot %d available %d need %d\n", 
                stack_ctrl.spio_cur_slot, 
                stack_ctrl.spio_avail_send_credits,
                credits_needed);
        return IPS_RC_NO_RESOURCE_AVAILABLE;
    }

    stack_ctrl.spio_fill_send_credits += num_credits;
    stack_ctrl.spio_avail_send_credits -= num_credits;

    int cur_slot = stack_ctrl.spio_cur_slot;
    volatile uint64_t* addr_sop = (volatile uint64_t*)
        ((uintptr_t)stack_ctrl.spio_buffer_base_sop + 
         (uint64_t)(cur_slot * 64));

    gettimeofday(&tv, NULL);
    usecs_start = tv.tv_sec * 1000000ULL + tv.tv_usec;

    /* Write the PBC: it is stored separately from the rest of the packet. */
    if(def_trig_p) {
        /* Deferred trigger: To defer the packet launch, skip writing the PCB.
           Queue it in the deferred trigger list instead. */
        def_trig_p[def_trig_idx].addr = addr_sop;
        def_trig_p[def_trig_idx].value = wp->pbc_wd;
        def_trig_p[++def_trig_idx].addr = NULL; /* Insurance */

        addr_sop += 1;
    } else {
        *addr_sop++ = wp->pbc_wd;
    }

    //Write the remaining part of the first SOP credit block.
    write_pio_data(addr_sop, (uint64_t*)wp->bufp, min(length, 56));

    //Write the remaining data in regular-address credit blocks.
    if(length > 56) {
        volatile uint64_t* addr = stack_ctrl.spio_buffer_base;
        if(cur_slot + 1 != stack_ctrl.spio_total_send_credits) {
            addr = (volatile uint64_t*)((uintptr_t)addr + 
                    ((cur_slot + 1) * 64));
        }

        write_pio_data(addr, (uint64_t*)(wp->bufp + 56), length - 56);
    }

    gettimeofday(&tv, NULL);
    usecs_end = tv.tv_sec * 1000000ULL + tv.tv_usec;

    _HFI_VDBG("Time to send %lu bytes %lu usecs (%4.2f MB/s)\n",
            (sizeof(uint64_t) + length), (usecs_end - usecs_start),
            ((double) (sizeof(uint64_t) + length) /
             (double) (usecs_end - usecs_start)));

    stack_ctrl.spio_cur_slot =
        (cur_slot + num_credits) % stack_ctrl.spio_total_send_credits;

    return IPS_RC_OK;
}


static int do_def_trigs(void)
{
	int idx = 0;
	struct def_trig *dtp;
	volatile uint64_t *addr;

	dtp = def_trig_p;
	if (dtp)
		for (; idx < def_trig_idx; ++idx) {
                        addr = dtp[idx].addr;
			if (addr != NULL) {
				*addr = dtp[idx].value;
			}
		}

	def_trig_idx = 0;
	return idx;
}


static int hfi_pkt_open(int unit, uint32_t * my_lid, uint16_t * my_ctxt)
{
	struct hfi1_user_info_dep hfi_drv_user_info;
        struct hfi1_base_info* hfi_drv_base_info;
        struct hfi1_ctxt_info* hfi_drv_ctxt_info;
	int mylid;
	struct hfi1_cmd cmd;
	uid_t *uid = (uid_t *)&hfi_drv_user_info.uuid;

        stack_ctrl.file_desc = hfi_context_open(unit, 0, 5000);
        if(stack_ctrl.file_desc == -1) {
		_HFI_ERROR("hfi_context_open failed. Device = %d, Error = %s\n",
                        unit, strerror(errno));
		return IPS_RC_UNKNOWN_DEVICE;
	}

	memset(&hfi_drv_user_info, 0, sizeof(struct hfi1_user_info_dep));
	*uid = getuid();

	hfi_drv_user_info.userversion = HFI1_USER_SWMINOR|(HFI1_USER_SWMAJOR<<16);

        hfi_drv_user_info.hfi1_alg = HFI1_ALG_ACROSS; /*HFI_ALG_WITHIN*/

        hfi_drv_user_info.subctxt_id = 0;
        hfi_drv_user_info.subctxt_cnt = 0;

	hfi_ctrl = hfi_userinit(stack_ctrl.file_desc, &hfi_drv_user_info);
	if (hfi_ctrl == NULL)
		return IPS_RC_DEVICE_INIT_FAILED;
       
	/* Setting P_KEY to default value */
	cmd.type = PSMI_HFI_CMD_SET_PKEY;
	cmd.len = 0;
	cmd.addr = (uint64_t)ctxt_p_key;

	if (hfi_cmd_write(hfi_ctrl->fd, &cmd, sizeof(cmd)) == -1) {
		if (errno != EINVAL)
			_HFI_ERROR("Failed to set context P_KEY %u\n", ctxt_p_key);
	}

	/*
	 * extract info from DrvBaseInfo 
	 */

        hfi_drv_base_info = &hfi_ctrl->base_info;
        hfi_drv_ctxt_info = &hfi_ctrl->ctxt_info;

	stack_ctrl.qp = hfi_drv_base_info->bthqp;
	stack_ctrl.j_key = hfi_drv_base_info->jkey;
	stack_ctrl.runtime_flags = hfi_drv_ctxt_info->runtime_flags;

	stack_ctrl.header_queue_base =
	    (uint32_t *) (ptrdiff_t) hfi_drv_base_info->rcvhdr_bufbase;

        /* We want a pointer to the offset, since it is changing and we want
           to read it regularly. */
        stack_ctrl.header_queue_head = 
            &(((uint64_t*)hfi_drv_base_info->user_regbase)[ur_rcvhdrhead]);
        stack_ctrl.header_queue_tail = 
            &(((uint64_t*)hfi_drv_base_info->user_regbase)[ur_rcvhdrtail]);

        /* WFR HAS 8.5.3 Receive Header Queue Management states that
           RcvHdrHead is set to 0 when the context is enabled. */
        assert((*stack_ctrl.header_queue_head & 0x1FFFFFF) == 0);

        stack_ctrl.header_queue_rhf_offset =
            hfi_drv_ctxt_info->rcvhdrq_entsize - 8;

        stack_ctrl.header_queue_seqnum = 1; //Ranges from 1-13: start at 1
        stack_ctrl.header_queue_index = 0;
        stack_ctrl.header_queue_size = hfi_drv_ctxt_info->rcvhdrq_cnt;
        stack_ctrl.header_queue_elem_size = hfi_drv_ctxt_info->rcvhdrq_entsize;

	stack_ctrl.eager_queue_base =
	    (void *)(ptrdiff_t) hfi_drv_base_info->rcvegr_bufbase;
        stack_ctrl.eager_queue_head = 
            &(((uint64_t*)hfi_drv_base_info->user_regbase)[ur_rcvegrindexhead]);

	stack_ctrl.eager_queue_elem_size =
            hfi_drv_ctxt_info->rcvegr_size; /* in bytes */


        stack_ctrl.spio_total_send_credits = hfi_drv_ctxt_info->credits;
        stack_ctrl.spio_fill_send_credits = 0;
        stack_ctrl.spio_avail_send_credits = hfi_drv_ctxt_info->credits;
        stack_ctrl.spio_cur_slot = 0;

	stack_ctrl.spio_buffer_base =
	    (uint64_t*)(uintptr_t)hfi_drv_base_info->pio_bufbase;
	stack_ctrl.spio_buffer_base_sop =
	    (uint64_t*)(uintptr_t)hfi_drv_base_info->pio_bufbase_sop;
        stack_ctrl.spio_buffer_end =
            (uint64_t*)((uintptr_t)stack_ctrl.spio_buffer_base +
                    (stack_ctrl.spio_total_send_credits * 64));

        update_send_credits();


        struct hfi1_status* status = (struct hfi1_status*)hfi_drv_base_info->status_bufbase;
	_HFI_DBG("Initial Chip and Link status: "
                "devstatus=0x%llx pstatus=0x%llx\n",
                status->dev, status->port);

	if ((mylid = hfi_get_port_lid(hfi_drv_ctxt_info->unit, 1)) == -1)
		_HFI_INFO("Failed to get our IB LID");

	*my_lid = mylid;
	*my_ctxt = hfi_drv_ctxt_info->ctxt;

        //hfi_check_unit_status not currently implemented in PSM
	//(void)hfi_check_unit_status(hfi_ctrl);	// warn up front if problems
	return IPS_RC_OK;
}




static struct fpkt *file_pkt_hd, *file_pkt_last;

static int curr_key;
static struct fpkt *curr_pkt;

static struct fpkt *get_pkt_desc(int key)
{
	int skip_cnt;
	struct fpkt *wp;
	if (key < curr_key || !curr_pkt) {
		/* First call, or unlikely event where we move back,
		 * just "rewind" and re-seek.
		 */
		curr_key = 0;
		curr_pkt = file_pkt_hd;
	}
	skip_cnt = key - curr_key;
	wp = curr_pkt;
	while (skip_cnt--) {
		wp = wp->next;
		if (!wp) {
			/* Ran out. May wrap in future. For now,
			 * return failure.
			 */
			break;
		}
	}
	if (wp) {
		curr_pkt = wp;
		curr_key = key;
	}
	return wp;
}

#define BUF_CHUNK 256
static unsigned char *bufp;
static int bufsize;
static int lineno, charno;
static int lid_warn;

/* Do one "field patch" for a header. That is, given a string of the form:
 * " fldname = val" with all whitespace optional, and fldname a valid name,
 * patch the appropriate field and return a pointer to the first character
 * not part of val.
 */
#define MAXFLDLEN 42

int patch_hdr_fld(uint8_t *bufp, char *olp, const char *hname)
{
	char fldname[MAXFLDLEN+1];
	char *lp, *rp;
	uint64_t val;
	int idx, rval;

	/* edits to header are enclosed in parens */
	lp = olp;
	while (isspace(*lp)) ++lp;
	for (idx = 0; idx < MAXFLDLEN ; ++idx) {
		if (!isalnum(lp[idx]) && lp[idx] != '_') break;
		fldname[idx] = lp[idx];
	}
	if (!idx)
		return -2;
	fldname[idx] = '\0';
	lp += idx;
	while (isspace(*lp)) ++lp;
	if (*lp != '=')
		return -2;
	val = strtoull(++lp, &rp, 0);
	if (lp == rp)
		return -1;
	rval = testpkt_put_hdr_fld(bufp, hname, fldname, val);
	if (rval < 0) {
		fprintf(stderr,"No field %s:%s\n",hname, fldname);
                return -1;
	}
	if (rval == 0) {
		/* Success, return number of input chars consumed */
		_HFI_VDBG("%s:%s=%"PRIX64"\n", hname, fldname, val);
		rval = rp - olp;
	}
	return rval;
}

int patch_hdr(uint8_t *bufp, char *olp, const char *hname)
{
	char *lp = olp;
	int rval;
	/* edits to header are enclosed in parens */
	if (*lp != '(')
		return 0;
	++lp;
	/* MEA: Warning: below may alter some fields of header before
	 * bailing. Need better error handling.
	 */
	do {
		rval = patch_hdr_fld(bufp, lp, hname);
		if (rval < 0)
			break;
		lp += rval;
		while (isspace(*lp)) ++lp;
		if (*lp == ')') {
			rval = lp + 1 - olp;
			break;
		}
		if (*lp == ',') ++lp;
	} while (*lp);
	return rval;
}

static void show_line(FILE *fp, int lineno, const char *label, const char *line, const char *curs)
{
	fprintf(fp,"Line %d, %s:\n",lineno, label);
	fprintf(fp,"%s",line);
	while (line < curs) {
		if (*line == '\t') fputc('\t',fp);
		else fputc(' ',fp);
		++line;
	}
	fputs("^\n",fp);
}

/* Reading input by lines makes "macros" easier.
 * initial limitation: 512-byte line max.
 */
#define INPUT_CHUNK 512

/* Possible flags for special packet handling (all require Test bit in PBC) */
#define TBIT_EBP 1
#define TBIT_ICRC 2
#define TBIT_HCRC 4
#define TBIT_BYPASS 8
#define TBIT_BYPASSICRC 0x10
#define TBIT_INTR 0x20
#define TBIT_DCINFO 0x40
#define TBIT_FECN 0x80
#define TBIT_NOPAD 0x100

/* Pad the global bufp packet buffer to a multiple of a specified length with
 * zeros.  If reallocation is necessary, the bufp and bufsize global variables
 * are updated.  The return value is the number of bytes added for padding, or
 * -1 if an error occurred.
 */
static int pad_buffer(int cur_len, int pad_multiple)
{
	int pad_remainder = cur_len % pad_multiple;
	int pad_len = pad_multiple - pad_remainder;

	if (pad_remainder == 0)
		return 0;

	if (cur_len + pad_len >= bufsize) {
		bufsize += BUF_CHUNK;
		bufp = realloc(bufp, bufsize);
		if (!bufp)
			return -1;
	}

	memset(bufp + cur_len, 0, pad_len);
	return pad_len;
}

static int get_pkt(FILE * ifp)
{
    int dig, dcnt;
    int bidx;
    int make_lrh;
    int make_bth;
    int make_bypass;
    int need_Tbit;	/* 0 for none, or one of TBIT_* */
    char *pbc_patch_str = NULL; /* PBC macro patch string, if found. */

    enum sst {
        SST_START,	/* at start or after '\n' */
        SST_COMMENT,	/* Have seen '#' */
    } state;

    pkt_bp *bp = &default_pkt_bp;

    static char *linebuf;
    static int lblen;
    char *lp = NULL;

    if (!linebuf) {
        linebuf = malloc(INPUT_CHUNK);
        if (!linebuf)
            return -ENOMEM;
        lblen = INPUT_CHUNK;
    }

    if (!bufp) {
        bufp = malloc(BUF_CHUNK);
        if (!bufp)
            return -ENOMEM;
        bufsize = BUF_CHUNK;
    }
    if (!ifp) {
        return -1;
    }

    dig = 0;
    bidx = 0;
    dcnt = 0;
    make_lrh = -1;	/* default none. Becomes LNH field if set */
    make_bth = 0;
    make_bypass = -1;
    need_Tbit = 0;
    state = SST_START;

    /* Line-at-a-time version, for easier support of "macros" */
    while (1) {
        if (!lp) {
            if (feof(ifp))
                break;
            lp = fgets(linebuf, lblen, ifp);
            if (!lp)
                break;
            /* MEA: Should auto-grow linebuf here, but not today */
            ++lineno;
            charno = 0;
        }
        ++charno;
        if (*lp == '.' && state == SST_START) {
            /*
             * End of packet. Suffix options allow for:
             * B Set EBP instead of EGP
             * I Insert an invalid ICRC (hardware chooses value)
             * H Disable hardware HCRC insertion
             * Y Enable PbcPacketBypass
             * C Enable PbcInsertBypassIcrc
             * N Enable PbcIntr
             * D Enable PbcDcInfo
             * F Enable PbcFecn
	     * P Disable padding bypass packets to quad word
             */
            lp += 1;
            while (strchr("BIHYCNDFP", toupper(*lp))) {
                switch (toupper(*lp)) {
                    case 'B' :
                        need_Tbit |= TBIT_EBP;
                        break;
                    case 'I' :
                        need_Tbit |= TBIT_ICRC;
                        break;
                    case 'H':
                        need_Tbit |= TBIT_HCRC;
                        break;
                    case 'Y':
                        need_Tbit |= TBIT_BYPASS;
                        break;
                    case 'C':
                        need_Tbit |= TBIT_BYPASSICRC;
                        break;
                    case 'N':
                        need_Tbit |= TBIT_INTR;
                        break;
                    case 'D':
                        need_Tbit |= TBIT_DCINFO;
                        break;
                    case 'F':
                        need_Tbit |= TBIT_FECN;
                        break;
		    case 'P':
			need_Tbit |= TBIT_NOPAD;
			break;
                }
                lp += 1;
            }
            break;
        }

        if (dcnt == 0)
        {
	    if(!strncmp(lp, "PBC", 3)) {
		int pbc_str_len;
		char* paren_match;

		lp += 3;

		/* Rather than patching a PBC together here, store the macro
		 * argument string.  The string will be used for patching after
		 * the PBC has been fully computed, allowing the packet input
		 * to override all computed values.
		 *
		 * Problem is, lp needs to be advanced here based on how many
		 * characters patch_hdr() consumes.  Search for the closing
		 * parenthesis to compute the length.
		 */
		paren_match = strchr(lp, ')');
		if(paren_match == NULL) {
		    return -1;
		}

		pbc_str_len = (int)(paren_match - lp) + 1;
		pbc_patch_str = strndup(lp, pbc_str_len);
		lp += pbc_str_len;
		continue;
	    }
	    else if(bidx == 0 && !strncmp(lp, "LRH", 3)) {
                        int patch_ret;
                        uint8_t lrh_buf[LRH_LEN];

                        make_lrh = HFI_LRH_BTH; /* assumed */

                        lp += 3;

                        if (*lp == 'R') {
                            make_lrh = HFI_LRH_RAW;
                            ++lp;
                        }
                        /*
                         * Fill in most of LRH. The length will be dealt
                         * with at end.
                         */
                        memset(bufp, 0, LRH_LEN);
                        testpkt_put_hdr_fld(bufp, "LRH", "VL", bp->vl & 0xF);
                        testpkt_put_hdr_fld(bufp, "LRH", "LVer", 0);
                        testpkt_put_hdr_fld(bufp, "LRH", "SL", bp->sl);
                        testpkt_put_hdr_fld(bufp, "LRH", "LNH", make_lrh);
                        if (!bp->rlid && !lid_warn) {
                            fprintf(stderr,"Rmt LID not spec'd. use -L\n");
                            ++lid_warn;
                        }
                        testpkt_put_hdr_fld(bufp, "LRH", "DLID",bp->rlid);
                        testpkt_put_hdr_fld(bufp, "LRH", "SLID",bp->mlid);
                        /* Having filled in the "boilerplate" version, look
                         * for "patches".
                         */
                        memcpy(lrh_buf, bufp, LRH_LEN);
                        patch_ret = patch_hdr(lrh_buf, lp, "LRH");
                        if(patch_ret < 0) {
                            return patch_ret;
                        } else if (patch_ret > 0) {
                            memcpy(bufp, lrh_buf, LRH_LEN);
                            lp += patch_ret;
                        } else { /* patch_ret == 0 */
                            lp = NULL;
                        }

                        bidx = LRH_LEN;
                        continue;
                    }
                else if(bidx == LRH_LEN && !strncmp(lp, "GRH", 3)) {
                        int patch_ret;

                        lp += 3;

                        /* No boilerplate for GRH yet? */
                        memset(bufp + bidx, 0, GRH_LEN);

                        patch_ret = patch_hdr(bufp + bidx, lp, "GRH");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        bidx += GRH_LEN;
                        continue;
                    }
                else if(!strncmp(lp, "MAD", 3)) {
                        int patch_ret;

                        lp += 3;

                        memset(bufp + bidx, 0, MAD_LEN);
                        testpkt_put_hdr_fld(bufp + bidx, "MAD", "BaseVersion", 1);
                        testpkt_put_hdr_fld(bufp + bidx, "MAD", "ClassVersion", 1);

                        patch_ret = patch_hdr(bufp + bidx, lp, "MAD");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        bidx += MAD_LEN;
                        continue;
                    }
                else if(!strncmp(lp, "BTH", 3)) {
                        uint64_t lnh_val;
                        int patch_ret;

                        lp += 3;

                        /* MEA: Should below sanity checks be warnings? */
                        lnh_val = testpkt_get_hdr_fld(bufp, "LRH", "LNH");
                        if (lnh_val == HFI_LRH_BTH && bidx != LRH_LEN) {
                            fprintf(stderr,"LNH is BTH, but BTH macros at offset %d\n",bidx);
                            goto failed_macro;
                        }
                        if (lnh_val == HFI_LRH_GRH && bidx != (LRH_LEN + GRH_LEN)) {
                            fprintf(stderr,"LNH is GRH, but BTH macros at offset %d\n",bidx);
                            goto failed_macro;
                        }
                        memset(bufp + bidx, 0, BTH_LEN);
                        testpkt_put_hdr_fld(bufp + bidx, "BTH", "OpCode", 
                                BTH_OP_UD_SEND);

                        int qp = bp->qp;
                        if(qp == 0xFFFFFFFF) {
                            /* No QP specified: construct a PSM-like QP
                               using the driver KDETH QP value and context. */
                            qp = (stack_ctrl.qp << 16) | bp->remote_ctxt;
                        }

                        testpkt_put_hdr_fld(bufp + bidx, "BTH", "DestQP", qp);
                        testpkt_put_hdr_fld(bufp + bidx, "BTH", "P_Key", 
                                HFI_DEFAULT_P_KEY);

                        patch_ret = patch_hdr(bufp + bidx, lp, "BTH");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        make_bth = testpkt_get_hdr_fld(bufp + bidx, "BTH", "PadCnt") == 0;

                        bidx += BTH_LEN;
                        continue;
                }
                else if(!strncmp(lp, "DETH", 4)) {
                        uint64_t lnh_val;
                        int patch_ret;

                        lp += 4;

                        /* MEA: Should below sanity checks be warnings? */
                        lnh_val = testpkt_get_hdr_fld(bufp, "LRH", "LNH");
                        if (lnh_val == HFI_LRH_BTH && bidx != (LRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is BTH, but DETH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }
                        if (lnh_val == HFI_LRH_GRH && bidx != (LRH_LEN + GRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is GRH, but DETH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }

                        memset(bufp + bidx, 0, DETH_LEN);
                        /* No default DETH yet */

                        patch_ret = patch_hdr(bufp + bidx, lp, "DETH");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        bidx += DETH_LEN;
                        continue;
                }
                else if(!strncmp(lp, "KDETH", 5)) {
                        uint64_t lnh_val;
                        int patch_ret;

                        lp += 5;

                        /* MEA: Should below sanity checks be warnings? */
                        lnh_val = testpkt_get_hdr_fld(bufp, "LRH", "LNH");
                        if (lnh_val == HFI_LRH_BTH && bidx != (LRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is BTH, but DETH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }
                        if (lnh_val == HFI_LRH_GRH && bidx != (LRH_LEN + GRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is GRH, but DETH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }

                        memset(bufp + bidx, 0, DETH_LEN);

                        testpkt_put_hdr_fld(bufp + bidx, "KDETH", "KVer", 1);
                        testpkt_put_hdr_fld(bufp + bidx, "KDETH", "TID",
                                HFI_EAGER_TID_ID);
                        testpkt_put_hdr_fld(bufp + bidx, "KDETH", "J_KEY", 
                                stack_ctrl.j_key);

                        patch_ret = patch_hdr(bufp + bidx, lp, "KDETH");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        bidx += KDETH_LEN;
                        continue;
                    }
                else if(!strncmp(lp, "TEST", 4)) {
                        uint64_t lnh_val;
                        int patch_ret;

                        lp += 4;

                        /* MEA: Should below sanity checks be warnings? */
                        lnh_val = testpkt_get_hdr_fld(bufp, "LRH", "LNH");
                        if (lnh_val == HFI_LRH_BTH && bidx != (LRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is BTH, but IPTH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }
                        if (lnh_val == HFI_LRH_GRH && bidx != (LRH_LEN + GRH_LEN + BTH_LEN)) {
                            fprintf(stderr,"LNH is GRH, but IPTH macro at offset %d\n",bidx);
                            goto failed_macro;
                        }

                        int qp = testpkt_get_hdr_fld(bufp, 
                                "BTH", "DestQP") >> 16;
                        if(qp != stack_ctrl.qp) {
                            fprintf(stderr, "KDETH specified in file, but BTH DestQP does not match driver KDETH QP %x\n", qp);
                            goto failed_macro;
                        }


                        memset(bufp + bidx, 0, TEST_LEN);

                        patch_ret = patch_hdr(bufp + bidx, lp, "TEST");
                        if(patch_ret < 0) {
                            return patch_ret;
                        }

                        lp += patch_ret;

                        bidx += TEST_LEN;
                        continue;
                    }
                    //break;
            else if(bidx == 0 && !strncmp(lp, "Bypass8", 7)) {
                int patch_ret;
                lp += 7;

                testpkt_put_hdr_fld(bufp, "Bypass8", "DLID",bp->rlid & 0xFFFFF);
                testpkt_put_hdr_fld(bufp, "Bypass8", "SLID",bp->mlid & 0xFFFFF);

                patch_ret = patch_hdr(bufp + bidx, lp, "Bypass8");
                if(patch_ret < 0) {
                    return patch_ret;
                }

                lp += patch_ret;

                bidx += BYPASS8_LEN;

                /* Bypass packet length is in QUAD words, not dwords. */
                int bypass_len = testpkt_get_hdr_fld(bufp, "Bypass8", "Length");

                if (!bypass_len) {
                    /* Minimum bypass size is 2 quadwords. */
                    testpkt_put_hdr_fld(bufp, "Bypass8", "Length", 2);

                    /* Set to adjust length when full packet size is known. */
                    make_bypass = 8;
                }

                continue;
            } else if(bidx == 0 && !strncmp(lp, "Bypass10", 8)) {
                int patch_ret;
                lp += 8;

                testpkt_put_hdr_fld(bufp, "Bypass10", "DLID",
                        bp->rlid & 0xFFFFF);
                testpkt_put_hdr_fld(bufp, "Bypass10", "SLID",
                        bp->mlid & 0xFFFFF);
                testpkt_put_hdr_fld(bufp, "Bypass10", "L2", 1);

                patch_ret = patch_hdr(bufp + bidx, lp, "Bypass10");
                if(patch_ret < 0) {
                    return patch_ret;
                }

                lp += patch_ret;

                bidx += BYPASS10_LEN;

                /* Bypass packet length is in QUAD words, not dwords. */
                int bypass_len =
                    testpkt_get_hdr_fld(bufp, "Bypass10", "Length");

                if (!bypass_len) {
                    /* Minimum bypass size is 3 quadwords. */
                    testpkt_put_hdr_fld(bufp, "Bypass10", "Length", 3);

                    /* Set to adjust length when full packet size is known. */
                    make_bypass = 10;
                }

                continue;
            } else if(bidx == 0 && !strncmp(lp, "Bypass16", 8)) {
                int patch_ret;
                lp += 8;

                testpkt_put_hdr_fld(bufp, "Bypass16", "DLID",
                        bp->rlid & 0xFFFFF);
                testpkt_put_hdr_fld(bufp, "Bypass16", "DLID2",
                        bp->rlid >> 20);
                testpkt_put_hdr_fld(bufp, "Bypass16", "SLID",
                        bp->mlid & 0xFFFFF);
                testpkt_put_hdr_fld(bufp, "Bypass16", "SLID2",
                        bp->mlid >> 20);
                testpkt_put_hdr_fld(bufp, "Bypass16", "L2", 2);

                patch_ret = patch_hdr(bufp + bidx, lp, "Bypass16");
                if(patch_ret < 0) {
                    return patch_ret;
                }

                lp += patch_ret;

                bidx += BYPASS16_LEN;

                /* Bypass packet length is in QUAD words, not dwords. */
                int bypass_len =
                    testpkt_get_hdr_fld(bufp, "Bypass16", "Length");

                if (!bypass_len) {
                    /* Minimum bypass size is 3 quadwords. */
                    testpkt_put_hdr_fld(bufp, "Bypass16", "Length", 3);

                    /* Set to adjust length when full packet size is known. */
                    make_bypass = 16;
                }

                continue;
            } else {
                goto failed_macro;
            }
        }

failed_macro:
        if (isxdigit(*lp)) {
            /* Got a hex digit, accumulate.
            */
            dig <<= 4;
            dig |= isdigit(*lp) ? (*lp - '0')
                : (toupper(*lp) - 'A' + 10);
            lp += 1;
            ++dcnt;
        } else {
            /* Ignore most other characters for now, other than to
             * store any residue. We do this by making any
             * dcnt == 1 into dcnt == 2;
             */
            dcnt <<= 1;
            if (*lp == '#' || *lp == '\n' || *lp == '\0') {
                /* Comments are easier with line-at-a-time,
                 * just reset lp so we grab the next line and keep going.
                 * same for '\n'.
                 */
                lp = 0;
            } else if (!isspace(*lp)) {
                show_line(stdout, lineno, "Invalid input character",
                        linebuf, lp);
                return -1;
            } else {
                lp += 1;
            }
        }
        if (dcnt > 1) {
            /* got two digits, store in buffer, reset digit count
            */
            if (bidx >= bufsize) {
                /* grow the buffer to allow new input.
                 * modern realloc( 0, whatever) acts like malloc
                 */
                bufp = realloc(bufp, (bufsize += BUF_CHUNK));
                if (!bufp) {
                    return -1;
                }
            }
            bufp[bidx++] = dig;
            dig = 0;
            dcnt = 0;
        }
    }

    if (bidx) {
        /* Got a packet. Stash in chain. */
        struct fpkt *wp;
	unsigned length, vl;
        unsigned lnh_val = testpkt_get_hdr_fld(bufp, "LRH", "LNH");

	/* All packets must always be rounded to a multiple of a dword. */
	if (bidx & 3) {
	    int bytes_padded;
	    /* Packet not integral number of 4-octet words, pad */
	    bytes_padded = pad_buffer(bidx, 4);
	    if (bytes_padded == -1) {
		return -1;
	    }

	    bidx += bytes_padded;

	    if (make_bth) {
		unsigned hlen = LRH_LEN;

		if (lnh_val == HFI_LRH_GRH)
		    hlen += GRH_LEN;
		testpkt_put_hdr_fld(bufp + hlen,
			"BTH", "PadCnt", bytes_padded);
	    }
	}

	/* Bypass packets must be rounded to a quad word. */
	/* However, don't pad if the NOPAD suffix was specified. */
	if (!(need_Tbit & TBIT_NOPAD) &&
		(need_Tbit & TBIT_BYPASS) &&
		(bidx & 7)) {
	    /* Bypass packets must be an in integral number of quad words */
	    int bytes_padded;
	    bytes_padded = pad_buffer(bidx, 8);
	    if(bytes_padded == -1) {
		return -1;
	    }

	    bidx += bytes_padded;
	}

        /* Get space for descriptor and buffer, together for now
         * Pad supplied data to 32-bit boundary
         */
        wp = malloc(bidx + sizeof(struct fpkt));
        if (!wp) {
            perror("Memory for packets");
            return -1;
        }
        wp->next = NULL;
        wp->bufp = (uint8_t *) (wp + 1);
        wp->plen = bidx;

        memcpy(wp->bufp, bufp, bidx);

        /* Conversion of bytes to dwords plus two dwords for PBC */
        length = (bidx >> 2) + 2;

        wp->pbc.pbc0 = 1 << HFI_PBC_CREDITRETURN_SHIFT;

        int qp = testpkt_get_hdr_fld(wp->bufp + 8, "BTH", "DestQP");
        if(((qp >> 16) & 0xFF) == stack_ctrl.qp) {
            //KDETH is present.. is GRH? default(0) is no GRH
            if (lnh_val == HFI_LRH_GRH) {
                wp->pbc.pbc0 |= 1 << HFI_PBC_INSERTHCRC_SHIFT;
            }
        } else {
            wp->pbc.pbc0 |= 2 << HFI_PBC_INSERTHCRC_SHIFT;
        }

        if (make_lrh >= 0 &&
                testpkt_get_hdr_fld(wp->bufp, "LRH", "Len") == 0) {
            /* Most of LRH was set while parsing input.
             * Adjust LEN field now that we know it, but only if LRH length
             * was not already set by the input file.
             */
            unsigned int plenwds = bidx >> 2;

            /* If BTH is present, add one word for the ICRC */
            if(make_lrh >= HFI_LRH_BTH) {
                plenwds += 1;
            }

            testpkt_put_hdr_fld(wp->bufp, "LRH", "Len", plenwds);
        } else if (make_bypass > 0) {
            /* Most of Bypass header was set while parsing input.
               Adjust Length field now that we know it. */
            /* Bypass header length is QUAD words, not dwords! */
            unsigned int plenwds = (bidx + 7) >> 3;

            if(make_bypass == 8) {
                testpkt_put_hdr_fld(wp->bufp, "Bypass8", "Length", plenwds);
            } else if(make_bypass == 10) {
                testpkt_put_hdr_fld(wp->bufp, "Bypass10", "Length", plenwds);
            } else if(make_bypass == 16) {
                testpkt_put_hdr_fld(wp->bufp, "Bypass16", "Length", plenwds);
            }
        }

        /* If we need to set the Test bit, we also need to
         * compute the CRCs and append them. We left room for
         * both above if so.
         */
        if (need_Tbit) {
            if (need_Tbit & TBIT_EBP)
                wp->pbc.pbc0 |= 1 << HFI_PBC_TESTEBP_SHIFT;
            if (need_Tbit & TBIT_HCRC)
                wp->pbc.pbc0 |= 2 << HFI_PBC_INSERTHCRC_SHIFT;
            if (need_Tbit & TBIT_ICRC)
                wp->pbc.pbc0 |= 1 << HFI_PBC_TESTBADICRC_SHIFT;
            if (need_Tbit & TBIT_BYPASS)
                wp->pbc.pbc0 |= 1 << HFI_PBC_PACKETBYPASS_SHIFT;
            if (need_Tbit & TBIT_BYPASSICRC)
                wp->pbc.pbc0 |= 1 << HFI_PBC_INSERTBYPASSICRC_SHIFT;
            if (need_Tbit & TBIT_INTR)
                wp->pbc.pbc0 |= 1 << HFI_PBC_INTR_SHIFT;
            if (need_Tbit & TBIT_DCINFO)
                wp->pbc.pbc0 |= 1 << HFI_PBC_DCINFO_SHIFT;
            if (need_Tbit & TBIT_FECN)
                wp->pbc.pbc0 |= 1 << HFI_PBC_FECN_SHIFT;

            if (lnh_val < HFI_LRH_BTH && (need_Tbit & TBIT_BYPASS) == 0) {
                /* LNH says non-IB (RAW local or IPV6), no ICRC */
                length -= 1;
                wp->plen -= 4; /* In length for PIO-copy too */
            }
        }

	/* Set length to PBC because the calculation is done */
	wp->pbc.pbc0 |= length & HFI_PBC_LENGTHDWS_MASK;

	/* The PBC VL is set to match the LRH VL, if present.  The VL defaults
	 * to 0 (set in the default_pkt_bp struct) and can be overridden on the
	 * command line or in a packet input file using the LRH macro. */
	if(make_lrh == -1) {
	    /* No LRH macro was specified; use the default VL. */
	    vl = bp->vl;
	} else {
	    /* LRH exists; extract the VL from there. */
	    vl = testpkt_get_hdr_fld(bufp, "LRH", "VL");
	}
	wp->pbc.pbc0 |= (vl & 0xf) << HFI_PBC_VL_SHIFT;

	/* Now that the PBC is fully computed, consider any overrides specified
	 * using a PBC(...) macro in the input file.  Anything set by this
	 * macro overwrites the computed value for the respective field.
	 * Above, check for the PBC macro and store the string.  The string is
	 * stored so that it can be used to patch over the computed PBC here.
	 */
	if(pbc_patch_str != NULL) {
	    int patch_ret;

	    patch_ret = patch_hdr((uint8_t*)&wp->pbc_wd, pbc_patch_str, "PBC");
	    if(patch_ret < 0) {
		return patch_ret;
	    }

	    /* pbc_patch_str is allocated using strndup */
	    free(pbc_patch_str);
	}

        if (!file_pkt_hd) {
            /* Very first packet. */
            file_pkt_hd = wp;
        } else {
            file_pkt_last->next = wp;
        }
        file_pkt_last = wp;
    }

    return bidx;
}


static int dump_bypass_fpkt(struct fpkt* wp)
{
    int hlen = 0;

    fprintf(stdout,"PBC_WD: %"PRIX64"\n",wp->pbc_wd);

    /* Determine which bypass header format this is: use the L2 field. */
    int l2 = testpkt_get_hdr_fld(wp->bufp, "Bypass8", "L2");

    if(l2 == 0) { // 8 byte
        printf("--- 8 byte Bypass ---\n");
        hlen = dump_hdr(stdout, wp->bufp, 0, "Bypass8");
        printf("Bypass8 consumed %d\n", hlen);
    } else if(l2 == 1) { // 10 byte
        printf("--- 10 byte Bypass ---\n");
        hlen = dump_hdr(stdout, wp->bufp, 0, "Bypass10");
        printf("Bypass10 consumed %d\n", hlen);
    } else if(l2 == 2) { // 16 byte
        printf("--- 16 byte Bypass ---\n");
        hlen = dump_hdr(stdout, wp->bufp, 0, "Bypass16");
        printf("Bypass16 consumed %d\n", hlen);
    } else {
        printf("Unrecognized bypass packet L2 type %d\n", l2);
    }

    return hlen;
}

static int dump_nonbypass_fpkt(struct fpkt *wp)
{
    int len_so_far = 0;
    int hlen;
    int lnh;

    fprintf(stdout,"PBC_WD: %"PRIX64"\n",wp->pbc_wd);
    fprintf(stdout,"--- LRH ---\n");
    hlen = dump_hdr(stdout, wp->bufp, len_so_far, "LRH");
    fprintf(stdout,"LRH consumed %d\n",hlen);

    lnh = testpkt_get_hdr_fld(wp->bufp, "LRH", "LNH");
    len_so_far += hlen;

    if (lnh == HFI_LRH_GRH) {
        fprintf(stdout,"--- GRH ---\n");
        hlen = dump_hdr(stdout, wp->bufp, len_so_far, "GRH");
        fprintf(stdout,"GRH consumed %d\n",hlen);
        /* Fake header chaining, assume GRH is followed by BTH */
        lnh = 2;
        len_so_far += hlen;
    }
    if (lnh >= HFI_LRH_BTH) {
        uint32_t qp;
        uint8_t opcode;

        fprintf(stdout,"--- BTH --- %d\n", len_so_far);
        hlen = dump_hdr(stdout, wp->bufp, len_so_far, "BTH");
        fprintf(stdout,"BTH consumed %d\n",hlen);
        opcode = testpkt_get_hdr_fld(wp->bufp + len_so_far, "BTH", "OpCode");
        qp = testpkt_get_hdr_fld(wp->bufp + len_so_far, "BTH", "DestQP");
        len_so_far += hlen;

        /* this is a KDETH packet if the top 8 bits of the
           24-bit QP match */
        if (((qp >> 16) & 0xff) == stack_ctrl.qp) {
            fprintf(stdout,"--- KDETH ---\n");
            hlen = dump_hdr(stdout, wp->bufp, len_so_far, "KDETH");
            fprintf(stdout,"KDETH consumed %d\n",hlen);
            len_so_far += hlen;
            fprintf(stdout,"--- PSM ---\n");
            hlen = dump_hdr(stdout, wp->bufp, len_so_far, "PSM");
            fprintf(stdout,"PSM consumed %d\n",hlen);
            len_so_far += hlen;
        } else if (opcode == BTH_OP_UD_SEND ||
                opcode == BTH_OP_UD_SEND_IMM) {
            /* unreliable datagram */
            fprintf(stdout,"--- DETH ---\n");
            hlen = dump_hdr(stdout, wp->bufp, len_so_far, "DETH");
            fprintf(stdout,"DETH consumed %d\n",hlen);
            len_so_far += hlen;
            if (opcode == BTH_OP_UD_SEND_IMM) {
                fprintf(stdout,"--- Immediate Data ---\n");
                hlen = dump_hdr(stdout, wp->bufp, len_so_far, "ImmData");
                fprintf(stdout,"Immediate Data consumed %d\n",hlen);
                len_so_far += hlen;
            } else if (opcode == BTH_OP_UD_SEND && qp == 0) {
                fprintf(stdout,"--- MAD ---\n");
                hlen = dump_hdr(stdout, wp->bufp, len_so_far, "MAD");
                fprintf(stdout,"MAD consumed %d\n",hlen);
                len_so_far += hlen;
            }
        }
    }

    return len_so_far;
}


static void dump_fpkt(struct fpkt *wp)
{
    int hlen;
    int dlen;
    int idx;
    uint8_t *dp = NULL;

    if(wp == NULL || !wp->bufp || wp->plen == 0) {
        return;
    }
	dp = wp->bufp;
    /* Use the PBC to determine whether this is a bypass packet. */
    if((((wp->pbc.pbc0) >> HFI_PBC_PACKETBYPASS_SHIFT) & 0x1) == 1) {
        hlen = dump_bypass_fpkt(wp);
    } else {
        hlen = dump_nonbypass_fpkt(wp);
    }

    printf("# PBC (8)\n");
    printf("%02lX %02lX %02lX %02lX %02lX %02lX %02lX %02lX\n",
            wp->pbc_wd >> 56 & 0xFF, wp->pbc_wd >> 48 & 0xFF,
            wp->pbc_wd >> 40 & 0xFF, wp->pbc_wd >> 32 & 0xFF,
            wp->pbc_wd >> 24 & 0xFF, wp->pbc_wd >> 16 & 0xFF,
            wp->pbc_wd >> 8 & 0xFF, wp->pbc_wd & 0xFF);

    printf("# header bytes (%d)\n", hlen);
    for (idx = 0; idx < hlen; ++idx) {
        printf("%02X%c", dp[idx],
                (idx & 0xf) == 0xf ? '\n' : ' ');
    }

    printf("\n\n");
    dp = wp->bufp + idx;
    dlen = wp->plen - idx;

    printf("# payload bytes (%d)\n", dlen);
    for (idx = 0; idx < dlen; ++idx) {
        printf("%02X%c", dp[idx],
                (idx & 0xf) == 0xf ? '\n' : ' ');
    }
    printf("\n.\n");
}


static int testpkt_fd;
static unsigned unit;
static unsigned nobuf_retries;
static int wait_flag;

/*
 * We may not get a piobuf immediately.  In which case the test_pkt
 * interface returns ENOSPC. Retry a limitted amount of times in this
 * case.
 */
#define PKT_BUSY_TRIES 10

static int send_fpkt(struct fpkt *wp)
{
	int plen, rc, busy_tries;

	if (!wp || !wp->bufp) {
		fprintf(stderr, "Bogus wp\n");
		return -1;
	}

	plen = wp->plen;

	if(plen<0) {
		fprintf(stderr, "length plen incorrect\n");
		return -1;
	}

	if (testpkt_fd) {
		busy_tries = PKT_BUSY_TRIES;
		while (busy_tries) {
			rc = testpkt_send(testpkt_fd, unit, 
                                /*stack_ctrl.context*/ 0,
					  wp->bufp, plen, wp->pbc_wd,
					  wait_flag);
			if (rc < 0 && errno == ENOSPC) {
				/* no PIO buffer, may be transient */
				++nobuf_retries;
				--busy_tries;
				sleep(1);
			} else {
				break;
			}
		}
		if (rc < 0) {
			perror("Kernel Send");
			_HFI_ERROR("Kernel send failed after %d tries\n",
				PKT_BUSY_TRIES-busy_tries);
			return -1;
		}
	}
	else {
		rc = send_prefab_frame(wp);

		if (rc != OK) {
			_HFI_ERROR("send failed\n");
			return -1;
		}
	}
	return wp->plen;
}


static int skip_next_hdr()
{
    int rc = 0;

    assert(stack_ctrl.header_queue_index < stack_ctrl.header_queue_size);
    uint64_t offset = 
        stack_ctrl.header_queue_index * stack_ctrl.header_queue_elem_size;

    struct rh_flags rhf = *(volatile struct rh_flags*)
        ((uintptr_t)stack_ctrl.header_queue_base + 
                stack_ctrl.header_queue_rhf_offset + offset);

    /* Packet arrival is detected by polling for the next sequence number
       in the RH flags. */
    if(rhf.RcvSeq != stack_ctrl.header_queue_seqnum) {
        return 0;
    }

    if(rhf.RcvType != RCVHQ_RCV_TYPE_EAGER) {
        _HFI_DBG("not eager packet: type is %x, skipping\n", rhf.RcvType);
        goto pktskip;
    }

    if(rhf.Err != 0) {
        _HFI_INFO("RHF indicates packet has errors, skipping: %x\n", rhf.Err);
        goto pktskip;
    }

    /* Successful packet arrival */
    rc = 1;

pktskip:
    /* Advance the eager queue if a buffer was consumed. */
    if(rhf.UseEgrBfr == 1) {
            *stack_ctrl.eager_queue_head = rhf.EgrIndex;
    }

    /* Write the next expected offset to the hardware. */
    stack_ctrl.header_queue_index = 
        (stack_ctrl.header_queue_index + 1) % stack_ctrl.header_queue_size;

    /* *header_queue_head is in dword units, 
       header_queue_elem_size is bytes. */
    *stack_ctrl.header_queue_head = 
        (uint64_t)(stack_ctrl.header_queue_index * 
                   (stack_ctrl.header_queue_elem_size)) >> 2;

    return rc;
}

const char lid_fname[] = "/sys/class/infiniband/hfi1_%d/ports/%d/lid";

static int get_my_lid(int unit, int port)
{
	char lidstr[42];
	char fname[sizeof lid_fname];
	FILE *lfp;
	uint32_t lid = 0;

	snprintf(fname,sizeof fname, lid_fname, unit, port);
	lfp = fopen(fname, "r");
	if (lfp) {
		lidstr[0] = '\0';
		if (fgets(lidstr, sizeof lidstr, lfp)) {
			lid = strtoul(lidstr, 0, 0);
		}
	}
	return lid;
}



void usage(char *progname)
{
	printf(
"Usage: %s [-U Unit#] [-L LID] [-C Context] [-Q QP] [-P P_KEY] [-d lvl] [-c #] [-k] [-r] filename\n"
"Valid options are:\n"
"-U unit: Specify HFI device number (default 0 corresponding to /dev/hfi0)\n"
"-L <remote lid>   - dest LID (ignored if pkt file specifies LID)\n"
"-C <remote ctxt>  - dest context (ignored if pkt file specifies context)\n"
"-Q <QP num>       - dest queue pair (ignored if pkt file specifies QP)\n"
"-V <VL num>       - Virtual Lane (ignored if pkt file specifies VL)\n"
"-P <P_KEY>        - Set user-context P_KEY (Default 0x8001)\n"
"-S <SL num>       - Service Level (ignored if pkt file specifies SL)\n"
"-d <debug_flags>  - enable different debug levels\n"
"-c <count>        - # of packets to send, or copies packets in file\n"
"                    Infinity if set to -1. Default is 100\n"
"-k                - use kernel (diagpkt) interface\n"
"-r                - check the receive queue and drop packets\n"
"\"filename\"        - contains packet contents to be sent (in ascii)\n"
"                    Numeric base decimal, unless 0x prefix for hex, 0 for\n"
"                    octal\n"
"-v                - display version information and exit\n"
"-w                - wait for credits to be returned (diagpkt only)\n"
"-h                - display this help message\n"
		, progname);
}

static const char valid_args[] = "Bc:C:d:D:hkl:L:Q:rS:U:V:vwP:";

int main(int argc, char **argv)
{
        const char *fname = NULL;
	char *progname = argv[0];
	uint64_t loops = 100;
        int unit_number = 0;
	int i;
	pkt_bp *bp;
	int loop_forever = 0;
	int do_recv = 0;
	char *lp;

	bp = &default_pkt_bp;
	bp->rlid = 0;
	bp->remote_ctxt = ~0;


	if (!progname)
		progname = "hfi1_pkt_send";
	lp = strrchr(progname,'/');
        if (lp)
		progname = lp + 1;
	while ((i = getopt(argc, argv, valid_args)) != -1)
		switch (i) {
		case 'l':	// counts or
		case 'c':	// loops - the same
			loops = strtoul(optarg, NULL, 0);
			if (loops == ~(uint64_t)0) {
				loop_forever = 1;
			}
			break;
		case 'd':	// debug
			hfi_debug =
			    (uint32_t) strtoul(optarg, NULL, 0);
			fprintf(stderr,"hfi_debug = %d (0x%X), PKT = %0x\n",
				hfi_debug, hfi_debug, __HFI_PKTDBG);
			break;
		case 'B': /* Batch trigger mode, send all packets at once */
			def_trig_idx = -1;
			break;
		case 'k': /* Use Kernel (diagpkt) interface for sends */
			testpkt_fd = testpkt_open();
			if (testpkt_fd < 0) {
				fprintf(stderr, "No access to kernel diagpkt interface; aborting\n");
				exit(1);
			}
			break;
		case 'L':	// lid
			bp->rlid = (uint32_t) strtoul(optarg, NULL, 0);
			break;

		case 'C':	// "port" or Context
			bp->remote_ctxt =
			    (uint16_t) strtoul(optarg, NULL, 0);
			break;

		case 'Q':	// QP number
			bp->qp = (uint32_t) strtoul(optarg, NULL, 0);
			break;

		case 'r':	// check receive queue and drop packets
			do_recv = 1;
			break;

		case 'S': /* Virtual Lane */
			bp->sl  = (uint8_t)strtoul(optarg, NULL, 0);
			break;

		case 'U': /* Unit number */
			unit_number = (int)strtoul(optarg, NULL, 0);
			break;

		case 'V': /* Virtual Lane */
			bp->vl  = (uint8_t)strtoul(optarg, NULL, 0);
			break;

		case 'v':
			printf("%s %s\n", __DIAGTOOLS_GIT_VERSION,
				__DIAGTOOLS_GIT_DATE);
			exit(0);
			break;
		case 'w':
			wait_flag = 1;
			break;
		case 'P': // User-context P_KEY
			ctxt_p_key = (uint16_t) strtoul(optarg, NULL, 0);
			break;
		default:
			usage(progname);
			exit(1);
			break;
		}		/* end switch _and_ while */

	if (optind < argc) {
		fname = argv[optind];
		fprintf(stdout, "Will read %s\n", fname ? fname : "<Bogus>");
	}
	
	signal(SIGINT, hfi_pkt_signal);
	signal(SIGTERM, hfi_pkt_signal);

	if (!fname && !bp->rlid) {
		/* generating packets, and no DLID */
		_HFI_ERROR("Remote LID not set\n");
		exit(1);
	}

	if (!fname && bp->remote_ctxt == (uint16_t) ~ 0U) {
		/* generating packets, and no remote context */
		_HFI_ERROR("Remote context not set\n");
		exit(1);
	}

	if (!testpkt_fd) {
	    if (hfi_pkt_open(unit_number, &bp->mlid, &bp->my_ctxt) != 0) {
		_HFI_ERROR("Open failed: %s\n", strerror(errno));
		exit(1);
	    }
	} else {
            /* Using kernel diagpkt interface */
	    if(do_recv) {
		printf("Warning: there is no receive context to check when driver diagpkt interface (-k) is used, disabling receive checks (-r).\n");
		do_recv = 0;
	    }

	    /* substitute plausible values for bits of stack_ctrl we really use */
	    unit = unit_number;
	    stack_ctrl.qp = 0x80; /* default KDETH QP prefix */
	    bp->mlid = get_my_lid(unit, /*stack_ctrl.context*/ 1);
	    bp->my_ctxt = 0; 
	}


	printf("My source LID is %i (0x%x), ctxt %i\n", 
		bp->mlid, bp->mlid, bp->my_ctxt);
	if (bp->rlid)
		printf("remote LID is %i (0x%x)",
			bp->rlid, bp->rlid);
	if (!fname)
		printf(" remote ctxt %i", bp->remote_ctxt);
	if (!fname || bp->rlid)
		printf("\n");

	if (fname) {
		FILE *ifp;
		int retval;

		ifp = fopen(fname, "r");
		if (!ifp) {
			perror(fname);
			return errno;
		}
		do {
			retval = get_pkt(ifp);
		}
		while (retval > 0);

                if(retval < 0) {
                    fprintf(stderr,
                            "get_pkt input file (%s) parsing failed: %d\n",
                            fname, retval);
                    exit(1);
                }
	}

	if (hfi_debug & __HFI_PKTDBG) {
		int key = 0;
		struct fpkt *wp;

		while (1) {
			wp = get_pkt_desc(key++);
			if (!wp)
				break;
			dump_fpkt(wp);
		}
		printf("%d packets in file\n", key - 1);
	}

	if (testpkt_fd && stack_ctrl.file_desc) {
		/* if using kernel send, close "normal" connection
		 * to free up buffs for kernel. We already have info we
		 * need.
		 */
		fprintf(stderr,"Closing HFI fd (%d) to free buffs\n",stack_ctrl.file_desc);
		close(stack_ctrl.file_desc);
		stack_ctrl.file_desc = 0;
		sleep(5);
	}
	if (def_trig_idx && testpkt_fd) {
		fprintf(stderr,"No deferred-trigger with kernel sends\n");
	}
	if (def_trig_idx && !testpkt_fd) {
		/* user wants deferred-trigger mode, set up needed state. */
                int total_credits = stack_ctrl.spio_total_send_credits;
		fprintf(stderr,
			"Allocating %d deferred-trigger structs, %d bytes ",
			total_credits,
			(int) (total_credits * sizeof *def_trig_p));

		def_trig_p = malloc((size_t)
				    (total_credits * sizeof *def_trig_p));

		fprintf(stderr, "at %p\n", def_trig_p);

		if (def_trig_p) {
			/* got the memory, init it. */
			def_trig_idx = 0;
			def_trig_p[0].addr = 0;
		} else {
			perror("Deferred Trig state");
		}
	}

	if (fname) {
		int retval = 0;
		int key;
		uint64_t loop;
		struct fpkt *wp;

		for (loop = 1; loop_forever || loop <= loops; ++loop) {
			key = 0;
			while (1) {
				wp = get_pkt_desc(key++);
				if (!wp)
					break;
				retval = send_fpkt(wp);
				if (retval < 0) {
					/* don't display the packet if it's
					 * just an IB link problem */
                                    //AWF - check_init_status unimplemented
#if 0
					int rc = hfi_check_unit_status(hfi_ctrl);
					if (rc != IPS_RC_NETWORK_DOWN &&
					    rc != IPS_RC_BUSY)
#endif
                                        dump_fpkt(wp);
					break;
				}
				if (nobuf_retries) {
					printf("%d nobuf retries\n", nobuf_retries);
					nobuf_retries = 0;
				}
			}	/* end while (preloading or sending all packets) */
			if (retval < 0) {
                            fprintf(stderr, "ERROR sending packet: %d\n", retval);
                            exit(1);
                        }

			if (def_trig_idx) {
				retval = do_def_trigs();
				if (loop < 10) {
					printf("%d deferred triggers hit\n",
					     retval);
				}
			}

			while (do_recv && skip_next_hdr())
				;
		}		/* end for (loops) */
	} else {
		fprintf(stderr,"Currently support only packets from file\n");
		usage(progname);
        }

	if (stack_ctrl.file_desc)
		close(stack_ctrl.file_desc);
	exit(0);
}


