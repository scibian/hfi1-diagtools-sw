/*
 * Copyright(c) 2015, 2016 Intel Corporation.
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define NO_4KB_BLOCK_ERASE_WA 1

/*
 * Dividing points between the partitions.
 */
#define P0_SIZE (128 * 1024)
#define P1_SIZE (  4 * 1024)
#define P1_START P0_SIZE
#define P2_START (P0_SIZE + P1_SIZE)

/* erase sizes supported by the controller */
#define SIZE_4KB (4 * 1024)
#define MASK_4KB (SIZE_4KB - 1)

#define SIZE_32KB (32 * 1024)
#define MASK_32KB (SIZE_32KB - 1)

#define SIZE_64KB (64 * 1024)
#define MASK_64KB (SIZE_64KB - 1)

#define SIZE_1MB (1024 * 1024)

/* "page" size for the EPROM, in bytes */
#define EP_PAGE_SIZE 256

/* EPROM WP_N line in GPIO signals */
#define EPROM_WP_N (1 << 14)

/*
 * ASIC block register offsets.
 */
#define CORE		    0x000000000000
#define ASIC		    (CORE + 0x000000400000)
#define ASIC_GPIO_OE	    (ASIC + 0x000000000208)
#define ASIC_GPIO_OUT	    (ASIC + 0x000000000218)
#define ASIC_EEP_CTL_STAT   (ASIC + 0x000000000300)
#define ASIC_EEP_ADDR_CMD   (ASIC + 0x000000000308)
#define ASIC_EEP_DATA	    (ASIC + 0x000000000310)
#define MAP_SIZE	    (ASIC + 0x000000000318)
void *reg_mem = NULL;

/*
 * Commands
 */
#define CMD_SHIFT 24
#define CMD_NOP			    (0)
#define CMD_PAGE_PROGRAM(addr)	    ((0x02 << CMD_SHIFT) | addr)
#define CMD_READ_DATA(addr)	    ((0x03 << CMD_SHIFT) | addr)
#define CMD_READ_SR1		    ((0x05 << CMD_SHIFT))
#define CMD_WRITE_ENABLE	    ((0x06 << CMD_SHIFT))
#define CMD_SECTOR_ERASE_4KB(addr)  ((0x20 << CMD_SHIFT) | addr)
#define CMD_SECTOR_ERASE_32KB(addr) ((0x52 << CMD_SHIFT) | addr)
#define CMD_CHIP_ERASE		    ((0x60 << CMD_SHIFT))
#define CMD_READ_JEDEC_ID	    ((0x9f << CMD_SHIFT))
#define CMD_RELEASE_POWERDOWN_NOID  ((0xab << CMD_SHIFT))
#define CMD_SECTOR_ERASE_64KB(addr) ((0xd8 << CMD_SHIFT) | addr)


/*
 * Magic bits appended to the end of an image to allow calculation
 * of an exact image size as required by UEFI Secure Boot.
 * Bits used equate in ASCII to 'OPAimage'
 */
#define IMAGE_MAGIC_LEN 8
#define IMAGE_MAGIC_VAL 0x4f5041696d616765

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
const char * const pci_device_table[] = { "0x24f0", "0x24f1" };
const char pci_eprom_device[] = "0x24f0";

struct hfi_cmd {
	uint32_t type;		/* command type */
	uint32_t len;		/* length of buffer pointed to by addr */
	uint64_t addr;		/* user space address */
};

const char *type_name(int type);

struct file_info {
	const char *name;	/* name for the file */
	size_t fsize;		/* file size, in bytes */
	size_t bsize;		/* buffer size, in bytes */
	void *buffer;		/* memory allocated buffer */
	uint32_t start;		/* starting point in the EPROM */
	int fd;			/* file descriptor for the file */
	int part;		/* partition number */
};

/* operations */
#define DO_NOTHING 0
#define DO_READ    1
#define DO_WRITE   2
#define DO_ERASE   3
#define DO_INFO	   4
#define DO_VERSION 5

/* Status Register 1 bits */
#define SR1_BUSY 0x1	/* the BUSY bit in SR1 */

/* verbose details */
#define WAIT_SLEEP_US 100
#define COUNT_DELAY_SEC(n) ((n) * (1000000/WAIT_SLEEP_US))

/* platform config header structure fields */
#define PLATFORM_CONFIG_HEADER_RECORD_IDX_SHIFT		0
#define PLATFORM_CONFIG_HEADER_RECORD_IDX_LEN_BITS	6
#define PLATFORM_CONFIG_HEADER_TABLE_LENGTH_SHIFT	16
#define PLATFORM_CONFIG_HEADER_TABLE_LENGTH_LEN_BITS	12
#define PLATFORM_CONFIG_HEADER_TABLE_TYPE_SHIFT		28
#define PLATFORM_CONFIG_HEADER_TABLE_TYPE_LEN_BITS	4

/* platform config meta data fields */
#define METADATA_TABLE_FIELD_START_SHIFT		0
#define METADATA_TABLE_FIELD_START_LEN_BITS		15
#define METADATA_TABLE_FIELD_LEN_SHIFT			16
#define METADATA_TABLE_FIELD_LEN_LEN_BITS		16

/* helper for creating a mask from a bit count */
#define mask_of(bits) ((1 << (bits)) - 1)

enum platform_config_table_type_encoding {
	PLATFORM_CONFIG_TABLE_RESERVED,
	PLATFORM_CONFIG_SYSTEM_TABLE,
	PLATFORM_CONFIG_PORT_TABLE,
	PLATFORM_CONFIG_RX_PRESET_TABLE,
	PLATFORM_CONFIG_TX_PRESET_TABLE,
	PLATFORM_CONFIG_QSFP_ATTEN_TABLE,
	PLATFORM_CONFIG_VARIABLE_SETTINGS_TABLE,
	PLATFORM_CONFIG_TABLE_MAX
};

