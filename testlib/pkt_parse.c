/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2007, 2008 Qlogic Corporations.  All rights reserved.
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
 * Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
 * Copyright (c) 2007, 2008 Qlogic Corporations.  All rights reserved.
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

/* pkt_parse -- test of packet-field structures */

/*
 * included header files
 */
#include "stdio.h"
#include "unistd.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <linux/stddef.h>

/*
 * Header editting. Supported by two structs, one to contain a single "edit"
 */

struct hdr_edit {
	struct hdr_edit *next_edit;
	uint32_t val_offs;	/* Added to computed/default value */
	uint32_t val_mask;	/* used to read/modify/write */
	uint16_t fld_bit_offs;	/* Offset in bit from start of pkt to edit-point */
	uint16_t fld_bit_len;	/* Length of field to edit. */
};

/*
 * Second header-edit struct defines symbolic names and their positions
 * "lsb" (usually) starts from zero at the lsb of the _last_ octet of the
 * header in question, for compatibility with IB specs. Since IB headers
 * are big-endian-by-octet, this means that the fld_bit_offs must be
 * derived as:
 * fld_bit_offs = (8 * first_octet_of_header)
 *		+ (8 * header_length_in_bits)
 *		- (field lsb from struct hfld)
 * "Key" headers are stored little-endian, so lsb starts from
 * first octet in header. A little-endian header field is
 * indicated below by a negative "length". Mixing big and little
 * endian fields in the same header is allowed by this schem, but
 * nonsense.
 */
struct hfld {
	const char *fldname;
	uint16_t lsb;
	int16_t len;	/* If < 0, littleEndian */
};

struct hfld pbc_flds[] = {
	{"Resv2", 48, -16},
	{"PbcStaticRateControl", 32, -16},
	{"PbcIntr", 31, -1},
	{"PbcDcInfo", 30, -1},
	{"PbcTestEbp", 29, -1},
	{"PbcPacketBypass", 28, -1},
	{"PbcInsertHcrc", 26, -2},
	{"PbcCreditReturn", 25, -1},
	{"PbcInsertBypassIcrc", 24, -1},
	{"PbcTestBadIcrc", 23, -1},
	{"PbcFecn", 22, -1},
	{"Resv1", 16, -6},
	{"PbcVL", 12, -4},
	{"PbcLengthDWs", 0, -12},
	{0, 0, 0}
};

struct hfld lrh_flds[] = {
	{"VL", 60, 4 },
	{"LVer", 56, 4 },
	{"SL", 52, 4 },
	{"Rsvd1", 50, 2},
	{"LNH", 48, 2},
	{"DLID", 32, 16},
	{"Rsvd2", 28, 4},
	{"Len", 16, 12},
	{"SLID", 0, 16},
	{0, 0, 0}
};

struct hfld grh_flds[] = {
	{"IPVer", 316, 4},
	{"TClass", 308, 8},
	{"FlowLabel", 288, 20},
	{"PayLen", 272, 16},
	{"NxtHdr", 264, 8},
	{"HopLimit", 256, 8},
	{"SGID", 128, 128},
	{"DGID", 0, 128},
	{0, 0, 0}
};

struct hfld bth_flds[] = {
	{"OpCode", 88, 8},
	{"SE", 87, 1},
	{"M", 86, 1},
	{"PadCnt", 84, 2},
	{"TVer", 80, 4},
	{"P_Key", 64, 16},
	{"Resv1", 56, 8},
	{"DestQP", 32, 24},
	{"A", 31, 1},
	{"Resv2", 24, 7},
	{"PSN", 0, 24},
        {"PSN31", 0, 31},
	{0, 0, 0}
};

/* hardware required KDETH fields */
struct hfld kdeth_flds[] = {
        {"DONOTUSEME!", 64,   0}, /* This is needed for HCRC to work */
	{"OFFSET",	0, -15},
	{"OM",		15,  -1},
	{"TID",		16,  -10},
	{"TIDCtrl",     26,   -2},
	{"Intr",        28,   -1},
	{"SH",	        29,   -1},
	{"KVer",	30,   -2},
	{"J_KEY",	32, -16},
	{"HCRC",        0,   16}, /* HCRC is big-endian.. wacky */
	{0,		0,    0}
};

/* PSM portion fields */
struct hfld psm_flds[] = {
	{"R",		222,  -1},
	{"AckPSN",	192, -30},
	{"Flags",	186,  -6},
	{"Flags",	186,  -6},
	{"ConnIdx",	160, -26},
	{"OpSpec0",	144, -16},
	{"OpSpec1",	128, -16},
	{"OpSpec2",	 96, -32},
	{"OpSpec3",	 64, -32},
	{"OpSpec4",	 32, -32},
	{"OpSpec5",	  0, -32},
	{0,		  0,   0}
};

