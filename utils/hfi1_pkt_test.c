/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2014, 2015 Intel Corporation.
 * Copyright (C) 2006, 2007, 2009 QLogic Corporation, All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc.  All rights reserved.
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
 * Copyright(c) 2014, 2015 Intel Corporation.
 * Copyright (C) 2006, 2007, 2009 QLogic Corporation, All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc.  All rights reserved.
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

#include <netinet/in.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <sched.h>
#include <stdlib.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/mman.h>

#include "opa_user.h"
#include "opa_service.h"
#include "ipserror.h"
#include "git_version.h"

#include <linux/stddef.h>


static void do_buftest(int unit_number, int loops)
	__attribute__ ((__noreturn__));
static void do_responses(int justack) __attribute__ ((__noreturn__));

/* payload length adjustment: first cache line + trigger word */
//#define PIOTEST_EXTRALEN 68

#define MAX_FAILS 50 /* for -e */


/*
 * CONST 
 */
#define ERR_TID HFI_RHF_TIDERR
#define LAST_RHF_SEQNO 13

//Request a send credit update from the hardware when fewer than
// this many credits are available.
//TODO - come up with a smarter credit handling scheme.
// Expose a command line parameter to control it:
//  A special value (0?) should enable infinite wait for credits.
//  Maybe another value should enable zero wait for credits -- just drop.
#define SEND_CREDIT_UPDATE_THRESH      128
#define SEND_CREDIT_UPDATE_RETRIES     10000

#ifndef HFI_RCVEGR_ENTRIES_DFLT
#define HFI_RCVEGR_ENTRIES_DFLT 1024
#endif

#define FOREVER 1

#define TRUE 1
#define FALSE 0
#define OK 0

#define MAX_PAYLOAD (10*1024)

/*
 * IB - LRH header consts 
 */
#define HFI_LRH_GRH 0x0003	/* 1. word of IB LRH - next header: GRH */
#define HFI_LRH_BTH 0x0002	/* 1. word of IB LRH - next header: BTH */

/*
 * BTH header consts
 */
#define HFI_BTH_OPCODE 0xC0    /* First manufacturer-reserved opcode */

/*
 * Receive Header Queue: receive type
 */
#define RCVHQ_RCV_TYPE_EXPECTED  0
#define RCVHQ_RCV_TYPE_EAGER     1
#define RCVHQ_RCV_TYPE_NON_KD    2
#define RCVHQ_RCV_TYPE_ERROR     3

/*
 * RHF error flag masks
 */

#define RHF_KHDRLENERR  0x001
#define RHF_DCUNCERR    0x002
#define RHF_DCERR       0x004
#define RHF_RCVTYPEERR  0x038
#define RHF_TIDERR      0x040
#define RHF_LENERR      0x080
#define RHF_ECCERR      0x100
#define RHF_RESERVED    0x200
#define RHF_ICRCERR     0x400

/*
 * misc. 
 */
#define IPS_PROTO_VERSION 0x1 //WFR Kver KDETH field value
#define SIZE_OF_CRC 1

#define min(a,b) ((a) < (b) ? (a) : (b))

/*
 * timer macros 
 */
#define us_2_cycles(us) nanosecs_to_cycles(1000ULL*(us))
#define ms_2_cycles(ms)  nanosecs_to_cycles(1000000ULL*(ms))
#define sec_2_cycles(sec) nanosecs_to_cycles(1000000000ULL*(sec))


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


typedef struct __attribute__((__packed__)) hfi_diags_pkt_header {
	__be16 lrh[4];
	__be32 bth[3];

        struct hfi_kdeth khdr;

	__le16 length;    /* This is filled in and used locally */
	__le16 seq_num;   /* This is filled in and checked/used remotely */
        __le32 user[4];
	__le64 time_stamp; /* This is used */
} hfi_diags_pkt_header;


typedef struct _hfi_pkt {
	hfi_diags_pkt_header h;
	char data[MAX_PAYLOAD];
} hfi_pkt;


struct _stack_ctrl {
	int file_desc;

	uint8_t vl;
        uint8_t sl;
	uint16_t j_key;
	uint32_t qp;                    /* QP value indicating KDETH hdr */
	uint32_t mtu;
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


/*
 * global VARS 
 */

static struct _hfi_ctrl *hfi_ctrl = NULL;

static uint32_t drops_since_inseq;

static struct _stack_ctrl stack_ctrl;

static hfi_pkt send_frame;
static uint16_t my_ctxt;
static uint16_t my_lid;
static uint16_t remote_lid;
static int16_t remote_ctxt = -1;
static uint16_t p_key = HFI_DEFAULT_P_KEY;

static uint64_t rcvd_badseq, rcvd_inseq;


/*
 * user mode 
 */

static uint16_t vl_lver_sl_lnh = HFI_LRH_BTH;

static uint64_t time_mark1;
static uint64_t time_mark2;


static void hfi_pkt_signal(int param)
{
	(void)param;
	close(stack_ctrl.file_desc);
	exit(1);
}



/* useful for general packet dumping when debugging  */
static void dump_buf(char *what, uint8_t *bytes, unsigned len)
{
	int idx;
	printf("%s: 0x%x bytes", what, len);
	for ( idx = 0; idx < len; ++idx)
		printf("%c%02X",(idx & 0xF) ? ' ' : '\n', bytes[idx]);
	putchar('\n');
	fflush(stdout);
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
        return -1;
    }

#if 0
    printf("fill_send_credits %d free_credits %d val %d %lx math %d\n",
            stack_ctrl.spio_fill_send_credits,
            free_credits->Counter,
            free_credits_val, credit_val,
            (stack_ctrl.spio_fill_send_credits - free_credits->Counter) % 2048);
#endif

    //Can this be optimized down to two cycles?
    //Reversing avail_send_credits might help performance.
    // Count only used credits here, then compare to total credits.
    stack_ctrl.spio_avail_send_credits = stack_ctrl.spio_total_send_credits -
        ((stack_ctrl.spio_fill_send_credits - free_credits->Counter) & 0x7FF);

    return 0;
}


/*
 * Write a packet to the PIO send buffer.
 */