enum platform_config_system_table_fields {
	SYSTEM_TABLE_RESERVED,
	SYSTEM_TABLE_NODE_STRING,
	SYSTEM_TABLE_SYSTEM_IMAGE_GUID,
	SYSTEM_TABLE_NODE_GUID,
	SYSTEM_TABLE_REVISION,
	SYSTEM_TABLE_VENDOR_OUI,
	SYSTEM_TABLE_META_VERSION,
	SYSTEM_TABLE_DEVICE_ID,
	SYSTEM_TABLE_PARTITION_ENFORCEMENT_CAP,
	SYSTEM_TABLE_QSFP_POWER_CLASS_MAX,
	SYSTEM_TABLE_QSFP_ATTENUATION_DEFAULT_12G,
	SYSTEM_TABLE_QSFP_ATTENUATION_DEFAULT_25G,
	SYSTEM_TABLE_VARIABLE_TABLE_ENTRIES_PER_PORT,
	SYSTEM_TABLE_MAX
};

/* platform config file magic number (4 bytes) */
#define PLATFORM_CONFIG_MAGIC_NUM 0x3d4f5041

/* string in front of the verison string for oprom and driver files */
const char version_magic[] = "VersionString:";

/* buffer size for platform config node string */
#define VBUF_MAX 64
#define MAX_DEV_ENTRIES 128
/*
 * PCI bus address if formatted as:
 * domain:bus:slot.function (dddd:bb:ss.f)
 * 13 chars required, rounder up to power of 2
 */
#define MAX_PCI_BUS_LEN 16
#define MAX_DEV_NAME 255

int num_dev_entries = 0;
char pci_device_addrs[MAX_DEV_ENTRIES][MAX_PCI_BUS_LEN];
char resource_file[MAX_DEV_NAME] = "";
char enable_file[MAX_DEV_NAME] = "";
const char pci_device_path[] = "/sys/bus/pci/devices";
const char default_driver_device_name[] = "/dev/hfi1_0";
const char *command;		/* derived command name */
uint32_t dev_id;		/* EEPROM device identification */
uint32_t dev_mbits;		/* device megabit size */
int verbose;

const struct size_info {
	uint32_t dev_id;		/* device */
	uint32_t megabits;		/* size in megabits */
} device_sizes[] = {
	/* JEDEC id, mbits */
	{ 0x001560ef,    16 },          /* Winbond W25Q16D{V,W} */
	{ 0x001660ef,    32 },          /* Winbond W25Q32D{V,W} */
	{ 0x001760ef,    64 },          /* Winbond W25Q64D{V,W} */
	{ 0x001860ef,   128 },          /* Winbond W25Q128FW */
	{ 0x0017BB20,    64 },          /* Micron MT25QU128ABA */
	{ 0x0018BB20,   128 },          /* Micron MT25QU128AB{A}*/
	{ 0x00182001,   128 },          /* Spansion S25FS128 */
};

const char *file_name(int part);

/* ========================================================================== */

uint32_t __attribute__ ((noinline)) read_reg(int fd, uint32_t csr)
{
	volatile uint64_t reg;

	if (csr >= MAP_SIZE ) {
		fprintf(stderr, "Unable to read from %x, out of range: max %x\n", csr, MAP_SIZE);
		exit(1);
	}

	reg = *(uint64_t *)(reg_mem + csr);

	return reg;
}

void __attribute__ ((noinline)) write_reg(int fd, uint32_t csr, volatile uint32_t val)
{
	if (csr >= MAP_SIZE) {
		fprintf(stderr, "Unable to write to %x, out of range: max %x\n", csr, MAP_SIZE);
		exit(1);
	}

	*(uint64_t *)((char *)reg_mem + csr) = val;
}

int is_all_1s(const uint8_t *buffer, int size)
{
	while (size-- > 0)
		if (*buffer++ != 0xff)
			return 0;
	return 1;
}

int is_all_0s(const uint8_t *buffer, int size)
{
	while (size-- > 0)
		if (*buffer++ != 0)
			return 0;
	return 1;
}

/*
 * Search through an image represented by buffer for a series
 * of magic bits, marking the end of that image. If those magic
 * bits are not found, return the full size of the buffer.
 */
uint32_t get_image_size(void *buffer, uint32_t bsize)
{
	uint32_t fsize;
	uint32_t tmp_bsize = 0;
	void *prev_p = NULL;
	void *current_p;
	union {
		uint64_t val;
		uint8_t bytes[IMAGE_MAGIC_LEN];
	} u;

	u.val = htole64(IMAGE_MAGIC_VAL);

	current_p = memmem(buffer, bsize, u.bytes, IMAGE_MAGIC_LEN);
	while (current_p) {
		tmp_bsize = (current_p - buffer);

		prev_p = current_p;
		/* Removing Magic bit string from buffer */
		current_p = current_p + IMAGE_MAGIC_LEN;
		tmp_bsize = tmp_bsize - IMAGE_MAGIC_LEN;
		current_p = memmem(current_p, bsize - tmp_bsize,
				   u.bytes, IMAGE_MAGIC_LEN);
	}

	if (prev_p)
		fsize = prev_p - buffer;
	else
		fsize = bsize;

	return fsize;
}

/*
 * Copy in the magic bits so we can determine the length of the driver
 * on read. Only copy the magic bits if there is actually space to
 * do so. Otherwise, leave the image as-is.
 */
void add_magic_bits_to_image(uint32_t fsize, uint32_t bsize, void *buffer)
{
	/* Make sure we don't try to write past the partition boundary */
	if (fsize + IMAGE_MAGIC_LEN <= bsize) {
		uint64_t magic_val = IMAGE_MAGIC_VAL;
		memcpy(&((uint8_t *)buffer)[fsize], &magic_val, IMAGE_MAGIC_LEN);
	}
}