/* STL 8b Bypass fields */
struct hfld bypass8_flds[] = {
        {"L2",          61,  -2},
        {"F",           60,  -1},
        {"RC",          57,  -3},
        {"SC",          52,  -5},
        {"DLID",        32, -20},
        {"B",           31,  -1},
        {"L4",          27,  -4},
        {"Length",      20,  -7},
        {"SLID",        0,  -20},
	{0,		0,    0}
};

/* STL 10b Bypass fields */
struct hfld bypass10_flds[] = {
        {"Entropy",     72,  -8},
        {"PKey",        68,  -4},
        {"L4",          64,  -4},
        {"L2",          61,  -2},
        {"F",           60,  -1},
        {"RC",          57,  -3},
        {"SC",          52,  -5},
        {"DLID",        32, -20},
        {"B",           31,  -1},
        {"Length",      20, -11},
        {"SLID",        0,  -20},
	{0,		0,    0}
};

/* STL 16b Bypass fields */
struct hfld bypass16_flds[] = {
        {"R",          120,  -8},
        {"Age",        112,  -8},
        {"Entropy",     96, -16},
        {"PKey",        80, -16},
        {"DLID2",       76,  -4},
        {"SLID2",       72,  -4},
        {"L4",          64,  -8},
        {"L2",          61,  -2},
        {"F",           60,  -1},
        {"RC",          57,  -3},
        {"SC",          52,  -5},
        {"DLID",        32, -20},
        {"B",           31,  -1},
        {"Length",      20, -11},
        {"SLID",        0,  -20},
	{0,		0,    0}
};


struct hfld deth_flds[] = {
        {"Q_key",  32, 32},
        {"Resv1",  24,  8},
        {"Src_QP",  0, 24},
        {0, 0, 0}
};


/* immediate data */
struct hfld immdata_flds[] = {
	{"Data", 0, 32},
	{0, 0, 0}
};

/* base MAD fields */
struct hfld mad_flds[] = {
	{"BaseVersion",	      184,  8},
	{"MgmtClass",	      176,  8},
	{"ClassVersion",      168,  8},
	{"R",	     	      167,  1},
	{"Method",	      160,  7},
	{"Status",	      144, 16},
	{"ClassSpecific",     128, 16},
	{"TransactionID",      64, 64},
	{"AttributeID",	       48, 16},
	{"Reserved",	       32, 16},
	{"AttributeModifier",   0, 32},
	{0, 0, 0}
};

/* Below is variant of header used for hfi1_pkt_test */
/* Follows LRH + BTH + KDETH */
struct hfld test_flds[] = {
        {"length", 208, -16},
        {"seq_num", 192, -16},
        {"user1", 160, -32},
        {"user2", 128, -32},
        {"user3", 96, -32},
        {"user4", 64, -32},
        {"time_stamp", 0, -64},
	{0, 0, 0},
};

/*
 * Locator for symbolic field-edits
 */
struct macro_flds {
	const char *name;
	struct hfld *hdr_flds;
	int (*macro_hook)(struct macro_flds *, char *);
} hdr_macros[] = {
	{"PBC", pbc_flds},
	{"LRH", lrh_flds},
	{"GRH", grh_flds},
	{"BTH", bth_flds},
        {"DETH", deth_flds},
	{"KDETH", kdeth_flds},
	{"PSM", psm_flds},
        {"Bypass8", bypass8_flds},
        {"Bypass10", bypass10_flds},
        {"Bypass16", bypass16_flds},
	{"ImmData", immdata_flds},
	{"TEST", test_flds},
	{"MAD", mad_flds},
	{0, 0, 0}
};

/* return length in octets for header described by specified
 * hfld table. Needed because IB packets are big-endian
 * by octet but descriptions use little-endian bit-numbers.
 */
static int hdrlen(struct hfld *hftbl)
{
	int top_fld, top_len;
	top_fld = top_len = 0;
	while (hftbl->fldname) {
		if (hftbl->lsb > top_fld) {
			top_fld = hftbl->lsb;
			top_len = hftbl->len;
			if (top_len < 0)
				top_len = -hftbl->len;
		}
		++hftbl;
	}
	return ((top_fld + top_len - 1) + 7) >> 3;
}

/*
 * Header fields are "mixed endian", at least of the normal
 * IB case. That is, bits within an octet are numbered from
 * 0 at the LSB (as is conventional for computers designed
 * post-1970), but octets within a word are numbered from
 * 0 at the MSByte.
 */