static void write_pio_packet(void* header, void* payload, int length)
{
    struct hfi_pbc buf;
    int i;
    int index = stack_ctrl.spio_cur_slot;

    memset(&buf, 0, sizeof(struct hfi_pbc));

    //TODO - change this to return credits every X packets?
    /* 64 is added to accommodate PBC + header. */
    /* Modulus check is to make sure we don't chop off the last 1-3 bytes
       for lengths that are not a multiple of a DW. */
    buf.pbc0 = (((64 + length) >> 2) & HFI_PBC_LENGTHDWS_MASK) |
		(((stack_ctrl.vl) & HFI_PBC_VL_MASK) << HFI_PBC_VL_SHIFT) |
		(1 << HFI_PBC_CREDITRETURN_SHIFT);


    volatile uint64_t* addr_sop = (volatile uint64_t*)
        ((uintptr_t)stack_ctrl.spio_buffer_base_sop + (uint64_t)(index * 64));

    /* Write the PBC block (8 bytes) */
    /* addr_sop is incremented to avoid repeating it in the loop(s) below. */
    *addr_sop++ = *(uint64_t*)&buf;

    /* Write the remaining 56 bytes of packet header */
    volatile uint64_t* src_hdr = header;

    /* Copy 7 quadwords: the first quadword (PBC) is already copied. 
       addr_sop is already incremented to the next quadword. */
    for(i = 0; i < 7; i++) {
        addr_sop[i] = src_hdr[i];
    }


    /* Write the payload.  Has to be >= 8-byte writes in 64byte blocks.
       For payloads that don't round out to 64 bytes, we have to pad with 0s. */
    volatile uint64_t* addr = stack_ctrl.spio_buffer_base;
    if(index + 1 != stack_ctrl.spio_total_send_credits) {
        addr = (volatile uint64_t*)((uintptr_t)addr + ((index + 1) * 64));
    }

    volatile uint64_t* src_data = payload;
    int remaining_length = length;

    /* This loop only copies whole 64-byte blocks. */
    for(; remaining_length >= 64; remaining_length -= 64) {
        assert(((uintptr_t)addr & (uintptr_t)0x3F) == 0);

        /* Copy 8 quadwords (64 bytes) of payload at a time. */
        for(i = 0; i < 8; i++) {
            *addr++ = *src_data++;
        }

        if(addr == stack_ctrl.spio_buffer_end) {
            addr = stack_ctrl.spio_buffer_base;
        }
    }

    assert(remaining_length < 64);
    assert(((uintptr_t)addr & (uintptr_t)0x3F) == 0);

    /* Handle the last credit block: <64 bytes to copy. */
    /* Copy as many whole quadwords as possible */
    while(remaining_length >= 8) {
        *addr++ = *src_data++;
        remaining_length -= 8;
    }

    assert(remaining_length == 4 || remaining_length == 0);

    /* If a dword remains, pack it into a quadword and copy. */
    if(remaining_length == 4) {
        uint64_t qword = *(uint32_t*)src_data;
        *addr++ = qword;
    }

    /* Finally, write 0 to remaining quadwords. */
    /* Write 0-value qwords until addr is aligned to a 64-byte multiple. */
    while(((uintptr_t)addr & (uintptr_t)0x3F) != 0) {
        *addr++ = (uint64_t)0;
    }
}


static int _transfer_frame(void *header, void *payload, int length)
{
    int num_credits = 1 + (length / 64) + (length % 64 ? 1 : 0);
    int credits_needed = num_credits > SEND_CREDIT_UPDATE_THRESH ? 
        num_credits : SEND_CREDIT_UPDATE_THRESH;

    /* length has to be a multiple of a DW (4 bytes) */
    assert(length % 4 == 0);
    assert((num_credits - 1) * 64 >= length);

    //printf("spio_avail_send_credits %d index %d num_credits %d credits_needed %d\n", stack_ctrl.spio_avail_send_credits, index, num_credits, credits_needed);
    //usleep(1000);

    int i;

    update_send_credits();

    for(i = 0; i < SEND_CREDIT_UPDATE_RETRIES &&
            stack_ctrl.spio_avail_send_credits < credits_needed; i++) {
        usleep(1);
        update_send_credits();
    }

    if(i == SEND_CREDIT_UPDATE_RETRIES) {
        _HFI_ERROR("no credits available for PIO send: cur_slot %d seq %d available %d need %d\n", 
                stack_ctrl.spio_cur_slot, 
                ((struct hfi_diags_pkt_header*)header)->seq_num,
                stack_ctrl.spio_avail_send_credits,
                credits_needed);
        return IPS_RC_NO_RESOURCE_AVAILABLE;
    }

    stack_ctrl.spio_fill_send_credits += num_credits;
    stack_ctrl.spio_avail_send_credits -= num_credits;

    write_pio_packet(header, payload, length);

    /* Update this after writing: write_pio_send references spio_cur_slot. */
    stack_ctrl.spio_cur_slot =
        (stack_ctrl.spio_cur_slot + num_credits) % 
        stack_ctrl.spio_total_send_credits;

    return IPS_RC_OK;
}

// linux doesn't have strlcat; this is a stripped down implementation
// not super-efficient, but we use it rarely, and only for short strings
// not fully standards conforming!
static size_t strlcat(char *d, const char *s, size_t l)
{
	int dlen = strlen(d), slen, max;
	if (l <= dlen)		// bug
		return l;
	slen = strlen(s);
	max = l - (dlen + 1);
	if (slen > max)
		slen = max;
	memcpy(d + dlen, s, slen);
	d[dlen + slen] = '\0';
	return dlen + slen + 1;	// standard says to return full length,
	// not actual
}

// decode RHF errors; only used one place now, may want more later
static void get_rhf_errstring(uint32_t err, char *msg, size_t len)
{
	*msg = '\0';		// if no errors, and so don't need to
	// check what's first

	if (err & RHF_ICRCERR)
		strlcat(msg, "ICRCerr ", len);
	if (err & RHF_ECCERR)
		strlcat(msg, "ECCerr ", len);
	if (err & RHF_LENERR)
		strlcat(msg, "LENerr ", len);
	if (err & RHF_TIDERR)
		strlcat(msg, "TIDerr ", len);

	if (err & RHF_RCVTYPEERR)
		strlcat(msg, "RCVTYPEerr ", len);

	if (err & RHF_DCERR)
		strlcat(msg, "DCerr ", len);
	if (err & RHF_DCUNCERR)
		strlcat(msg, "DCUNCerr ", len);
	if (err & RHF_KHDRLENERR)
		strlcat(msg, "KHDRLENerr ", len);
        if (err & RHF_RESERVED)
		strlcat(msg, "Reserved ", len);

}