/* Return the chip size */
uint32_t find_chip_bit_size(uint32_t dev_id)
{
	int i;

	/* No EEPROM chip detected, return 0 size */
	if (dev_id == 0)
		return 0;

	for (i = 0; i < sizeof(device_sizes)/sizeof(device_sizes[0]); i++) {
		if (device_sizes[i].dev_id == dev_id)
			return device_sizes[i].megabits;
	}

	/* No match, assume small 16 Mbit chip*/
	return 16;
}

/* Return JEDEC vendor id */
int find_chip_vendor(uint32_t dev_id)
{
	return dev_id & 0xff;
}

/* wait for the device to become not busy */
void wait_for_not_busy(int fd)
{
	unsigned long count = 0;
	uint32_t reg;

	write_reg(fd, ASIC_EEP_ADDR_CMD, CMD_READ_SR1); /* starts page mode */
	while (1) {
		usleep(WAIT_SLEEP_US);
		count++;
		reg = read_reg(fd, ASIC_EEP_DATA);
		if ((reg & SR1_BUSY) == 0)
			break;
		/* 200s is the largest time for a 128Mb device */
		if (count > COUNT_DELAY_SEC(200)) {
			fprintf(stderr, "Waited too long for busy to clear - failing\n");
			exit(1);
		}
	}
	if (verbose > 3)
		printf("Wait not busy count: %lu (%d us per count)\n", count,
			WAIT_SLEEP_US);
	/* stop page mode with another NOP */
	write_reg(fd, ASIC_EEP_ADDR_CMD, CMD_NOP);
}

void erase_chip(int dev_fd)
{
	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_WRITE_ENABLE);
	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_CHIP_ERASE);
	wait_for_not_busy(dev_fd);
}

#if NO_4KB_BLOCK_ERASE_WA

void do_write(int dev_fd, struct file_info *fi);
void do_read(int dev_fd, struct file_info *fi);
void prepare_file(int op, int partition, const char *fname,
		  struct file_info *fi);

/*
 * Alternative erase range routine that does not use block erase commands.
 * Workaround for chips that cannot erase 4KB pages at arbitrary offset.
 * It reads back whole chip into buffer, clears given range in the buffer,
 * erases whole chip and writes back modified buffer to chip.
 * This may be very slow, i.e. HfiPcieGen3Loader.rom update can take
 * 75 seconds on 128Mbit chip.
 */
void erase_range_slow(int dev_fd, uint32_t start, uint32_t len)
{
	struct file_info tmp_fi;
	prepare_file(DO_VERSION, -1, "chip", &tmp_fi);

	do_read(dev_fd, &tmp_fi);
	if (is_all_1s(tmp_fi.buffer + start, len)) {
		/* partition to erase is empty, we are done */
		return;
	}

	if (verbose)
		printf("Erasing whole chip\n");
	erase_chip(dev_fd);

	memset(tmp_fi.buffer + start, 0xff, len);
	do_write(dev_fd, &tmp_fi);
}
#endif

void erase_range(int dev_fd, uint32_t start, uint32_t len)
{
	uint32_t end = start + len;

#if NO_4KB_BLOCK_ERASE_WA
	if (find_chip_vendor(dev_id) == 0x01) {
		erase_range_slow(dev_fd, start, len);
		return;
	}
#endif
	if (verbose)
		printf("...erasing range 0x%08x-0x%08x\n", start, end);
	if ((start & MASK_4KB) || (end & MASK_4KB)) {
		fprintf(stderr, "Non-algined range (0x%x,0x%x) for a 4KB erase\n",
			start, end);
		exit(1);
	}

	while (start < end) {
		write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_WRITE_ENABLE);
		if (((start & MASK_64KB) == 0 && (start + SIZE_64KB) <= end)) {
			if (verbose > 1)
				printf("Erase 64KB %x\n", start);
			write_reg(dev_fd, ASIC_EEP_ADDR_CMD,
				  CMD_SECTOR_ERASE_64KB(start));
			start += SIZE_64KB;
		} else if (((start & MASK_32KB) == 0
			    && (start + SIZE_32KB) <= end)) {
			if (verbose > 1)
				printf("Erase 32KB %x\n", start);
			write_reg(dev_fd, ASIC_EEP_ADDR_CMD,
				  CMD_SECTOR_ERASE_32KB(start));
			start += SIZE_32KB;
		} else {	/* 4K */
			if (verbose > 1)
				printf("Erase 4KB %x\n", start);
			write_reg(dev_fd, ASIC_EEP_ADDR_CMD,
				  CMD_SECTOR_ERASE_4KB(start));
			start += SIZE_4KB;
		}
		wait_for_not_busy(dev_fd);
	}

	/* the wait_for_not_busy will clear page mode */
}

uint32_t read_device_id(int fd)
{
	/* Read the Manufacture Device ID */
	write_reg(fd, ASIC_EEP_ADDR_CMD, CMD_READ_JEDEC_ID);
	return read_reg(fd, ASIC_EEP_DATA);
}

/* reads a 256 byte (64 dword) page, placing it in result */
void read_page(int dev_fd, uint32_t offset, uint32_t *result)
{
	int i;

	if ((offset % EP_PAGE_SIZE) != 0) {
		fprintf(stderr, "%s: invalid address 0x%x", __func__, offset);
		exit(1);
	}

	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_READ_DATA(offset));
	for (i = 0; i < EP_PAGE_SIZE/sizeof(uint32_t); i++)
		result[i] = read_reg(dev_fd, ASIC_EEP_DATA);
	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_NOP);

	if (verbose > 2)  {
		if (is_all_1s((uint8_t *)result, EP_PAGE_SIZE)) {
			if (verbose > 3)
				printf("%08x: all 1ns\n", offset);
		} else {
			printf("%08x: %08x %08x %08x %08x %08x %08x ...\n",
			       offset, result[0], result[1], result[2],
			       result[3], result[4], result[5]);
		}
	}
}