static uint64_t get_hdr_fld(uint8_t *bp, int hlen, int fld_lsb, int fld_len)
{
	uint64_t accum = 0;
	unsigned int val, lsb;
	int len_remaining;
	int bidx, bincr;

	lsb = fld_lsb;

	if (fld_len < 0) {
		len_remaining = -fld_len;
		/* Point to LSoctet of header */
		bidx = lsb >> 3;
		bincr = 1;
	} else {
		len_remaining = fld_len;
		/* Point to LSoctet of header */
		bidx = hlen - 1 - (lsb >> 3);
		bincr = -1;
	}
	accum = 0;
	while (len_remaining > 0) {
		int len_in_octet = 8 - (lsb & 7);
		if (len_in_octet >  len_remaining)
			len_in_octet = len_remaining;
		/* Get next octet containing field, shift if needed */
		val = bp[bidx] >> (lsb & 7);
		val &= (1 << len_in_octet) - 1;
		accum |= (uint64_t) val << (lsb - fld_lsb);
		len_remaining -= len_in_octet;
		lsb += len_in_octet;
		bidx += bincr;
	}
	return accum;
}

static int put_hdr_fld(uint8_t *bp, int hlen, int fld_lsb, int fld_len, uint64_t val)
{
	unsigned int bval, bmask; /* Byte value, mask */
	unsigned int lsb;
	int len_remaining;
	int bidx, bincr;

	lsb = fld_lsb;

	/* Point to LSoctet of field */
	bidx = lsb >> 3;
	bincr = -1;
	if (fld_len < 0) {
		len_remaining = -fld_len;
		bidx = (lsb >> 3);
		bincr = 1;
	} else {
		len_remaining = fld_len;
		bidx = (hlen - 1) - (lsb >> 3);
		bincr = -1;
	}
	while (len_remaining > 0) {
		int len_in_octet = 8 - (lsb & 7);
		if (len_in_octet >  len_remaining)
			len_in_octet = len_remaining;
		bmask = (1 << len_in_octet) - 1;
		bval = (val & bmask) << (lsb & 7);
		bmask <<= (lsb & 7);
		/* Put next octet containing field */
		bp[bidx] = (bp[bidx] & ~bmask) | bval;
		val >>= len_in_octet;
		len_remaining -= len_in_octet;
		lsb += len_in_octet;
		bidx += bincr;
	}
	return bidx;
}

uint64_t testpkt_get_hdr_fld(uint8_t *buff, const char *hdrname, const char *fldname)
{
	struct hfld *hp;
	int idx, hlen;

	for (idx = 0; ; ++idx) {
		if (hdr_macros[idx].name == 0)
			return -1;
		if (!strcmp(hdr_macros[idx].name, hdrname))
			break;
	}
	hp = hdr_macros[idx].hdr_flds;
	hlen = hdrlen(hp);
	while (hp->fldname) {
		int flen;
		const char *fdelim = strchr(hp->fldname,':');
		if (fdelim)
			flen = fdelim - hp->fldname;
		else
			flen = strlen(fldname);
		if (!strncmp(hp->fldname, fldname, flen))
			break;
		++hp;
	}
	if (!hp->fldname)
		return -2;
	return get_hdr_fld(buff, hlen, hp->lsb, hp->len);
}

int testpkt_put_hdr_fld(uint8_t *buff, const char *hdrname, const char *fldname, uint64_t val)
{
	struct hfld *hp;
	int idx, hlen;

	for (idx = 0; ; ++idx) {
		if (hdr_macros[idx].name == 0)
			return -1;
		if (!strcmp(hdr_macros[idx].name, hdrname))
			break;
	}

	hp = hdr_macros[idx].hdr_flds;
	hlen = hdrlen(hp);
	while (hp->fldname) {
		int flen;
		const char *fdelim = strchr(hp->fldname,':');
		if (fdelim)
			flen = fdelim - hp->fldname;
		else
			flen = strlen(fldname);

		if (flen == strlen(hp->fldname) &&
                                !strncmp(hp->fldname, fldname, flen))
			break;
		++hp;
	}

	if (!hp->fldname)
		return -2;

	if (val >= (1 << abs(hp->len))) {
		printf("WARNING: Field %s is %d bits wide; value 0x%lx will be truncated to 0x%lx\n",
				fldname, abs(hp->len), val,
				val & ((1 << abs(hp->len)) - 1));
	}

	put_hdr_fld(buff, hlen, hp->lsb, hp->len, val);
	return 0;
}

/* First-cut symbolic header dump, given buffer pointer and position,
 * dumps appropriate header. Returns length consumed.
 */
int dump_hdr (FILE *ofp, uint8_t *buff, int posn, const char *htype)
{
	struct hfld *hp;
	uint64_t val;
	int idx, hlen;

	for (idx = 0; ; ++idx) {
		if (hdr_macros[idx].name == 0)
			return -1;
		if (!strcmp(hdr_macros[idx].name, htype))
			break;
	}
	hp = hdr_macros[idx].hdr_flds;
	hlen = hdrlen(hp);
	while (hp->fldname) {
		int len = hp->len;
		val = get_hdr_fld(buff + posn, hlen, hp->lsb, len);
		fprintf(ofp,"%s: %0*"PRIX64"\n",hp->fldname, (len + 3) >> 2, val);
		++hp;
	}
	return hlen;
}