// separate function to reduce cache impact, since errors are rare.
//static void show_rhf_errors(uint32_t * rhdr)
static void show_rhf_errors(uint64_t offset)
{
    char emsg[256];
    uint32_t error_flags;
    uint8_t op_code, ptype;
    hfi_diags_pkt_header *phdr;

    volatile struct rh_flags* rhf = (struct rh_flags*)
        ((uintptr_t)stack_ctrl.header_queue_base + 
                stack_ctrl.header_queue_rhf_offset + offset);

    error_flags = rhf->Err;
    ptype = rhf->RcvType;
    phdr = (hfi_diags_pkt_header*)
	    ((uintptr_t)stack_ctrl.header_queue_base + offset +
	     (rhf->HdrqOffset << 2));
    op_code = __be32_to_cpu(phdr->bth[0]) >> 24 & 0xff;

    // some of this value may not be valid, but still want to
    // print it, for debugging purposes.
    get_rhf_errstring(error_flags, emsg, sizeof emsg);
    if (error_flags & ERR_TID) {	// print this type at debug only
#if __HFI_INFO || __HFI_DBG
	uint32_t kdeth0 = __le32_to_cpu(phdr->khdr.kdeth0);
	uint16_t tid = (kdeth0 >> HFI_KHDR_TID_SHIFT) & HFI_KHDR_TID_MASK;
#endif
        if (ptype == RCVHQ_RCV_TYPE_EXPECTED) {	// this is bad
            // in fact, almost certainly fatal, since we'll keep
            // resending from the other side with the same bad TID,
            // unless it was extremely unlikely packet corruption
            // Therefore we report this at INFO, so it's always seen
            // unless all error reporting is disabled
            _HFI_INFO
                ("ExpSend opcode %x tid=%x, rhf_error %x: %s\n",
                 op_code, tid, error_flags, emsg);
        } else
            _HFI_DBG
                ("ptype %x opcode %x tid=%x, rhf_error %x: %s\n",
                 ptype, op_code, tid, error_flags, emsg);
    } else
        _HFI_DBG("Error pkt type %x opcode %x, rhf_error %x: %s\n",
                ptype, op_code, error_flags, emsg);
}


/* check to see if another is there, used to decide to delay or not.  Because
 * setting up poll() is moderately expensive, and only the responder uses this,
 * loop a few times before saying there aren't any packets, or we can fall into
 * a more or less pingpong state where we only get a few packets, poll, get
 * behind so sender pauses, and repeat.
 */
static unsigned more_rcvpkts(void)
{
    unsigned pkts = 0, cnt = 100;

    uint64_t offset = 
        stack_ctrl.header_queue_index * stack_ctrl.header_queue_elem_size;

    volatile struct rh_flags* rhf = (struct rh_flags*)
        ((uintptr_t)stack_ctrl.header_queue_base + 
                stack_ctrl.header_queue_rhf_offset + offset);

    //TODO - HFI_RUNTIME_NODMA_RTAIL stuff.. what is that?
    // I should probably support it.
    do {
        pkts = (rhf->RcvSeq == stack_ctrl.header_queue_seqnum);
    } while (!pkts && --cnt);

#if 0
	do {
		curr_rcv_hdr = stack_ctrl.header_queue_base + rcv_hdr_head;
                //TODO - RHF format changed, update accordingly.
		rhf = (const __le32 *) curr_rcv_hdr +
				stack_ctrl.header_queue_rhf_offset;

		if (stack_ctrl.runtime_flags & HFI_RUNTIME_NODMA_RTAIL) {
			uint32_t seq;
			seq = hfi_hdrget_seq(rhf);
			pkts = rcv_hdr_seq == seq;
		}
		else
			pkts = rcv_hdr_head != hfi_get_rcvhdrtail(hfi_ctrl);
	} while (!pkts && --cnt);
	return pkts;
#endif
        return 0;
}


// get the next hdrq entry pointer, returning NULL if none
// similar to code in psm (not exported), but for our simpler
// environment.
static hfi_diags_pkt_header *get_next_hdr(uint32_t *rcv_egr_index_head)
{
    uint8_t op_code;
    static uint64_t same;
    hfi_diags_pkt_header* hdr;

    *rcv_egr_index_head = ~0U;

    assert(stack_ctrl.header_queue_index < stack_ctrl.header_queue_size);
    uint64_t offset = 
        stack_ctrl.header_queue_index * stack_ctrl.header_queue_elem_size;


    struct rh_flags rhf = *(volatile struct rh_flags*)
        ((uintptr_t)stack_ctrl.header_queue_base + 
                stack_ctrl.header_queue_rhf_offset + offset);

    /* Packet arrival is detected by polling for the next sequence number
       in the RH flags. */
    if(rhf.RcvSeq != stack_ctrl.header_queue_seqnum) {
        if (!(++same % 1000000ULL)) {
            _HFI_DBG("No pkts %12llu times\n", (unsigned long long)same);
        }

        return NULL;
    }

    if (same > 1000000ULL)
        _HFI_DBG("got a pkt after %llu times\n",
                (unsigned long long)same);
    same = 0ULL;

    /* seqnum ranges from 1-LAST_RHF_SEQNO inclusive, 
       so use a branch instead of mod. */
    if(stack_ctrl.header_queue_seqnum == LAST_RHF_SEQNO) {
        stack_ctrl.header_queue_seqnum = 1;
    } else {
        stack_ctrl.header_queue_seqnum += 1;
    }

    if(rhf.RcvType != RCVHQ_RCV_TYPE_EAGER) {
        _HFI_DBG("not eager packet: type is %x, skipping\n", rhf.RcvType);
        goto pktskip;
    }

    if(rhf.Err != 0) {
        char msg[64];
        get_rhf_errstring(rhf.Err, msg, sizeof msg);
        _HFI_INFO("RHF indicates packet has errors, skipping: %s\n", msg);
        /* normally prints under debug only */
        show_rhf_errors(offset);
        goto pktskip;
    }

    hdr = (hfi_diags_pkt_header*)((uintptr_t)stack_ctrl.header_queue_base +
				  offset + (rhf.HdrqOffset << 2));

    if(rhf.PktLen != __be16_to_cpu(hdr->lrh[2])) {
        printf("ERROR RHF length %d does not equal LRH length %d (bytes)\n", 
                rhf.PktLen << 2, __be16_to_cpu(hdr->lrh[2] << 2));
        goto pktskip;
    }

    op_code = __be32_to_cpu(hdr->bth[0]) >> 24 & 0xff;
    if (op_code != HFI_BTH_OPCODE) {
        _HFI_DBG("packet opcode is 0x%x, not 0x%x, skipping\n",
                op_code, HFI_BTH_OPCODE);
        if (hfi_debug & __HFI_PKTDBG) {
            unsigned len = min(__be16_to_cpu(hdr->lrh[2]) << 2,
                    (sizeof(hfi_diags_pkt_header) + sizeof(__le64)));
            show_rhf_errors(offset);
            dump_buf("Pkt header, wrong opcode\n",
                    (uint8_t *)&hdr, len);
        }

        goto pktskip;
    }

    if(rhf.UseEgrBfr == 1) {
        *rcv_egr_index_head = rhf.EgrIndex;
    }

    /* In this (successful packet) case, do not update the RHQ head yet.
       Wait until this packet has been processed by respond(). */
    return hdr;

pktskip:
    //Write the next expected offset to the hardware.
    stack_ctrl.header_queue_index = 
        (stack_ctrl.header_queue_index + 1) % stack_ctrl.header_queue_size;

    //*header_queue_head is in dword units, header_queue_elem_size is bytes.
    *stack_ctrl.header_queue_head = 
        (uint64_t)(stack_ctrl.header_queue_index * 
                   (stack_ctrl.header_queue_elem_size)) >> 2;

    return NULL;
}