/* writes a 256 byte (64 dword) page */
void write_page(int dev_fd, uint32_t offset, uint32_t *data)
{
	int i;

	if ((offset % EP_PAGE_SIZE) != 0) {
		fprintf(stderr, "%s: invalid address 0x%x", __func__, offset);
		exit(1);
	}
	/*
	 * No need to write data which is all 1ns - we can only write over
	 * erased (all 1ns) sectors so  they are already all 1ns.
	 * For 128Mbit EEPROMs it can speed up driver partition update by
	 * a factor of 10 (from ~36 to ~3.5 seconds)
	 */
	if (is_all_1s((uint8_t *)data, EP_PAGE_SIZE))
		return;

	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_WRITE_ENABLE);
	write_reg(dev_fd, ASIC_EEP_DATA, data[0]);
	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_PAGE_PROGRAM(offset));
	for (i = 1; i < EP_PAGE_SIZE/sizeof(uint32_t); i++)
		write_reg(dev_fd, ASIC_EEP_DATA, data[i]);
	/* will close the open page */
	wait_for_not_busy(dev_fd);
}

void init_eep_interface(int dev_fd)
{
	/* reset on (a level variable) */
	write_reg(dev_fd, ASIC_EEP_CTL_STAT, 0x4);
	/* reset off and set speed */
	/*    RATE_SPI = 0x2		// 0x2 << 8 */
	write_reg(dev_fd, ASIC_EEP_CTL_STAT, 0x200);
	/* wake the device with command "release powerdown NoID" */
	write_reg(dev_fd, ASIC_EEP_ADDR_CMD, CMD_RELEASE_POWERDOWN_NOID);
}

void write_system_file(struct file_info *fi)
{
	ssize_t nwritten;

	/* write buffer to system */
	nwritten = write(fi->fd, fi->buffer, fi->fsize);
	if (nwritten < 0) {
		fprintf(stderr, "Partition %d file \"%s\" write error: %s\n",
				fi->part, fi->name, strerror(errno));
		exit(1);
	}
	if (nwritten != fi->fsize) {
		fprintf(stderr, "Partition %d file \"%s\" only wrote 0x%lx of 0x%lx bytes\n",
			fi->part, fi->name, nwritten, fi->fsize);
		exit(1);
	}
}

/*
 * Find the given string in the buffer.  The buffer may not be all text.
 * Return a pointer to the next character after the string or NULL if the
 * string is not present.
 */
void *find_string_in_buffer(const uint8_t *buffer, int size, const char *str)
{
	void *p;
	int offset = 0;
	int str_len = strlen(str);

	while (size - offset > 0) {
		/* look for first letter */
		p = memchr(&buffer[offset], str[0], size - offset);
		/* not found */
		if (!p)
			break;
		/* update offset */
		offset = p - (void *)buffer;

		/* if not enough room for string, done */
		if (size - offset < str_len)
			break;

		/* is this the string? */
		if (strncmp(p, str, str_len) == 0)
			return p + str_len; /* found */

		/* move to next character */
		offset++;
	}

	return NULL; /* not found */
}

/*
 * Look through the platform config file in buffer and extract the
 * NODE_STRING, placing it in out_buf.
 * Return 1 on success, 0 on failure.
 */
int parse_platform_config(const void *buffer, int size, char *out_buf,
				int out_size)
{
	const uint32_t *ptr = buffer;
	const uint32_t *end;
	const uint32_t *sys_table_data = NULL;
	const char *vers;
	uint32_t ns_metadata = 0;	/* node string meta data */
	uint32_t mask;
	uint32_t temp;
	uint32_t file_length;
	uint32_t header1, header2;
	uint32_t record_idx, table_length_dwords, table_type;
	int found_metadata = 0;
	int ns_start = 0;
	int ns_len = 0;

	/* read magic */
	temp = *ptr;
	if (temp != PLATFORM_CONFIG_MAGIC_NUM) {
		printf("%s: invalid magic\n", __func__);
		return 0; /* fail */
	}
	ptr++;

	/* read file length */
	file_length = (*ptr) * 4;	/* *4 to convert to bytes */
	if (verbose > 1) {
		printf("%s: file length %d (buffer size %d)\n",
			__func__, file_length, size);
	}
	if (file_length > size) {
		printf("%s: file length %d is larger than buffer %d\n",
			__func__, file_length, size);
		return 0; /* fail */
	}
	ptr++;

	/* this is the valid bounds within the file */
	end = (uint32_t *)(buffer + file_length);

	/* fully walk the file as a sanity check */
	while (ptr < end) {
		if (ptr + 2 > end) {
			printf("%s: not enough room for header\n", __func__);
			return 0; /* fail */
		}
		header1 = *ptr;
		header2 = *(ptr + 1);
		if (header1 != ~header2) {
			printf("%s: header validation failed, "
				"h1 0x%x, h2 0x%x\n",
				__func__, header1, header2);
			return 0; /* fail */
		}
		ptr += 2;

		record_idx = header1 &
			mask_of(PLATFORM_CONFIG_HEADER_RECORD_IDX_LEN_BITS);

		table_length_dwords = (header1 >>
				PLATFORM_CONFIG_HEADER_TABLE_LENGTH_SHIFT) &
		      mask_of(PLATFORM_CONFIG_HEADER_TABLE_LENGTH_LEN_BITS);

		table_type = (header1 >>
				PLATFORM_CONFIG_HEADER_TABLE_TYPE_SHIFT) &
			mask_of(PLATFORM_CONFIG_HEADER_TABLE_TYPE_LEN_BITS);
		if (verbose > 1)
			printf("%s: header: record %d, length %2d, type %d\n",
				__func__, record_idx, table_length_dwords,
				table_type);

		/* NODE_STRING is in the system table, gather data */
		if (table_type == PLATFORM_CONFIG_SYSTEM_TABLE) {
			if (record_idx) { /* the data */
				sys_table_data = ptr;
			} else { /* the metadata */
				ns_metadata = *(ptr + SYSTEM_TABLE_NODE_STRING);
				found_metadata = 1;
			}
		}

		ptr += table_length_dwords;
		ptr++; /* jump the CRC dword */

		/* something is wrong if we are past the end */
		if (ptr > end) {
			printf("%s: position: oops %ld bytes over\n",
				__func__, ptr - end);
			return 0; /* fail */
		}
	}

	/* we have walked through the file and grabbed the parts we want */

	/* make sure the parts were found */
	if (!sys_table_data) {
		printf("%s: system table data not found\n", __func__);
		return 0; /* fail */
	}
	if (!found_metadata) {
		printf("%s: system table metadata not found\n", __func__);
		return 0; /* fail */
	}

	/*
	 * Extract the offset and size from the meta data.  The values
	 * are in bits.
	 */
	mask = mask_of(METADATA_TABLE_FIELD_START_LEN_BITS);
	ns_start = ns_metadata & mask;

	ns_metadata >>= METADATA_TABLE_FIELD_LEN_SHIFT;
	mask = mask_of(METADATA_TABLE_FIELD_LEN_LEN_BITS);
	ns_len = ns_metadata & mask;

	ns_start /= 8;	/* change to bytes */
	ns_len /= 8;	/* change to bytes */

	if (verbose > 1)
		printf("%s: node string start %d, len %d\n", __func__,
			ns_start, ns_len);

	/* make sure we have a large enough buffer */
	if (ns_len > out_size) {
		printf("%s: node string len %d too long, truncating\n",
			__func__, ns_len);
		ns_len = out_size;
	}

	/* copy into alreay-zeroed buffer, with an extra nul */
	vers = ((const char *)sys_table_data) + ns_start;
	memcpy(out_buf, vers, ns_len);

	return 1; /* success */
}