// ack_type == -1 means no ack at all; the former "waiting_for_reply()"
// code path, where we just see if there is a packet, and increment
// the head registers if there is.
// If ack_type == 0, we respond with the incoming payload.
// Oother non-0 means respond with just a minimal ack packet (normal case).
// Payload and paylen are only used for ack_type == 0.
// Returns FALSE if no packet or packet had error, else TRUE.
// Callers who care about sequence number match pass in arg to get
// sequence number received.
static int respond(int ack_type, unsigned seqnum, unsigned *rcvdseq)
{
	uint32_t rcv_egr_index_head;
	unsigned seq;
	uint8_t *payload;
	hfi_diags_pkt_header *protocol_header, nprotocol_header;

	if(ack_type == -1)
		time_mark1 = get_cycles();

        /* Check for a waiting packet. */
	protocol_header = get_next_hdr(&rcv_egr_index_head);
	if(!protocol_header) {
		return FALSE; // no packets, or error
        }

	if(rcv_egr_index_head != ~0U) {
            payload = (void*)(stack_ctrl.eager_queue_base +
                    (rcv_egr_index_head * stack_ctrl.eager_queue_elem_size));
        }
	else
		payload = NULL;

	seq = __le16_to_cpu(protocol_header->seq_num);
	if(seq != seqnum)
		rcvd_badseq++;
	else {
		rcvd_inseq++;
		drops_since_inseq = 0;
	}

	if(rcvdseq)
		*rcvdseq = seq;

	if(ack_type != -1) {
		uint32_t ctxt;
        int payload_length = 0;


		memset(&nprotocol_header, 0, sizeof(nprotocol_header));
		nprotocol_header = *protocol_header;
		/* we respond with our programmed VL/SL, not the one that we
		 * got.  Can be different one direction than the other */
		nprotocol_header.lrh[0] = __cpu_to_be16(vl_lver_sl_lnh);

                /* Copy and exchange the DLID/SLID values */
		nprotocol_header.lrh[1] = protocol_header->lrh[3];
		nprotocol_header.lrh[3] = protocol_header->lrh[1];

		if(ack_type) { /* normal, just flip back the header we got */
			nprotocol_header.length = 0;
			nprotocol_header.lrh[2] = __cpu_to_be16(
				(sizeof(hfi_diags_pkt_header)>>2) + SIZE_OF_CRC);
                        payload_length = 0;
			payload = NULL;
		}
		else {
			nprotocol_header.lrh[2] = protocol_header->lrh[2];
                        payload_length = (nprotocol_header.length << 2) -
                            sizeof(hfi_diags_pkt_header);
		}


		/* flip back the full packet we got */
		protocol_header = &nprotocol_header;

                /* The sender's context is encoded in the PSN field */
		ctxt = __be32_to_cpu(protocol_header->bth[2]);

                protocol_header->bth[1] =
                    __cpu_to_be32((stack_ctrl.qp << 16) | ctxt);

		/* Only kver and jkey need to be set for eager packets. */
		protocol_header->khdr.kdeth0 =
			(IPS_PROTO_VERSION << HFI_KHDR_KVER_SHIFT);
                protocol_header->khdr.job_key = stack_ctrl.j_key;

		_transfer_frame(protocol_header, payload, 
                        payload_length);
                        //__le16_to_cpu(protocol_header->length));
	}

	if(rcv_egr_index_head != ~0U) {
            /* Update the eager buffer queue head. */
            //TODO - as an optimization, update this only periodically,
            // not for every packet.
            *stack_ctrl.eager_queue_head = rcv_egr_index_head;
        }

	/*
	 * update the headerQueue head pointer 
	 */

        //Write the next expected offset to the hardware.
        stack_ctrl.header_queue_index = 
            (stack_ctrl.header_queue_index + 1) % stack_ctrl.header_queue_size;

            //*header_queue_head is in dword units, 
            // header_queue_elem_size is bytes.
            *stack_ctrl.header_queue_head = 
                (uint64_t)(stack_ctrl.header_queue_index * 
                        (stack_ctrl.header_queue_elem_size)) >> 2;

	if(ack_type == -1)
		time_mark2 = get_cycles();

	return TRUE;
}


static int hfi_pkt_open(int unit, uint16_t * my_lid, uint16_t * my_ctxt)
{
	struct hfi1_user_info_dep hfi_drv_user_info;
        struct hfi1_base_info* hfi_drv_base_info;
        struct hfi1_ctxt_info* hfi_drv_ctxt_info;
	int mylid, mymtu;
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
	cmd.addr = (uint64_t)p_key;

	if (hfi_cmd_write(hfi_ctrl->fd, &cmd, sizeof(cmd)) == -1) {
		if (errno != EINVAL)
			_HFI_ERROR("Failed to set context P_KEY %u\n", p_key);
	}

	/*
	 * extract info from DrvBaseInfo 
	 */

        hfi_drv_base_info = &hfi_ctrl->base_info;
        hfi_drv_ctxt_info = &hfi_ctrl->ctxt_info;

	mymtu = hfi_get_port_vl2mtu(hfi_drv_ctxt_info->unit, 1, stack_ctrl.vl);
	if (mymtu == -1) {
		_HFI_INFO("Failed to get our IB MTU");
		return IPS_RC_DEVICE_INIT_FAILED;
	}

	stack_ctrl.qp = hfi_drv_base_info->bthqp;
        stack_ctrl.mtu = mymtu - HFI_MESSAGE_HDR_SIZE_HFI;
	stack_ctrl.runtime_flags = hfi_drv_ctxt_info->runtime_flags;
	stack_ctrl.j_key = hfi_drv_base_info->jkey;

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


static int _build_frame(int payload_length)
{
	int i;
	uint32_t ctxt = remote_ctxt;

	payload_length += sizeof(hfi_diags_pkt_header);
	payload_length >>= 2;

	send_frame.h.lrh[0] = __cpu_to_be16(vl_lver_sl_lnh);
	send_frame.h.lrh[1] = __cpu_to_be16(remote_lid);
	send_frame.h.lrh[2] = __cpu_to_be16(payload_length + SIZE_OF_CRC);
	send_frame.h.lrh[3] = __cpu_to_be16(my_lid);

	send_frame.h.bth[0] =
	    __cpu_to_be32((HFI_BTH_OPCODE << 24) | p_key);

        send_frame.h.bth[1] = __cpu_to_be32((stack_ctrl.qp << 16) | ctxt);

        /* bth[2] contains the PSN and acknowledgement flag.
           Our context is encoded in the PSN field and used by the responder. */
	send_frame.h.bth[2] = __cpu_to_be32(my_ctxt);

	/* Only kver and jkey need to be set for eager packets. */
	send_frame.h.khdr.kdeth0 = (IPS_PROTO_VERSION << HFI_KHDR_KVER_SHIFT);
        send_frame.h.khdr.job_key = stack_ctrl.j_key;

        send_frame.h.length = __cpu_to_le16(payload_length);

	for (i = 0; i < MAX_PAYLOAD; i++) {
		send_frame.data[i] = i;
        }

	return 0;
}


static void usage(int exval)
{
	printf("%s [-d lvl] (-r [-f]) | (-L LID -C Context [-c #] [-s #] [-p] [-P P_KEY])\n",
	       __progname);
	printf("%s -B (-D dev]\n", __progname);
	printf( "\t-r   : receive (otherwise send)\n"
		"\t-f   : reply with full packet (default is no-payload ack)\n"
		"\t-c # : packet count to send (default is 100000)\n"
		"\t-d # : set debug level\n"
		"\t-e   : error if too many missing acks\n"
		"\t-L # : dest LID\n"
		"\t-C # : dest context \n"
		"\t-p   : pingpong mode (default is streaming)\n"
		"\t-s # : Size of payload (default is 8192)\n"
		"\t-B   : Perform a buffer copy performance test; no packets are sent\n"
		"\t-u   : set poll type to urgent, no ack (receiver only)\n"
		"\t-U unit : Specify HFI device number (default 0 corresponding to /dev/hfi0)\n"
		"\t-P pkey : Partition key to be used in the packet headers (default 0x8001)\n"
		"\t-v   : display version information and exit\n"
		"\t-h   : display this help message\n");
	exit(exval);
}


int main(int argc, char **argv)
{
	char *stmp, *envv;
        int unit_number = 0;
	int urgent = 0;
	int responder = FALSE;
	unsigned payload_length = 0, tlen, setlen = 0;;
	uint16_t streamseq = 0U;
	uint64_t loops = 0;
	int stream_mode = 1, justack = 1, buftest = 0;
	int error_if_no_ack = 0, nfails=0;
	const uint64_t loop_prime = 10; // TODO - should be 100, 10 for simics
	uint64_t loopcnt, rcvd_ok = 0;
	int rc;
	int i;
	uint64_t time_out;
	uint64_t rtt = 0;
	uint64_t *rtt_arr2 = NULL;
	struct timeval tv;
	uint64_t usecs_start=0ULL, usecs_end;
	unsigned long ltmp;

	if (sizeof(hfi_diags_pkt_header) != HFI_MESSAGE_HDR_SIZE) {
		printf
		    ("ERROR: Header size mismatch ! (hfi_pkt) %zi vs. (ips) %i\n",
		     sizeof(hfi_diags_pkt_header), HFI_MESSAGE_HDR_SIZE);

		exit(1);
	}

        hfi_debug = 1;
        //hfi_debug = 0xffffff;

	while ((i = getopt(argc, argv, "+Bc:C:d:efhL:prs:uU:vP:")) != -1)
		switch (i) {
		case 'B':	// PIO buffer copy test; no packets sent
			buftest = 1;
			break;

		case 'c':	// loops - the same
			loops = (int)strtoul(optarg, NULL, 0);
			break;

		case 'C':	// ctxt
			remote_ctxt = (uint16_t) strtoul(optarg, NULL, 0);
			break;

		case 'd':	// debug
			hfi_debug = (unsigned)strtoul(optarg, NULL, 0);
			break;

		case 'e':	// error out if we get too far behind on ACK
			error_if_no_ack = 1;
			break;

		case 'f':  // reply with full packet, otherwise ack only
			justack = 0;
			break;

		case 'L':	// lid
			remote_lid = (uint16_t) strtoul(optarg, NULL, 0);
			break;

		case 'p': // pingpong
			stream_mode = 0;
			break;

		case 'P': //PKEY
			p_key = (uint16_t) strtoul(optarg, NULL, 0);
			break;

		case 'r':	// responder
			responder = TRUE;
			break;

		case 's':	// size of payload
			setlen = 1;
			payload_length = (int)strtoul(optarg, NULL, 0);

			if ((payload_length & ~3) != payload_length) {
				payload_length &= ~3;
				printf("the payload is rounded to %i\n",
				       payload_length);
			}

			break;

		case 'u': /* urgent */
			urgent = 1;
			break;

		case 'U': /* Unit number */
			unit_number = (int)strtoul(optarg, NULL, 0);
			break;

		case 'v':
			printf("%s %s\n", __DIAGTOOLS_GIT_VERSION,
				__DIAGTOOLS_GIT_DATE);
			exit(0);
			break;
		case '?':
		case 'h':
			usage(0);
			break;

		default:
			usage(1);
			break;
		}

	signal(SIGINT, hfi_pkt_signal);
	signal(SIGTERM, hfi_pkt_signal);

	envv = getenv("HFI_VL");
	if(envv && ((ltmp = strtoul(envv, &stmp, 0)) || stmp != envv)) {
		if(ltmp < 8) {
			stack_ctrl.vl = (uint8_t)ltmp;
			vl_lver_sl_lnh |= stack_ctrl.vl << 12;
			_HFI_PDBG("will send with VL %u\n", stack_ctrl.vl);
		}
		else
			_HFI_ERROR("Out of range VL %lu ignored\n", ltmp);
	}

	envv = getenv("HFI_SL");
	if(envv && ((ltmp = strtoul(envv, &stmp, 0)) || stmp != envv)) {
		if(ltmp < 8) {
			stack_ctrl.sl = (uint8_t)ltmp;
			vl_lver_sl_lnh |= stack_ctrl.sl << 4;
			_HFI_PDBG("will send with SL %u\n", stack_ctrl.sl);
		}
		else
			_HFI_ERROR("Out of range SL %lu ignored\n", ltmp);
	}


	if (responder) {
		if (loops || remote_ctxt != -1 || remote_lid || setlen || buftest)
			usage(1);
		if (urgent && justack == 0) {
			_HFI_ERROR("-f is not valid with -u\n");
			usage(1);
		}
		if (hfi_pkt_open(unit_number, &my_lid, &my_ctxt) != 0) {
			_HFI_ERROR("Open failed: %s\n", strerror(errno));
			exit(1);
		}
		if (urgent) {	/* set to urgent, default is ANYRCV */
			hfi_poll_type(hfi_ctrl, HFI1_POLL_TYPE_URGENT);
			justack = -1; /* no response */
		}
		do_responses(justack);
		/* NOTREACHED */
	}


        if(buftest) {
            if(remote_lid || remote_ctxt != -1 || !stream_mode || setlen) {
                _HFI_ERROR("Can't use -L, -C, -p, or -s with -B\n");
                usage(1);
            }

            do_buftest(unit_number, loops);
            /* NOTREACHED */
        }


        /* Sender mode only at this point 
           (responder and buftest don't reach here) */

	if (!remote_lid  || remote_ctxt == -1) {
		_HFI_ERROR("Sender needs both -L and -C options\n");
		usage(1);
	}

	if(!justack) {
		_HFI_ERROR("-f is valid only on responder\n");
		usage(1);
	}

	if (urgent) {
		_HFI_ERROR("-u is valid only on responder\n");
		usage(1);
	}

	if (hfi_pkt_open(unit_number, &my_lid, &my_ctxt) != 0) {
		_HFI_ERROR("Open failed: %s\n", strerror(errno));
		exit(1);
	}


        if(!buftest) {
		if (!loops)
			loops = 100000;
		if (!setlen) {
			payload_length = 8192;
			if(stack_ctrl.mtu < payload_length)
				payload_length = stack_ctrl.mtu;
		}
		else if (payload_length > stack_ctrl.mtu) {
			payload_length = stack_ctrl.mtu;
			printf
				("the payload is truncated to driver IB max (less control): %i\n",
				 payload_length);
		}

		// +4 ICRC, +2 LCRC + 2 SOP/EOP; gives bus efficiency
		// Doesn't account for link flow control packets, though
		tlen = payload_length + sizeof(hfi_diags_pkt_header) + 4 + 2 + 2;
	}

	if(!stream_mode) {
		rtt_arr2 = (uint64_t*)calloc(loops, sizeof(uint64_t));
		if (!rtt_arr2) {
			_HFI_ERROR("no memory available for results: %s\n",
					 strerror(errno));
			exit(1);
		}
	}

	_build_frame(payload_length/*, buftest*/);

        printf("Sending on HFI %d from LID %u ctxt %u to LID %u ctxt %u\n",
                unit_number, my_lid, my_ctxt, remote_lid, remote_ctxt);
        fflush(stdout);

	/* prime the caches, etc., and make sure that we can talk to 
	 * the requested LID.   Always done as ping pong */
	for (loopcnt=0; loopcnt < loop_prime; loopcnt++) {
		unsigned long seconds;
		unsigned got;
		uint16_t seq = 0;
		
		gettimeofday(&tv, NULL);
		seconds = tv.tv_sec;

		seq = __le16_to_cpu(send_frame.h.seq_num) + 1;
		send_frame.h.seq_num = __cpu_to_le16(seq);
		send_frame.h.time_stamp = __cpu_to_le64(get_cycles());

		rc = _transfer_frame(&send_frame, send_frame.data,
						 payload_length);
		if (rc != OK) {
			_HFI_ERROR("Priming loop send failed [%i]\n", rc);
			exit(1);
		}

		got = 0;
		while (!respond(-1, streamseq, &got)) {
			gettimeofday(&tv, NULL);
			if(tv.tv_sec > (seconds + 30)) {
				_HFI_ERROR("No response for 30 seconds; bad "
					"LID/Port or no responder?\n");
				exit(2);
			}

			sched_yield();
		}
		if(got != seq)
			_HFI_PDBG("Expected priming seq %u, but got %u\n",
				    seq, got);
		streamseq = got + 1;
	}

        printf("Done priming, starting benchmark\n");

	if(stream_mode) {
		streamseq = __le16_to_cpu(send_frame.h.seq_num) + 1;
		gettimeofday(&tv, NULL);
		usecs_start = tv.tv_sec * 1000000ULL + tv.tv_usec;
	}

	for (loopcnt=0; loopcnt<loops; loopcnt++) {
		unsigned got;
		uint16_t this_seq = 0;

		this_seq = __le16_to_cpu(send_frame.h.seq_num) + 1;
		send_frame.h.seq_num = __cpu_to_le16(this_seq);

		time_out = get_cycles();
		send_frame.h.time_stamp = __cpu_to_le64(time_out);
		time_out += sec_2_cycles(3);

                rc = _transfer_frame(&send_frame, send_frame.data,
                        payload_length);

		if (rc != OK) {
			_HFI_ERROR("send failed [%i]\n", rc);
			exit(1);
		}

		if(stream_mode) {
			int tries = 1000;
			// don't get too far ahead of receiver, or we'll
			// drop packets, and never complete.  We don't try
			// too hard, though, in case of lost replies.
			// If we get out of seq, try to get back in seq
			while((rcvd_ok+500) < loopcnt && --tries>0) {
				if (respond(-1, streamseq, &got))
					rcvd_ok++;
				/* done this way for better debugging */
				if(got > streamseq) {
					/* try to get back in seq */
					_HFI_PDBG("Expected seq %u, but got "
						    "%u (sent %u)\n",
						    streamseq, got, this_seq);
				}
				streamseq = got + 1;
			}

			if(!tries && error_if_no_ack) {
				printf("Exiting send loop due to missing "
				       "replies (rcvd %llu of %llu)\n",
					(unsigned long long)rcvd_ok,
					(unsigned long long)loopcnt);
				nfails = MAX_FAILS;
				break;
			}
		}
		else { /* ping pong, should always receive what we sent last */
			int res;
			while ((res=respond(-1, this_seq, &got)) == FALSE) {
				if (get_cycles() > time_out) {
					nfails++;
					break; // lost packet?
				}

				sched_yield();
			}
			if (this_seq != got) {
				_HFI_PDBG("Expected pingpong seq %u, got %u\n",
					    this_seq, got);
                        }

			if(res == TRUE) {
				rtt_arr2[loopcnt] = ((time_mark2 - __le64_to_cpu(
					send_frame.h.time_stamp)) * 1000) /
					nanosecs_to_cycles(1000);
                        } else if(nfails == MAX_FAILS) {
				printf("Exiting send loop due to %u missing "
				       "replies\n", nfails);
				break;
			}
		}
	}

	if(stream_mode) {
		if(rcvd_ok != loops && nfails != MAX_FAILS) {
			unsigned long seconds;
			unsigned got;
			uint16_t lastseq = 0;

			lastseq = __le16_to_cpu(send_frame.h.seq_num);
			gettimeofday(&tv, NULL);
			seconds = tv.tv_sec;
			_HFI_PDBG("wait for final responses (%llu OK, %llu "
				"out of sequence), lastgot %u want %u\n",
				(unsigned long long)rcvd_ok,
				(unsigned long long)rcvd_badseq, streamseq,
				lastseq);
			// print the message because we could have dropped
			// packets on one side or the other
			while(streamseq < lastseq) {
				if (respond(-1, streamseq, &got)) {
					rcvd_ok++;
					if(got > streamseq) {
						/* try to get back in seq; expect next to
						 * be 1 more than what we got this time */
						_HFI_PDBG("cleanup expected seq %u, got %u\n",
							    streamseq, got);
					}
					streamseq = got + 1;
					continue;
				}
				gettimeofday(&tv, NULL);
				if(tv.tv_sec > (seconds + 10)) {
					_HFI_ERROR("Didn't get all responses (got %llu/%llu)\n",
						(unsigned long long)(rcvd_ok+rcvd_badseq),
						(unsigned long long)loops);
					break;
				}
			}
			gettimeofday(&tv, NULL);
			if(rcvd_badseq)
				printf("Received %llu packets in sequence, %llu out of sequence\n",
					(unsigned long long)rcvd_ok,
					(unsigned long long)rcvd_badseq);
		}
		else
			gettimeofday(&tv, NULL);
		usecs_end = tv.tv_sec * 1000000ULL + tv.tv_usec;
		rtt =  usecs_end - usecs_start;
	}
	else {
		uint64_t r;
		rtt = 0;
		for (r = 0; r < loopcnt; r++)
			rtt += rtt_arr2[r];
		rtt /= loopcnt;
	}

        printf("%s: %7llu * %4d payload (%4d total): ",
                stream_mode?"stream":"pingpong",
                (unsigned long long)loopcnt, payload_length, tlen);
        if (payload_length)
            printf("payload %3.1f MB/s ",
                    stream_mode ?  (payload_length * loopcnt) / (double)rtt :
                    (payload_length * 1000.) / (double)(rtt >> 1));
        printf("(%3.1f MB/s)\n",
                stream_mode ?  (tlen * loopcnt) / (double)rtt :
                (tlen * 1000.) / (double)(rtt >> 1));

	close(stack_ctrl.file_desc);
	exit(0);
}


#define PKT_SPIN 10000

static void do_responses(int acksonly)
{
	int pkt_tries;
	unsigned seq = 1, nseq;

	printf("Receiving on LID %u context %u (%s)\n", my_lid, my_ctxt,
		acksonly < 0 ? "no response" :
		acksonly  ?  "acks only" : "full packet reply");
	fflush(stdout);

	for(;;) {
		if(respond(acksonly, seq, &nseq)) {
			if (nseq >= seq) /* this is done for debugging, not necessary */
				seq = nseq + 1;
			continue;
		}
		for (pkt_tries = 0; pkt_tries < PKT_SPIN; ++pkt_tries) {
			if(more_rcvpkts())
				break;
		}
		 /* avoid loading system */
		if (pkt_tries >= PKT_SPIN) {
			int ret = hfi_wait_for_packet(hfi_ctrl);
			/*
			 * A return of > 0 means that one or more packets
			 * was received.  If in urgent mode (acksonly < 0),
			 * print that something was received.
			 */
			if (ret > 0 && acksonly < 0)
				printf("packet received during wait\n");
		}
	}
}

static int hfi_simple_userinit(int fd, struct hfi1_user_info_dep *uinfo)
{
	struct hfi1_ctxt_info *cinfo;
	struct hfi1_base_info *binfo;
	void *tmp;
	struct hfi1_cmd c;
	uintptr_t pg_mask;
	int __hfi_pg_sz;
	struct hfi1_user_info uinfo_new;

	/* First get the page size */
	__hfi_pg_sz = sysconf(_SC_PAGESIZE);
	pg_mask = ~(intptr_t) (__hfi_pg_sz - 1);

	cinfo = &hfi_ctrl->ctxt_info;
	binfo = &hfi_ctrl->base_info;

	_HFI_VDBG("uinfo: ver %x, alg %d, subc_cnt %d, subc_id %d\n",
		  uinfo->userversion, uinfo->hfi1_alg,
		  uinfo->subctxt_cnt, uinfo->subctxt_id);

	/* 1. ask driver to assign context to current process */
	memset(&c, 0, sizeof(struct hfi1_cmd));
	c.type = PSMI_HFI_CMD_ASSIGN_CTXT;
	c.len = sizeof(uinfo_new);
	c.addr = (__u64)&uinfo_new;

	uinfo_new.userversion = uinfo->userversion;
	uinfo_new.pad         = uinfo->pad;
	uinfo_new.subctxt_cnt = uinfo->subctxt_cnt;
	uinfo_new.subctxt_id  = uinfo->subctxt_id;
	memcpy(uinfo_new.uuid,uinfo->uuid,sizeof(uinfo_new.uuid));

	if (hfi_cmd_write(fd, &c, sizeof(c)) == -1) {
		_HFI_INFO("assign_context command failed: %s\n",
			  strerror(errno));
		return 1;
	}

	uinfo->userversion = uinfo_new.userversion;
	uinfo->pad         = uinfo_new.pad;
	uinfo->subctxt_cnt = uinfo_new.subctxt_cnt;
	uinfo->subctxt_id  = uinfo_new.subctxt_id;
	memcpy(uinfo->uuid,uinfo_new.uuid,sizeof(uinfo_new.uuid));

	/* 2. get context info from driver */
	c.type = PSMI_HFI_CMD_CTXT_INFO;
	c.len = sizeof(*cinfo);
	c.addr = (__u64) cinfo;

	if (hfi_cmd_write(fd, &c, sizeof(c)) == -1) {
		_HFI_INFO("CTXT_INFO command failed: %s\n", strerror(errno));
		return 1;
	}

	_HFI_VDBG("ctxtinfo: runtime_flags %llx, credits %d\n",
		  cinfo->runtime_flags, cinfo->credits);
	_HFI_VDBG("ctxtinfo: active %d, unit %d, ctxt %d, subctxt %d\n",
		  cinfo->num_active, cinfo->unit, cinfo->ctxt, cinfo->subctxt);
	_HFI_VDBG("ctxtinfo: numa %d, cpu %x, send_ctxt %d\n",
		  cinfo->numa_node, cinfo->rec_cpu, cinfo->send_ctxt);

	/* 3. Get user base info from driver */
	c.type = PSMI_HFI_CMD_USER_INFO;
	c.len = sizeof(*binfo);
	c.addr = (__u64) binfo;

	if (hfi_cmd_write(fd, &c, sizeof(c)) == -1) {
		_HFI_INFO("BASE_INFO command failed: %s\n", strerror(errno));
		return 1;
	}

	_HFI_VDBG("baseinfo: hwver %x, swver %x, jkey %d, qp %d\n",
		  binfo->hw_version, binfo->sw_version,
		  binfo->jkey, binfo->bthqp);
	_HFI_VDBG("baseinfo: credit_addr %llx, sop %llx, pio %llx\n",
		  binfo->sc_credits_addr, binfo->pio_bufbase_sop,
		  binfo->pio_bufbase);

	/* Map the PIO buffer address */
	tmp = hfi_mmap64(0, cinfo->credits * 64,
			 PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd,
			 (__off64_t) binfo->pio_bufbase & pg_mask);
	if (tmp == MAP_FAILED) {
		_HFI_INFO("mmap of pio buffer at %llx failed: %s\n",
			  (unsigned long long)binfo->pio_bufbase,
			  strerror(errno));
		return 1;
	}
	/* Do not try to read the PIO buffers; they are mapped write */
	/* only.  We'll fault them in as we write to them. */
	binfo->pio_bufbase = (uintptr_t) tmp;
	_HFI_VDBG("sendpio_bufbase %llx\n", binfo->pio_bufbase);

	return 0;
}

static int hfi_simple_open(int unit)
{
	struct hfi1_user_info_dep hfi_drv_user_info;
	struct hfi1_base_info *hfi_drv_base_info;
	struct hfi1_ctxt_info *hfi_drv_ctxt_info;
	uid_t *uid = (uid_t *)&hfi_drv_user_info.uuid;

	stack_ctrl.file_desc = hfi_context_open(unit, 0, 5000);
	if (stack_ctrl.file_desc == -1) {
		_HFI_ERROR("hfi_context_open failed. Device = %d, Error = %s\n",
			   unit, strerror(errno));
		return IPS_RC_UNKNOWN_DEVICE;
	}

	memset(&hfi_drv_user_info, 0, sizeof(struct hfi1_user_info_dep));
	*uid = getuid();

	hfi_drv_user_info.userversion = HFI1_USER_SWMINOR |
		(HFI1_USER_SWMAJOR<<16);
	hfi_drv_user_info.hfi1_alg = HFI1_ALG_ACROSS; /*HFI_ALG_WITHIN*/
	hfi_drv_user_info.subctxt_id = 0;
	hfi_drv_user_info.subctxt_cnt = 0;

	if (hfi_simple_userinit(stack_ctrl.file_desc, &hfi_drv_user_info)) {
		hfi_context_close(stack_ctrl.file_desc);
		stack_ctrl.file_desc = -1;
		return IPS_RC_DEVICE_INIT_FAILED;
	}

	/*
	 * extract info from DrvBaseInfo
	 */

	hfi_drv_base_info = &hfi_ctrl->base_info;
	hfi_drv_ctxt_info = &hfi_ctrl->ctxt_info;

	stack_ctrl.spio_total_send_credits = hfi_drv_ctxt_info->credits;
	stack_ctrl.spio_fill_send_credits = 0;
	stack_ctrl.spio_avail_send_credits = hfi_drv_ctxt_info->credits;
	stack_ctrl.spio_cur_slot = 0;

	stack_ctrl.spio_buffer_base =
	    (uint64_t *)(uintptr_t)hfi_drv_base_info->pio_bufbase;
	stack_ctrl.spio_buffer_end =
		(uint64_t *)((uintptr_t)stack_ctrl.spio_buffer_base +
			    (stack_ctrl.spio_total_send_credits * 64));

	return IPS_RC_OK;
}

static void hfi_simple_close(void)
{
	/* Unmap the buffer */
	munmap((void *)hfi_ctrl->base_info.pio_bufbase,
		 hfi_ctrl->ctxt_info.credits * 64);

	/* Close the file */
	hfi_context_close(stack_ctrl.file_desc);
	stack_ctrl.file_desc = -1;
}

static void do_buftest(int unit_number, int loops)
{
    int i;
    int j;
    uint64_t total_quads = 0;
    uint64_t rtt = 0;
    double mbs;
    uint64_t usecs_start, usecs_end;
    struct timeval tv;
    int ret = 0;

    hfi_ctrl = calloc(1, sizeof(struct _hfi_ctrl));
    if (!hfi_ctrl) {
        _HFI_INFO("can't allocate memory for hfi_ctrl: %s\n", strerror(errno));
        exit(1);
    }
    if (loops <= 0)
        loops = 100;

    printf("Buffer Copy test:  "/*, num_quads * 8*/);
    fflush(stdout);

    /*
     * It's important to be reminded that we can't repeatedly write to the
     * same pio buffer send block before the packet is sent out. As a
     * result, we have to repeatedly open and close the file to avoid
     * putting the send context into error state. It is also very important
     * that we release all resources before closing the file to make sure
     * the context be freed.
     */
    for (i = 0; i < loops; i++) {
        if (hfi_simple_open(unit_number) != 0) {
            _HFI_ERROR("%d: Open failed: %s\n", i, strerror(errno));
            ret = 1;
            goto buftest_exit;
        }

        /* Determine how much to copy: skip the first credit block.
           Each credit is 64 bytes, and we write one quadword at a time. */
        int num_quads = ((stack_ctrl.spio_total_send_credits - 1) * 64) / 8;
        total_quads += num_quads;

        /* Set the address to the beginning of the second credit block. */
        volatile uint64_t* addr = (volatile uint64_t*)
            ((uintptr_t)stack_ctrl.spio_buffer_base + 64);

        gettimeofday(&tv, NULL);
        usecs_start = tv.tv_sec * 1000000ULL + tv.tv_usec;

        for(j = 0; j < num_quads; j++) {
            *addr++ = (uint64_t)0;
        }

        gettimeofday(&tv, NULL);
        usecs_end = tv.tv_sec * 1000000ULL + tv.tv_usec;
        rtt +=  usecs_end - usecs_start;

        hfi_simple_close();
    }

    mbs = (total_quads * 8) / (double)rtt;
    if(mbs < 100.) // unusual!  (probably emulation)
        printf("%4.2f MB/s (%ld bytes written)\n",
                mbs, total_quads * 8);
    else
        printf("%4.0f MB/s (%ld bytes written)\n",
                mbs, total_quads * 8);

buftest_exit:
    free(hfi_ctrl);
    exit(ret);
}