/* called when finding the version of the data */
void print_data_version(struct file_info *fi)
{
	uint8_t *buf;
	uint8_t *vers;
	char vers_buf[VBUF_MAX+1]; /* +1 for terminator */
	int found = 0;
	int size;
	int offset;
	int i;
	char c;

	buf = fi->buffer;
	size = fi->bsize;
	memset(vers_buf, 0, sizeof(vers_buf));

	if (is_all_1s(buf, size)) {
		printf("%s is erased (all 1s)\n", file_name(fi->part));
		return;
	}
	if (is_all_0s(buf, size)) {
		printf("%s is zeroed\n", file_name(fi->part));
		return;
	}

	if (fi->part == 0 || fi->part == 2) {
		/* UEFI drivers */
		/* look for version magic in the data */
		vers = find_string_in_buffer(buf, size, version_magic);
		if (vers) {
			found = 1;
			/*
			 * Expect the version string to immediately
			 * follow the version magic in a C string.
			 * Copy it out, but keep checking for buffer
			 * end to be on the safe side.
			 */
			offset = (int)(vers - buf);
			for (i = 0; i < VBUF_MAX && i+offset < size; i++) {
				c = buf[i+offset];
				if (c == 0)
					break;
				vers_buf[i] = c;
			}
		}
	} else if (fi->part == 1) {
		/* platform config file */
		found = parse_platform_config(buf, size, vers_buf, VBUF_MAX);
	} else {
		return;
	}

	if (found)
		printf("%s version: %s\n", file_name(fi->part), vers_buf);
	else
		printf("%s version: not found\n", file_name(fi->part));
}

#define PRINT_SIZE (64 * 1024)
void print_of(const char *what, uint32_t offset, uint32_t total_k)
{
	/* show something every PRINT_SIZE bytes */
	if (verbose > 1 && ((offset % PRINT_SIZE) == 0)) {
		printf("...%s %dK of %dK\n", what, offset/1024, total_k);
		fflush(stdout);
	}
}

/* read from the EPROM into the buffer, then write the buffer to the file */
void do_read(int dev_fd, struct file_info *fi)
{
	uint32_t offset;
	uint32_t total_k;

	if (verbose)
		printf("Reading %s\n", file_name(fi->part));
	total_k = fi->bsize / 1024;

	/* read into our buffer */
	for (offset = 0; offset < fi->bsize; offset += EP_PAGE_SIZE) {
		read_page(dev_fd, fi->start + offset,
					(uint32_t *)(fi->buffer + offset));
		print_of("read", offset + EP_PAGE_SIZE, total_k);
	}

	fi->fsize = get_image_size(fi->buffer, fi->bsize);

	if (fi->fd >= 0)
		write_system_file(fi);
	else
		print_data_version(fi);
}

void write_enable(int dev_fd)
{
	/* raise signal */
	write_reg(dev_fd, ASIC_GPIO_OUT, EPROM_WP_N);
	/* raise enable */
	write_reg(dev_fd, ASIC_GPIO_OE, EPROM_WP_N);
}

void write_disable(int dev_fd)
{
	/* assumes output already enabled */
	write_reg(dev_fd, ASIC_GPIO_OUT, 0);	/* lowers signal */
	write_reg(dev_fd, ASIC_GPIO_OE, 0);	/* remove enable */
}

void do_erase(int dev_fd, struct file_info *fi)
{
	write_enable(dev_fd);
	if (fi->part < 0) {
		if (verbose)
			printf("Erasing whole chip\n");
		erase_chip(dev_fd);
	} else {
		if (verbose)
			printf("Erasing %s\n", file_name(fi->part));
		erase_range(dev_fd, fi->start, fi->bsize);
	}
	write_disable(dev_fd);
}

/* write to the EPROM, from the file, using the buffer as a stage */
void do_write(int dev_fd, struct file_info *fi)
{
	uint32_t offset;
	uint32_t total_k;

	if (verbose)
		printf("Writing %s at 0x%x len %lx\n",
		       fi->name, fi->start, fi->bsize);
	total_k = fi->bsize / 1024;

	/* data is already read from the file into the buffer */

	write_enable(dev_fd);

	/* write from buffer into EPROM */
	for (offset = 0; offset < fi->bsize; offset += EP_PAGE_SIZE) {
		write_page(dev_fd, fi->start + offset,
					(uint32_t *)(fi->buffer + offset));
		print_of("wrote", offset + EP_PAGE_SIZE, total_k);
	}

	write_disable(dev_fd);
}

uint32_t do_info(int dev_fd)
{
	return read_device_id(dev_fd);
}

void enable_device()
{
	FILE *fp;
	char boolean_value = '1';

	fp = fopen(enable_file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Unable to open device enable file as writable: %s\n",
			enable_file);
		exit(1);
	}
	fwrite(&boolean_value, sizeof(boolean_value), sizeof(boolean_value), fp);
	fclose(fp);
}

void set_pci_files(int entry)
{
	snprintf(resource_file, sizeof(resource_file),
		"%s/%s/resource0", pci_device_path, pci_device_addrs[entry]);
	snprintf(enable_file, sizeof(enable_file),
		"%s/%s/enable", pci_device_path, pci_device_addrs[entry]);
}

int do_init(const char *dev_name)
{
	int i;
	int dev_fd;

	if (!dev_name) {
		set_pci_files(0);
		dev_name = resource_file;
		if (verbose)
			printf("Using default device: %s\n", dev_name);
	} else {
		for (i = 0; i < num_dev_entries; i++) {
			set_pci_files(i);
			if (!strcmp(dev_name, resource_file)) {
				if (verbose)
					printf("Using specified device: %s\n", dev_name);
				break;
			}
		}
		if (i == num_dev_entries) {
			fprintf(stderr, "Incorrect device specified, "
				"HFI discrete device is required\n"
				"Specify one from the list below:\n");
			for (i = 0; i < num_dev_entries; i++) {
				fprintf(stderr, "%s/%s/resource0\n",
					pci_device_path, pci_device_addrs[i]);
			}
			exit(1);
		}
	}

	dev_fd = open(dev_name, O_RDWR);
	if (dev_fd < 0) {
		fprintf(stderr, "Unable to open file [%s]\n", dev_name);
		exit(1);
	}

	reg_mem = mmap(NULL, MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED,
		       dev_fd, 0);

	if (reg_mem == MAP_FAILED) {
		fprintf(stderr, "Unable to mmap %s, %s\n", dev_name,
			strerror(errno));
		exit(1);
	}

	init_eep_interface(dev_fd);

	return dev_fd;
}

/* ========================================================================== */

/* return the file name */
const char *file_name(int part)
{
	if (part < 0)
		return "whole chip";

	switch (part) {
	case 0: return "loader file";
	case 1: return "config file";
	case 2: return "driver file";
	}
	return "unknown";
}

/*
 * Initialize the file info fields.
 */
void prepare_file(int op, int partition, const char *fname,
		  struct file_info *fi)
{
	ssize_t nread;
	int flags;
	mode_t mode;

	fi->name = fname;
	fi->part = partition;
	fi->fd = -1;
	fi->buffer = NULL;

	/* start and buffer (partition) size */
	if (fi->part < 0) {
		fi->start = 0;
		fi->bsize = (dev_mbits * ((1024*1024) / 8));
	} else if (fi->part == 0) {
		fi->start = 0;
		fi->bsize = P0_SIZE;
	} else if (fi->part == 1) {
		fi->start = P1_START;
		fi->bsize = P1_SIZE;
	} else if (fi->part == 2) {
		fi->start = P2_START;
		if (dev_mbits == 0)
			fi->bsize = 0;
		else
			fi->bsize = (dev_mbits * ((1024*1024) / 8)) - P2_START;
	}

	if (op == DO_ERASE || op == DO_INFO) {
		/* no buffer or file for erase and info */
		return;
	}

	if (op == DO_READ || op == DO_WRITE) {
		/* open our file system file */
		if (op == DO_READ) {
			/* read from EPROM, write to file */
			flags = O_WRONLY | O_CREAT | O_TRUNC;
			mode = S_IRWXU | S_IRGRP | S_IROTH;
		} else {
			/* write to EPROM, read from file */
			flags = O_RDONLY;
			mode = 0;
		}

		fi->fd = open(fi->name, flags, mode);
		if (fi->fd < 0) {
			fprintf(stderr, "Cannot open file \"%s\": %s\n",
				fi->name, strerror(errno));
			exit(1);
		}
	} /* else op == DO_VERSION - fall through to create a buffer */

	/*
	 * Allocate a read/write buffer.  Always make this the partition size
	 * to avoid complicating the read and write routines.  When writing
	 * the parition, always write the whole thing even though the actual
	 * file will be smaller.  When reading the partition, always read the
	 * whole thing because the actual working size is unkown.
	 */
	fi->buffer = malloc(fi->bsize);
	if (!fi->buffer) {
		fprintf(stderr, "Unable to allocate 0x%lx sized buffer\n",
			fi->bsize);
		exit(1);
	}

	/*
	 * Always init buffer to 0xff (not 0x00)!
	 * 1ns represent erased data in EPPROM memory.
	 * The write_page() function take advantage of the fact that
	 * most of the driver buffer is all 1ns.
	 */
	memset(fi->buffer, 0xff, fi->bsize);

	/* if writing, read from file into buffer */
	if (op == DO_WRITE) {
		struct stat sbuf;

		if (fstat(fi->fd, &sbuf) == -1) {
			fprintf(stderr, "Unable to stat \"%s\": %s\n",
				fi->name, strerror(errno));
			exit(1);
		}

		/* the input file cannot be larger than the parition */
		if (sbuf.st_size > fi->bsize) {
			fprintf(stderr, "File size 0x%lx is larger than partition size 0x%lx\n",
				sbuf.st_size, fi->bsize);
			exit(1);
		}

		/* read from the file into the buffer */
		fi->fsize = sbuf.st_size;
		nread = read(fi->fd, fi->buffer, sbuf.st_size);
		if (nread < 0) {
			fprintf(stderr, "Read fail for \"%s\": %s\n", fi->name,
				strerror(errno));
			exit(1);
		}
		if (nread != sbuf.st_size) {
			fprintf(stderr, "Read only 0x%lx of expectd 0x%lx bytes for \"%s\"\n",
				nread, sbuf.st_size, fi->name);
			exit(1);
		}

		add_magic_bits_to_image(sbuf.st_size, fi->bsize, fi->buffer);
	}
}

void do_operation(int operation, int dev_fd, int partition, char *fname)
{
	struct file_info fi;

	/* open file if needed, get sizes */
	prepare_file(operation, partition, fname, &fi);
	if (verbose) {
		printf("Operation %d\n", operation);
		printf("File information:\n");
		printf("  part   %d\n", fi.part);
		printf("  fname   %s\n", fi.name);
		printf("  fsize  0x%lx\n", fi.fsize);
		printf("  start  0x%x\n", fi.start);
		printf("  bsize  0x%lx\n", fi.bsize);
		printf("  buffer %p\n", fi.buffer);
		printf("  fd     %d\n", fi.fd);
	}

	if (operation == DO_ERASE) {
		do_erase(dev_fd, &fi);
	} else if (operation == DO_WRITE) {
		do_erase(dev_fd, &fi);
		do_write(dev_fd, &fi);
	} else if (operation == DO_READ || operation == DO_VERSION) {
		do_read(dev_fd, &fi);
	}

	if (fi.fd >= 0)
		close(fi.fd);
	if (fi.buffer)
		free(fi.buffer);
}

/*
 *  enumerate_devices() finds HFI discrete devices and stores pci bus
 *  addresses in gobal array: pci_device_addrs
 */
void enumerate_devices(void)
{
	DIR *dir = opendir(pci_device_path);
	struct dirent *dentry;

	if (!dir) {
		fprintf(stderr, "Unable to open %s\n", pci_device_path);
		exit(1);
	}

	/* Search through the directory looking for device files */
	num_dev_entries = 0;
	while ((dentry = readdir(dir))) {
		FILE *file;
		char dev_file[MAX_DEV_NAME];
		char buf[7];
		char *buf_ptr;

		snprintf(dev_file, sizeof(dev_file), "%s/%s/device", pci_device_path, dentry->d_name);

		/* try to open the file, it may error, ignore */
		file = fopen(dev_file, "r");
		if (!file)
			continue;

		memset(buf, '\0', sizeof(buf));
		buf_ptr = fgets(buf, sizeof(buf), file);
		fclose(file);
		if (!buf_ptr)
			continue;

		if (!strncmp(buf, pci_eprom_device, 6)) {
			if (strlen(dentry->d_name) > MAX_PCI_BUS_LEN - 1) {
				fprintf(stderr, "Device pci address is too long: %s\n", dentry->d_name);
				exit(1);
			}
			if (num_dev_entries == MAX_DEV_ENTRIES) {
				fprintf(stderr, "Too many HFI discrete devices found in the system\n");
				exit(1);
			}
			strcpy(pci_device_addrs[num_dev_entries], dentry->d_name);
			num_dev_entries++;
                }
	}

	if (num_dev_entries > 0)
		set_pci_files(0);
	closedir(dir);
}

void usage(void)
{
	printf(
"\n"
"usage: %s -w [-o loaderfile][-c configfile][-b driverfile][allfile]\n"
"       %s -r [-o loaderfile][-c configfile][-b driverfile][allfile]\n"
"       %s -e [-o][-c][-b]\n"
"       %s -V [-o][-c][-b]\n"
"       %s -i\n"
"       %s -h\n"
"\n"
"Write, read, erase, or gather information on the Intel Omni-Path Architecture\n"
"fabric EPROM.  Only one operation may be performed per invocation.  If no\n"
"operation is given, info is used.  Read, write, and erase may be performed\n"
"on a specific file, or the whole device.\n"
"\n"
"Options:\n"
"  -b driverfile use the EFI driver file (.efi)\n"
"  -c configfile use the platform configuration file\n"
"  -d device     specify the device file to use\n"
"                  [%s]\n"
"  -e            erase the given file or all if no file options given\n"
"  -h            print help\n"
"  -i            print the EPROM device ID [default]\n"
"  -o loaderfile use the driver loader (option rom) file (.rom)\n"
"  -r            read the given file(s) from the EPROM\n"
"  -s size       override EPROM size, must be power of 2, in Mbits\n"
"  -v            be more verbose\n"
"  -V            print the version of the file written in the EPROM\n"
"  -w            write the given file(s) to the EPROM\n"
"  allfile       name of file to use for reading or writing the whole device\n"
"\n",
		command, command, command, command, command, command,
		resource_file);
}

void only_one_operation(void)
{
	fprintf(stderr, "Only one operation may be performed at a time\n");
	usage();
	exit(1);
}

int main(int argc, char **argv)
{
	const char *dev_name = NULL;
	uint32_t override_mbits = 0;
	int operation = DO_NOTHING;
	char *end;
	int dev_fd;
	int opt;
	int do_oprom_part = 0;
	int do_config_part = 0;
	int do_bulk_part = 0;
	char *oprom_name = NULL;
	char *config_name = NULL;
	char *bulk_name = NULL;
	char *all_name = NULL;

	/* isolate the command name */
	command = strrchr(argv[0], '/');
	if (command)
		command++;
	else
		command = argv[0];

	enumerate_devices();

	/*
	 * Use ':' at the start of getopt's string return a ':' for options
	 * that require an argument but don't have one.  Erase and
	 * version expect individual options to not have an argument.
	 */
	while ((opt = getopt(argc, argv, ":b:c:d:ehio:rs:vVw")) != -1) {
		switch (opt) {
		case ':':
			switch (optopt) {
			case 'b':
				do_bulk_part = 1;
				break;
			case 'c':
				do_config_part = 1;
				break;
			case 'o':
				do_oprom_part = 1;
				break;
			default:
				fprintf(stderr, "An argument is required"
					" for option '%c'\n", optopt);
				usage();
				exit(1);
			}
			break;
		case 'b':
			do_bulk_part = 1;
			bulk_name = optarg;
			break;
		case 'c':
			do_config_part = 1;
			config_name = optarg;
			break;
		case 'd':
			dev_name = optarg;
			break;
		case 'e':
			if (operation != DO_NOTHING)
				only_one_operation();
			operation = DO_ERASE;
			break;
		case 'h':
			usage();
			exit(0);
		case 'i':
			if (operation != DO_NOTHING)
				only_one_operation();
			operation = DO_INFO;
			break;
		case 'o':
			do_oprom_part = 1;
			oprom_name = optarg;
			break;
		case 'r':
			if (operation != DO_NOTHING)
				only_one_operation();
			operation = DO_READ;
			break;
		case 's':
			override_mbits = strtoul(optarg, &end, 0);
			/*
			 * Reject if:
			 *   - not all bytes converted
			 *   - zero value
			 *   - must be power of 2
			 */
			if (*end != 0 || override_mbits == 0 ||
				(override_mbits & (override_mbits - 1)) != 0) {

				fprintf(stderr, "Invalid size \"%s\"\n",
					optarg);
				usage();
				exit(1);
			}
			break;
		case 'w':
			if (operation != DO_NOTHING)
				only_one_operation();
			operation = DO_WRITE;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			if (operation != DO_NOTHING)
				only_one_operation();
			operation = DO_VERSION;
			break;
		case '?':
			usage();
			exit(1);
			break;
		}
	}

	/* first argument is our whole-chip name */
	if (optind < argc)
		all_name = argv[optind];

	/* if no operation is specified, just gather information */
	if (operation == DO_NOTHING)
		operation = DO_INFO;

	if ((operation == DO_ERASE || operation == DO_VERSION) && (all_name ||
					((do_oprom_part && oprom_name)
					|| (do_config_part && config_name)
					|| (do_bulk_part && bulk_name)))) {
		fprintf(stderr, "Erase and version takes no file names\n");
		usage();
		exit(1);
	}

	if ((operation == DO_READ || operation == DO_WRITE) && !all_name
			&& !(do_oprom_part || do_config_part || do_bulk_part)) {
		fprintf(stderr, "Read or write requires a file name"
			" or individual file names\n");
		usage();
		exit(1);
	}

	if ((operation == DO_READ || operation == DO_WRITE) && all_name
			&& (do_oprom_part || do_config_part || do_bulk_part)) {
		fprintf(stderr, "Read or write requires a file name"
			" or individual file names, but not both\n");
		usage();
		exit(1);
	}

	if ((operation == DO_READ || operation == DO_WRITE) && (
					(do_oprom_part && !oprom_name)
					|| (do_config_part && !config_name)
					|| (do_bulk_part && !bulk_name))) {
		fprintf(stderr, "Read or write requires a named file\n");
		usage();
		exit(1);
	}

	if (operation == DO_INFO && (all_name || do_oprom_part
					|| do_config_part || do_bulk_part)) {
		fprintf(stderr, "Info takes no file arguments\n");
		usage();
		exit(1);
	}

	if (num_dev_entries == 0) {
		fprintf(stderr, "Unable to find an HFI discrete device\n");
		exit(1);
	}

	/*
	 * If the driver has never been loaded, the device is not
	 * enabled. Enable the device every time.
	 */
	enable_device();

	/* do steps for all operations */
	dev_fd = do_init(dev_name);

	/* find the size */
	dev_id = do_info(dev_fd);
	if (dev_id == 0xFFFFFFFF) {
		fprintf(stderr, "Unable to read device id.\n");
		exit(1);
	}

	dev_mbits = find_chip_bit_size(dev_id);
	if (dev_mbits == 0)
		printf("Device ID: 0x%08x, unrecognized - no size\n", dev_id);
	else
		if (operation == DO_INFO || verbose)
			printf("Device ID: 0x%08x, %d Mbits\n", dev_id,
				dev_mbits);
	if (override_mbits) {
		if (verbose)
			printf("Using override size of %d Mbits\n",
				override_mbits);
		dev_mbits = override_mbits;
	}

	/* if only gathering information, we're done */
	if (operation == DO_INFO)
		return 0;

	/*
	 * Fail if we don't have the device size and the operation
	 * requires knowing the full chip size.
	 */
	if (dev_mbits == 0 && (operation == DO_READ || operation == DO_WRITE)
						&& (all_name || bulk_name)) {
		fprintf(stderr, "Cannot operate on device without the device size\n");
		exit(1);
	}

	/* do the operation(s) */
	if (all_name || (operation == DO_ERASE
		    && !(do_oprom_part || do_config_part || do_bulk_part))) {
		/* doing a whole-chip operation */
		do_operation(operation, dev_fd, -1, all_name);
	} else {
		/* doing an individual partition operation */
		if (do_oprom_part)
			do_operation(operation, dev_fd, 0, oprom_name);
		if (do_config_part)
			do_operation(operation, dev_fd, 1, config_name);
		if (do_bulk_part)
			do_operation(operation, dev_fd, 2, bulk_name);
	}

	if (reg_mem)
		munmap(reg_mem, MAP_SIZE);

	close(dev_fd);

	return 0;
}
