/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (C) 2006, 2008 QLogic Corporation, All rights reserved.
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
 * Copyright(c) 2015 Intel Corporation.
 * Copyright (C) 2006, 2008 QLogic Corporation, All rights reserved.
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

/* This program gets and prints the HFI chip cntrs and driver stats */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <signal.h>
#include <dirent.h>
#include <ctype.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define U32_MAX (~(uint32_t)0)

struct timeval udelta;
static char *deltafmts = "+%lu seconds:\n";
static char *deltafmtms = "+%lu.%03lu seconds:\n";
char *deltafmt; /* set to one of above, if -c */
int drstatfd, *devcntrfd, **portcntrfd;
char **drstatnames, ***cntrnames, ***portcntrnames;
int verbose, numcols = 1, nozero, autosize = 1;
int check_complained;
char *mntpnt = "/sys/kernel/debug/hfi1";
int stat1sterr, *dev1sterr, *port1sterr, hcaset, portset;
unsigned hca, hcaport;
int maxstrlen;
char **excludes; /* list of stats/counters to exclude */
int json_format = 0;

/* these are global so we can init and cleanup in functions */
int nunits, *nports, nstats, *cntrsz, *portcntrsz;
int changes, errors_only, sdma, dc, cce, rcv, tx, filter, xmit;
int misc, pio, pcic, send;
uint64_t *stats, *last_stats;
uint64_t **cntrs, **last_cntrs, ***portcntrs, ***last_portcntrs;


/* divide by 1000, append K ; used for fields that quickly get large */
#define MAX_DIGITS_BEFORE_DIVIDE 999999999

const char colfmt[] = "%*s %13llu";
const char colfmtK[] = "%*s %12lluK";
const char colfmtd[] = "%*s %13lld"; /* stats deltas can be negative */
const char colfmtdK[] = "%*s %12lldK"; /* stats deltas can be negative */

const char json_colfmt[]   = "\t\t\t\"%*s\":\t%13llu";
const char json_colfmtK[]  = "\t\t\t\"%*s\":\t%12llu000";
const char json_colfmtd[]  = "\t\t\t\"%*s\":\t%13lld"; /* stats deltas can be negative */
const char json_colfmtdK[] = "\t\t\t\"%*s\":\t%12lld000"; /* stats deltas can be negative */

typedef unsigned long long ull;

// I *really* want to use strlcat, but it's not part
// of the linux distributions for some bizarre reason,
// and I don't want to directly incorporate GPL code
// into this source.   ARGHGHGHGHGHGHGHG

#define strlcat(d,s,n) strncat((d),(s),(n)-1)

/*
 * IB stats interface provides a debugfs alternative for
 * non-root users
 */

#define IBST_ROOT	"/sys/class/infiniband"
#define IBST_MAX_UNITS	4
#define IBST_MAX_PORTS	4
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

static char *ibst_cntr_root[IBST_MAX_UNITS][IBST_MAX_PORTS + 1];
static int ibst_num_ports[IBST_MAX_UNITS];
static int ibst_num_units;
static int use_ibst;

int ibst_init(void)
{
	struct stat st;
	char path[80];
	int unit, port;

	for (unit = 0; unit < IBST_MAX_UNITS; unit++) {
		snprintf(path, ARRAY_SIZE(path), "%s/hfi1_%d/hw_counters",
			 IBST_ROOT, unit);
		if (stat(path, &st) || !S_ISDIR(st.st_mode))
			break;
		ibst_cntr_root[unit][0] = strdup(path);
		ibst_num_units++;

		for (port = 1; port <= IBST_MAX_PORTS; port++) {
			snprintf(path, ARRAY_SIZE(path),
				 "%s/hfi1_%d/ports/%d/hw_counters",
				 IBST_ROOT, unit, port);
			if (stat(path, &st) || !S_ISDIR(st.st_mode))
				break;
			ibst_cntr_root[unit][port] = strdup(path);
			ibst_num_ports[unit]++;
		}
	}

	return ibst_num_units ? 0 : -ENODEV;
}

void ibst_exit(void)
{
	int unit, port;

	for (unit = 0; unit < ibst_num_units; unit++) {
		for (port = 0; port <= ibst_num_ports[unit]; port++)
			free(ibst_cntr_root[unit][port]);
		ibst_num_ports[unit] = 0;
	}
	ibst_num_units = 0;
}

/*
 * A counter name is in the form '<base><index>[,32]' where <index>
 * is an integer and the optional ',32' suffix indicates a 32-bit
 * counter. Find the length of <base> and get the value of <index>.
 */
int get_base_cntr_name(const char *s, int *val)
{
	int n;
	int has_number = 0;
	char *p = strstr(s, ",32");

	if (p)
		n = p - s;
	else
		n = strlen(s);

	while (n > 0 && isdigit(s[n - 1])) {
		n--;
		has_number = 1;
	}

	if (has_number)
		*val = atoi(s + n);
	else
		*val = -1;

	return n;
}

int cmp_cntr_names(const void *p1, const void *p2)
{
	const char *s1 = *(char **)p1;
	const char *s2 = *(char **)p2;
	int n1, n2, val1, val2;
	int ret;

	n1 = get_base_cntr_name(s1, &val1);
	n2 = get_base_cntr_name(s2, &val2);

	if (n1 < n2) {
		ret = strncmp(s1, s2, n1);
		return ret ? ret : -1;
	}

	if (n1 > n2) {
		ret = strncmp(s1, s2, n2);
		return ret ? ret : 1;
	}

	ret = strncmp(s1, s2, n1);
	if (ret)
		return ret;

	return  (val1 < val2) ? -1 : (val1 > val2);
}

/*
 * Get a sorted list of counter names under a specific directory, with
 * optional prefix filtering.
 *
 * @cntr_root:	the directory where the counter files are located.
 * @prefix:	a prefix that the counter names must match
 * 		(or must not match if it begins with '!').
 * @cntr_names:	pointer to hold the returned array of counter names.
 *
 * Return the number of counters.
 */
int ibst_get_cntr_names(char *cntr_root, char *prefix, char ***cntr_names)
{
	DIR *dir;
	struct dirent *ent;
	int prefix_len = 0, skip_len = 0;
	int exclusive = 0, match;
	char **names = NULL;
	int i, n;

	if (prefix) {
		prefix_len = strlen(prefix);
		if (prefix[0] == '!') {
			prefix++;
			prefix_len--;
			exclusive = 1;
		} else {
			skip_len = prefix_len;
			exclusive = 0;
		}
	}

	dir = opendir(cntr_root);
	if (!dir)
		return 0;

	/*
	 * Walk over the directory twice: count the entries during the first
	 * pass and fill in the name array during the second pass.
	 */
	for (i = 0; i < 2; i++) {
		n = 0;
		while ((ent = readdir(dir))) {
			if (ent->d_name[0] == '.')
				continue;
			if (strcmp(ent->d_name, "lifespan") == 0)
				continue;
			if (prefix) {
				match = !strncmp(prefix, ent->d_name,
						 prefix_len);
				if (exclusive)
					match = !match;
				if (!match)
					continue;
			}

			if (names)
				names[n] = strdup(ent->d_name + skip_len);
			n++;
		}

		if (!n)
			break;

		if (!names) {
			names = malloc(n * sizeof(char *));
			if (!names) {
				fprintf(stderr, "No memory to save %u counter names: %s\n",
					n, strerror(errno));
				n = 0;
				break;
			}
			rewinddir(dir);
		}
	}

	closedir(dir);

	if (n)
		qsort(names, n, sizeof(char *), cmp_cntr_names);

	*cntr_names = names;
	return n;
}

int ibst_get_cntr_values(char *cntr_root, char *prefix, int num_cntrs,
			 char **cntr_names, uint64_t *cntrs)
{
	char s[128];
	FILE *fp;
	int i;

	for (i = 0; i < num_cntrs; i++) {
		snprintf(s, ARRAY_SIZE(s), "%s/%s%s", cntr_root,
			 prefix ? prefix : "", cntr_names[i]);
		fp = fopen(s, "r");
		if (!fp)
			continue;
		if (fscanf(fp, "%lu", &cntrs[i]) != 1)
			perror("fscanf");
		fclose(fp);
	}

	return num_cntrs;
}

int getdrstat_names_ibst(void)
{
	return ibst_get_cntr_names(ibst_cntr_root[0][0], "DRIVER_",
				   &drstatnames);
}

int getcntr_names_ibst(int unit)
{
	return ibst_get_cntr_names(ibst_cntr_root[unit][0], "!DRIVER_",
				   &cntrnames[unit]);
}

int getportcntr_names_ibst(int unit)
{
	return ibst_get_cntr_names(ibst_cntr_root[unit][1], NULL,
				   &portcntrnames[unit]);
}

int getdrstats_ibst(uint64_t *s, int len)
{

	return ibst_get_cntr_values(ibst_cntr_root[0][0], "DRIVER_", len,
				    drstatnames, s);
}

int getcntrs_ibst(int unit, uint64_t *s, int len)
{
	return ibst_get_cntr_values(ibst_cntr_root[unit][0], NULL, len,
				    cntrnames[unit], s);
}

int getportcntrs_ibst(int unit, int port, uint64_t *s, int len)
{
	return ibst_get_cntr_values(ibst_cntr_root[unit][port + 1], NULL, len,
				    portcntrnames[unit], s);
}

void
usage(int exval)
{
	struct opts {
		char *option;
		char *desc;
	} *ptr, options[] = {
		{"-c", "Continuous display interval (in seconds)"},
		{"-e", "Display only error statistics"},
		{"-i", "Display statistics for specific unit/port"},
		{"-m", "Mount point of the IpathFS filesystem"},
		{"-z", "Display only non-zero statistics"},
		{"-n", "Display statistics in specified number of columns"},
		{"-E", "Exclude statistics from list (by name)"},
		{"-o", "Display statistics delta"},
		{"-v", "Verbose output"},
		{"-V", "Display version information and exit"},
		{"-F", "Filter statistics based on options(SDMA,DC,CCE,TX,RX,XMIT,MISC,PIO,PCIC,SEND)"},
		{"-j", "Display statistics in JSON format"},
		{0, 0}
	};

        fprintf(stderr, "Usage: [-c secs [-o]] [-e (errors)] [-h (help)] [-i unit[:IBport#]]\n"
		"\t[-m mntpt] [-z (nozero)] [-n columns)] [-v (verbose)]\n"
		"\t[-E exclude1,exclude2...] [-F option1,option2...\n");
	for (ptr = options; ptr->option; ptr++)
		fprintf(stderr, "%5s %s\n", ptr->option, ptr->desc);
	exit(exval);

}

/* we can't use stat to get size of the names or stat files, because
 * the simple*fs kernel libfs routines don't support setting the
 * size of the files.
 */
int getdrstat_names()
{
	char *nm, pathname[PATH_MAX];
	char buf[8193], *names[512]; /* ridiculously large */
	int i, t, cnt, nfd, firsterr = -1;

	if (use_ibst)
		return getdrstat_names_ibst();

	snprintf(pathname, sizeof(pathname), "%s/%s", mntpnt,
			"driver_stats_names");
	nfd = open(pathname, O_RDONLY);
	if(nfd < 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to open %s: %s\n", pathname,
				strerror(errno));
		}
		return 0;
	}
	i = read(nfd, buf, sizeof(buf)-1);
	if(i <= 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Read of stat names from %s failed: %s\n",
				pathname, i == -1 ? strerror(errno) : "no data");
		}
		close(nfd);
		return 0;
	}
	close(nfd);

	buf[i] = 0;
	for(cnt=t=0; t<i; cnt++) {
		char *nl;
		nm = buf + t;
		if(!*nm)
			break;
		nl = strchr(nm, '\n');
		if(nl) {
			*nl = 0;
			t += 1 + nl - nm;
		}
		else
			t = i;
		if(!strncmp(nm, "E ", 2)) {
			if(firsterr == -1)
				firsterr = cnt;
			nm += 2;
		}
		if(cnt < (sizeof(names)/sizeof(names[0]))) {
			if (excludes) {
				char **e = excludes;
				while(*e) {
					if (!strcmp(*e, nm)) {
						nm = NULL;
						break;
					}
					e++;
				}
			}
			names[cnt] = nm;
		}
	}
	if(cnt >= (sizeof(names)/sizeof(names[0]))) {
		printf("Too many driver stat names (%u), skipping rest\n", cnt);
		cnt = sizeof(names)/sizeof(names[0]);
	}
	drstatnames = malloc(cnt * sizeof(char *));
	if(!drstatnames) {
		fprintf(stderr, "No memory to save %u driver stat names: %s\n",
			cnt, strerror(errno));
		return 0;
	}
	for(i=0; i<cnt; i++) {
		if (!names[i]) { /* excluded */
			drstatnames[i] = NULL;
			continue;
		}
		drstatnames[i] = strdup(names[i]);
		if(!drstatnames[i]) {
			fprintf(stderr, "No memory to save driver stat name %s: %s\n",
				names[i], strerror(errno));
			return 0;
		}
	}
	stat1sterr = firsterr == -1 ? cnt /* none */ : firsterr;
	return cnt;
}

int getcntr_names(int unit)
{
	char *nm, pathname[PATH_MAX];
	char buf[8193], *names[512]; /* ridiculously large */
	int i, t, cnt, nfd, firsterr = -1;

	if (use_ibst)
		return getcntr_names_ibst(unit);

	snprintf(pathname, sizeof(pathname), "%s/%u/%s", mntpnt, unit,
		"counter_names");

	nfd = open(pathname, O_RDONLY);
	if(nfd < 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to open %s: %s\n", pathname,
				strerror(errno));
		}
		return 0;
	}
	i = read(nfd, buf, sizeof(buf)-1);
	if(i <= 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Read of counter names from %s failed: %s\n",
				pathname, i == -1 ? strerror(errno) : "no data");
		}
		close(nfd);
		return 0;
	}
	close(nfd);

	buf[i] = 0;
	for(cnt=t=0; t<i; cnt++) {
		char *nl;
		nm = buf + t;
		if(!*nm)
			break;
		nl = strchr(nm, '\n');
		if(nl) {
			*nl = 0;
			t += 1 + nl - nm;
		}
		else
			t = i;
		if(!strncmp(nm, "E ", 2)) {
			if(firsterr == -1)
				firsterr = cnt;
			nm += 2;
		}
		if(cnt < (sizeof(names)/sizeof(names[0]))) {
			if (excludes) {
				char **e = excludes;
				while(*e) {
					if (!strcmp(*e, nm)) {
						nm = NULL;
						break;
					}
					e++;
				}
			}
			names[cnt] = nm;
		}
	}
	if(cnt >= (sizeof(names)/sizeof(names[0]))) {
		printf("Too many counter names (%u), skipping rest\n", cnt);
		cnt = sizeof(names)/sizeof(names[0]);
	}
	cntrnames[unit] = malloc(cnt * sizeof(char *));
	if(!cntrnames) {
		fprintf(stderr, "No memory to save %u counter names: %s\n",
			cnt, strerror(errno));
		return 0;
	}
	for(i=0; i<cnt; i++) {
		if (!names[i]) { /* excluded */
			cntrnames[unit][i] = NULL;
			continue;
		}
		cntrnames[unit][i] = strdup(names[i]);
		if(!cntrnames[unit][i]) {
			fprintf(stderr, "No memory to save counter name %s: %s\n",
				names[i], strerror(errno));
			return 0;
		}
	}
	if(dev1sterr)
		dev1sterr[unit] = firsterr == -1 ? cnt /* none */ : firsterr;
	return cnt;
}

int getportcntr_names(int unit)
{
	char *nm, pathname[PATH_MAX];
	char buf[8193], *names[512]; /* ridiculously large */
	int i, t, cnt, nfd, firsterr = -1;

	if (use_ibst)
		return getportcntr_names_ibst(unit);

	snprintf(pathname, sizeof(pathname), "%s/%u/portcounter_names",
		mntpnt, unit);

	nfd = open(pathname, O_RDONLY);
	if(nfd < 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to open %s: %s\n", pathname,
				strerror(errno));
		}
		return 0;
	}
	i = read(nfd, buf, sizeof(buf)-1);
	if(i <= 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Read of portcounter names from %s "
				"failed: %s\n",
				pathname, i == -1 ? strerror(errno)
				: "no data");
		}
		close(nfd);
		return 0;
	}
	close(nfd);

	buf[i] = 0;
	for(cnt=t=0; t<i; cnt++) {
		char *nl;
		nm = buf + t;
		if(!*nm)
			break;
		nl = strchr(nm, '\n');
		if(nl) {
			*nl = 0;
			t += 1 + nl - nm;
		}
		else
			t = i;
		if(!strncmp(nm, "E ", 2)) {
			if(firsterr == -1)
				firsterr = cnt;
			nm += 2;
		}
		if(cnt < (sizeof(names)/sizeof(names[0]))) {
			if (excludes) {
				char **e = excludes;
				while(*e) {
					if (!strcmp(*e, nm)) {
						nm = NULL;
						break;
					}
					e++;
				}
			}
			names[cnt] = nm;
		}
	}
	if(cnt >= (sizeof(names)/sizeof(names[0]))) {
		printf("Too many portcntr names (%u), skipping rest\n", cnt);
		cnt = sizeof(names)/sizeof(names[0]);
	}
	portcntrnames[unit] = malloc(cnt * sizeof(char *));
	if(!portcntrnames[unit]) {
		fprintf(stderr, "No memory to save %u portcntr names: %s\n",
			cnt, strerror(errno));
		return 0;
	}
	for(i=0; i<cnt; i++) {
		if (!names[i]) { /* excluded */
			portcntrnames[unit][i] = NULL;
			continue;
		}
		portcntrnames[unit][i] = strdup(names[i]);
		if(!portcntrnames[unit][i]) {
			fprintf(stderr, "No memory to save portcntr name %s: %s\n",
				names[i], strerror(errno));
			return 0;
		}
	}
	if(port1sterr)
		port1sterr[unit] = firsterr == -1 ? cnt /* none */ :
			firsterr;
	return cnt;
}

int getdrstats(uint64_t *s, int len)
{
	int i, n;

	if (use_ibst) {
		drstatfd = -1;
		return getdrstats_ibst(s, len);
	}

	if(drstatfd == -1) {
		char pathname[PATH_MAX];
		snprintf(pathname, sizeof(pathname), "%s/driver_stats", mntpnt);
		drstatfd = open(pathname, O_RDONLY);
		if(drstatfd < 0) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Unable to open %s: %s\n",
					pathname, strerror(errno));
			}
			return 0;
		}
	}
	else if(lseek(drstatfd, 0, SEEK_SET) != 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to rewind driver stats: %s\n",
				strerror(errno));
		}
		return 0;
	}
	n = len*sizeof(*s);
	i = read(drstatfd, s, n);
	if(i != n) {
		if(i == -1) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Error reading driver stat: %s\n",
					strerror(errno));
			}
			close(drstatfd);
			drstatfd = -1;
			return i;
		}
		else if(verbose)
			fprintf(stderr, "Read only %u of %u bytes from driver"
				" stats\n", i, n);
	}
	return i/sizeof(s);
}

int is_cntr_32_bits(char * cntr_name, int remove_suffix)
{
	int result = 0;
	char * p_suffix = NULL;
	p_suffix = strstr(cntr_name, ",32");
	if (p_suffix != NULL) {
		result = 1;
		if (remove_suffix)
			*p_suffix = '\0';
	}
	return result;
}

uint64_t cntr_delta_rollover(uint64_t prevstat, uint64_t curstat, int is_cntr_32_bits)
{
	uint32_t prev_32;
	uint32_t curr_32;
	uint32_t max_32_bit_num = U32_MAX;
	uint64_t delt;

	if (is_cntr_32_bits){
		prev_32 = (uint32_t) prevstat; /* Getting lower 32 bits */
		curr_32 = (uint32_t) curstat;  /* Getting lower 32 bits */

		if (prev_32 > curr_32) {
			delt = (max_32_bit_num - prev_32) + curr_32;
		} else {
			delt = curr_32 - prev_32;
		}
	} else {
		delt = curstat - prevstat;
	}

	return delt;
}

/* show the per-driver stats, and optionally deltas, in multicolumn
 * or single-line format.
 */
int showstats(uint64_t *prevstat, uint64_t *curstat, int nst)
{
	int i, npr = 0;
	uint64_t val = 0;
	int64_t delt = 0;
	char *cntr_name;
	int cntr_32_bit = 0;

	if (json_format)
		printf("\t\"general_stats\" : {\n");

	for(npr = i = 0; i < nst; i++) {
		if (sdma || dc || cce || misc || pio || pcic || send)
			continue;
		if(!drstatnames[i] || (errors_only && i < stat1sterr))
			continue;
		cntr_name = strdup(drstatnames[i]);
                if(!cntr_name) {
                       fprintf(stderr, "No memory to save driver stat name %s: %s\n",
                                 drstatnames[i], strerror(errno));
                        break;
                }
		cntr_32_bit = is_cntr_32_bits(cntr_name, 1);

		if(prevstat)
			delt = cntr_delta_rollover(prevstat[i], curstat[i],
					cntr_32_bit);
		else
			val = curstat[i];
		if(((prevstat || nozero) && !delt) ||
			(!prevstat && nozero && !val)) {
			free(cntr_name);
			continue;
		}
		if(!npr++ && prevstat)
			printf(deltafmt, udelta.tv_sec, udelta.tv_usec/1000);

		if(prevstat) {
			/* stats deltas can be negative, for open ports,
			 * maybe others in future */
			if((delt > 0 && delt >= MAX_DIGITS_BEFORE_DIVIDE) ||
				(delt < 0 && -delt >= MAX_DIGITS_BEFORE_DIVIDE))
				if (json_format)
					printf(json_colfmtdK, maxstrlen,
						cntr_name,
						(long long)delt / 1000LL);
				else
					printf(colfmtdK, maxstrlen, cntr_name,
						(long long)delt / 1000LL);
			else
				if (json_format)
					printf(json_colfmtd, maxstrlen,
						cntr_name,
						(long long)delt);
				else
					printf(colfmtd, maxstrlen, cntr_name,
						(long long)delt);
		}
		else if(val < MAX_DIGITS_BEFORE_DIVIDE) {
			if (json_format)
				printf(json_colfmt, maxstrlen, cntr_name,
					(ull)val);
			else
				printf(colfmt, maxstrlen, cntr_name, (ull)val);
		} else {
			if (json_format)
				printf(json_colfmtK, maxstrlen, cntr_name,
					(ull)val / 1000ULL);
			else
				printf(colfmtK, maxstrlen, cntr_name,
					(ull)val / 1000ULL);
		}
		if (json_format) {
			if ((i + 1) < nst)
				printf(",");
			printf("\n");
		} else {
			printf("%s", (numcols == 1 || !(npr%numcols)) ? "\n" : " ");
		}
		free(cntr_name);
	}

	if (json_format) {
		printf("\t},\n");
	} else {
		if(numcols > 1 && (npr%numcols))
			printf("\n");
	}
	return npr;
}

int getcntrs(int unit, uint64_t *s, int len)
{
	int i, n;

	if (use_ibst) {
		devcntrfd[unit] = -1;
		return getcntrs_ibst(unit, s, len);
	}

	if(devcntrfd[unit] == -1) {
		char pathname[PATH_MAX];
		snprintf(pathname, sizeof(pathname), "%s/%d/counters", mntpnt,
			unit);
		devcntrfd[unit] = open(pathname, O_RDONLY);
		if(devcntrfd[unit] < 0) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Unable to open %s: %s\n",
					pathname, strerror(errno));
			}
			return 0;
		}
	}
	else if(lseek(devcntrfd[unit], 0, SEEK_SET) != 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to rewind unit %d counters: %s\n",
				unit, strerror(errno));
		}
		close(devcntrfd[unit]);
		devcntrfd[unit] = -1;
		return 0;
	}
	n = len*sizeof(*s);
	i = read(devcntrfd[unit], s, n);
	if(i != n) {
		if(i == -1) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Error reading %d/counters: %s\n",
					unit, strerror(errno));
			}
			return i;
		}
		else if(verbose)
			fprintf(stderr, "Read only %u of %u bytes from "
				"%d/counters\n", i, n, unit);
	}
	return i/sizeof(s);
}

/* show the per-unit counters, and optionally deltas, in multicolumn
 * or single-line format.
 */
int showcntrs(int unit, uint64_t *prevstat, uint64_t *curstat, int ncntr, int any)
{
	int i, npr = 0;
	uint64_t delt;
	int print=1;
	char *cntr_name;
	int cntr_32_bit = 0;

	for(i = 0; i < ncntr; i++) {
		if(!cntrnames[unit][i] || (errors_only && i < dev1sterr[unit]))
			continue;
		cntr_name = strdup(cntrnames[unit][i]);
                if(!cntr_name) {
                       fprintf(stderr, "No memory to save driver stat name %s: %s\n",
                                 cntrnames[unit][i], strerror(errno));
                        break;
                }
		cntr_32_bit = is_cntr_32_bits(cntr_name, 1);
		print = 1;
		if(filter){
			if (strncmp("SDMA", cntrnames[unit][i], 4) == 0
				&& !sdma)
				print = 0;
			if (strncmp("SDma", cntrnames[unit][i], 4) == 0
				&& !sdma)
				print = 0;
			if (strncmp("Dc", cntrnames[unit][i], 2) == 0
				&& !dc)
				print = 0;
			if (strncmp("Cce", cntrnames[unit][i], 3) == 0
				&& !cce)
				print = 0;
			if (strncmp("Rx", cntrnames[unit][i], 2) == 0
				&& !rcv)
				print = 0;
			if (strncmp("Tx", cntrnames[unit][i], 2) == 0
				&& !tx)
				print = 0;
			if (strncmp("MISC", cntrnames[unit][i], 4) == 0
				&& !misc)
				print = 0;
			if (strncmp("Pio", cntrnames[unit][i], 3) == 0
				&& !pio)
				print = 0;
			if (strncmp("Pcic", cntrnames[unit][i], 4) == 0
				&& !pcic)
				print = 0;
			if (strncmp("Send", cntrnames[unit][i], 4) == 0
				&& !send)
				print = 0;
			}
		if(print == 0) {
			free(cntr_name);
			continue;
		}
		if(prevstat)
			delt = cntr_delta_rollover(prevstat[i], curstat[i],
					    cntr_32_bit);
		else
			delt = curstat[i];
		if((prevstat || nozero) && !delt) {
			free(cntr_name);
			continue;
		}
		if(prevstat && !any++)
			printf(deltafmt, udelta.tv_sec, udelta.tv_usec/1000);

		if (json_format) {
			npr++;
			printf("\t\t");
		} else {
			if(!npr++)
				printf("Unit%d:\n", unit);
		}

		if(delt < MAX_DIGITS_BEFORE_DIVIDE) {
			if (json_format)
				printf(json_colfmt, maxstrlen, cntr_name,
					(ull)delt);
			else
				printf(colfmt, maxstrlen, cntr_name, (ull)delt);
		} else {
			if (json_format)
				printf(json_colfmtK, maxstrlen, cntr_name,
					(ull)delt/1000ULL);
			else
				printf(colfmtK, maxstrlen, cntr_name,
					(ull)delt/1000ULL);
		}

		if (json_format) {
			if ((i + 1) < ncntr)
				printf(",");
			printf("\n");
		} else {
			printf("%s", (numcols == 1 || !(npr%numcols)) ? "\n" : " ");
		}
		free(cntr_name);
	}

	if (!json_format) {
		if(numcols > 1 && (npr%numcols))
			printf("\n");
	}

	return npr + any;
}

int getportcntrs(int unit, int port, uint64_t *s, int len)
{
	int i, n;

	if(use_ibst) {
		portcntrfd[unit][port] = -1;
		return getportcntrs_ibst(unit, port, s, len);
	}

	if(portcntrfd[unit][port] == -1) {
		char pathname[PATH_MAX];
		snprintf(pathname, sizeof(pathname), "%s/%d/port%dcounters",
			mntpnt, unit, port+1);
		portcntrfd[unit][port] = open(pathname, O_RDONLY);
		if(portcntrfd[unit][port] < 0) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Unable to open %s: %s\n",
					pathname, strerror(errno));
			}
			return 0;
		}
	}
	else if(lseek(portcntrfd[unit][port], 0, SEEK_SET) != 0) {
		if(!check_complained || verbose) {
			check_complained = 1;
			fprintf(stderr, "Unable to rewind unit %d,%d counters: %s\n",
				unit, port, strerror(errno));
		}
		close(portcntrfd[unit][port]);
		portcntrfd[unit][port] = -1;
		return 0;
	}
	n = len*sizeof(*s);
	i = read(portcntrfd[unit][port], s, n);
	if(i != n) {
		if(i == -1) {
			if(!check_complained || verbose) {
				check_complained = 1;
				fprintf(stderr, "Error reading %d/port%dcounters"
					": %s\n", unit, port, strerror(errno));
			}
			return i;
		}
		else if(verbose)
			fprintf(stderr, "Read only %u of %u bytes from "
				"%d/port%dcounters\n", i, n, unit, port);
	}
	return i/sizeof(s);
}

/* show the per-port counters, and optionally deltas, in multicolumn
 * or single-line format.
 */
int showportcntrs(int unit, int port, uint64_t *prevstat, uint64_t *curstat,
	int ncntr, int any)
{
	int i, npr = 0;
	uint64_t delt;
	int print = 1;
	char *cntr_name;
	int cntr_32_bit = 0;

	for(i = 0; i < ncntr; i++) {
		if(!portcntrnames[unit][i] || (errors_only && i < port1sterr[unit]))
			continue;
		cntr_name = strdup(portcntrnames[unit][i]);
                if(!cntr_name) {
                       fprintf(stderr, "No memory to save driver stat name %s: %s\n",
                                 portcntrnames[unit][i], strerror(errno));
                        break;
                }
		cntr_32_bit = is_cntr_32_bits(cntr_name, 1);

		print = 1;
		if(filter){
			if((sdma || dc || cce) && !(tx || rcv || xmit))
				print = 0;
			if(strncmp("Tx", portcntrnames[unit][i], 2)==0 && !tx)
				print = 0;
			if(strncmp("Rcv", portcntrnames[unit][i], 3)==0 && !rcv)
				print = 0;
			if(strncmp("Xmit", portcntrnames[unit][i], 4)==0 && !xmit)
				print = 0;
		}
		if(print == 0) {
			free(cntr_name);
			continue;
		}
		/* show flow packets if verbose; never for changes; hacky
		 * because it "knows" the labelname for them */
		if((!verbose || (prevstat && verbose < 2)) &&
			!strcmp("xFlowPkt", portcntrnames[unit][i]+1)) {
			free(cntr_name);
			continue;
		}
		if(prevstat)
			delt = cntr_delta_rollover(prevstat[i], curstat[i],
					cntr_32_bit);
		else
			delt = curstat[i];
		if((prevstat || nozero) && !delt) {
			free(cntr_name);
			continue;
		}
		if(prevstat && !any++)
			printf(deltafmt, udelta.tv_sec, udelta.tv_usec/1000);

		if (json_format) {
			npr++;
			printf("\t\t");
		} else {
			if(!npr++)
				printf("Port%d,%d:\n", unit, port+1);
		}

		if(delt < MAX_DIGITS_BEFORE_DIVIDE) {
			if (json_format)
				printf(json_colfmt, maxstrlen, cntr_name,
					(ull)delt);
			else
				printf(colfmt, maxstrlen, cntr_name, (ull)delt);
		} else {
			if (json_format)
				printf(json_colfmtK, maxstrlen, cntr_name,
					(ull)delt/1000ULL);
			else
				printf(colfmtK, maxstrlen, cntr_name,
					(ull)delt/1000ULL);
		}

		if (json_format) {
			if ((i + 1) < ncntr)
				printf(",");
			printf("\n");
		} else {
			printf("%s", (numcols == 1 || !(npr%numcols)) ? "\n" : " ");
		}
		free(cntr_name);
	}

	if (!json_format) {
		if(numcols > 1 && (npr%numcols))
			printf("\n");
	}

	return npr + any;
}


/* cleanup, after ipathfs unmounted, etc.
 *  This can be executed multiple times, so must
 *  check state. nunits and nports can change
 *  on a re-init, so this has to be done before
 *  they are set.  Because a new driver can be
 *  loaded while running, even the number of
 *  counters, etc. can change (or we can be started
 *  before the driver is loaded, and if -c is used,
 *  we'll keep trying until it's loaded).
 */
void cleanup(void)
{
	int unit, port;

	if(drstatfd != -1) {
		close(drstatfd);
		drstatfd = -1;
	}

	for(unit = 0; unit < nunits; unit++) {
		if(devcntrfd && devcntrfd[unit] != -1){
			close(devcntrfd[unit]);
			devcntrfd[unit] = -1;
		}
		if(cntrs)
			free(cntrs[unit]);
		if(last_cntrs)
			free(last_cntrs[unit]);
		if(!nports || !nports[unit])
			continue;
		for(port = 0; port < nports[unit]; port++) {
			if(portcntrfd && portcntrfd[unit] &&
				portcntrfd[unit][port] != -1)
				close(portcntrfd[unit][port]);
			if(portcntrs && portcntrs[unit])
				free(portcntrs[unit][port]);
			if(last_portcntrs && last_portcntrs[unit])
				free(last_portcntrs[unit][port]);
		}
		if(portcntrs)
			free(portcntrs[unit]);
		if(last_portcntrs)
			free(last_portcntrs[unit]);
		if(portcntrfd)
			free(portcntrfd[unit]);
	}
	free(stats);
	free(last_stats);
	free(cntrs);
	free(last_cntrs);
	free(devcntrfd);
	free(dev1sterr);
	free(nports);
	free(portcntrs);
	free(last_portcntrs);
	free(portcntrfd);
	free(port1sterr);

	nunits = 0; /* make it safe for next time */

	if (use_ibst)
		ibst_exit();
}

void initialize(void)
{
	char pathname[PATH_MAX];
	struct stat st;
	int i, j = 0, unit;

	if(stat(mntpnt, &st) || !S_ISDIR(st.st_mode)) {
		if (ibst_init() < 0) {
			if(!check_complained) {
				check_complained = 1;
				/* don't specially warn if dir present, not mounted */
				warnx("%s mountpoint not found\n", mntpnt);
			}
			return; /* if -c, wait for it to show up */
		}
		use_ibst = 1;
	}

	nstats = getdrstat_names();
	if(!nstats && !check_complained)
		check_complained = 1;
	else {
		stats = calloc(nstats, sizeof(*stats));
		if(changes)
			last_stats = calloc(nstats, sizeof(*stats));
		if(!stats || (changes && !last_stats)) {
			fprintf(stderr,
				"No memory for %u driver stat fields: %s\n",
				nstats, strerror(errno));
			nstats = 0;
		}
	}
	if (use_ibst) {
		nunits = ibst_num_units;
	} else {
		for(i=0; ; i++) {
			snprintf(pathname, sizeof(pathname), "%s/%d", mntpnt, i);
			if(stat(pathname, &st) || !S_ISDIR(st.st_mode))
				break;
		}
		nunits = i;
	}
	if(!nunits) {
		if(!check_complained) {
			check_complained = 1;
			fprintf(stderr, "Didn't find any devices in %s\n",
				mntpnt);
		}
		return;
	}

	nports = calloc(nunits, sizeof(*nports));
	cntrsz = calloc(nunits, sizeof(*cntrsz));
	cntrnames = calloc(nunits, sizeof(*cntrnames));
	devcntrfd = calloc(nunits, sizeof(*devcntrfd));
	cntrs = calloc(nunits, sizeof(*cntrs));
	dev1sterr = calloc(nunits, sizeof(*dev1sterr));
	portcntrsz = calloc(nunits, sizeof(*portcntrsz));
	portcntrnames = calloc(nunits, sizeof(*portcntrnames));
	portcntrfd = calloc(nunits, sizeof(*portcntrfd));
	portcntrs = calloc(nunits, sizeof(*portcntrs));
	port1sterr = calloc(nunits, sizeof(*port1sterr));
	if(changes) {
		last_cntrs = calloc(nunits, sizeof(*last_cntrs));
		last_portcntrs = calloc(nunits, sizeof(*last_portcntrs));
	}
	if(!nports || !cntrsz || !cntrnames || !devcntrfd || !cntrs ||
		!dev1sterr || !portcntrsz || !portcntrnames || !portcntrfd ||
		!portcntrs || (changes && (!last_cntrs || !last_portcntrs))) {
		errx(3, "No memory for counter counts: %s\n",
			strerror(errno));
	}

	for(i=0; i<nunits; i++) {
		if (use_ibst) {
			nports[i] = ibst_num_ports[i];
		} else {
			for(j=1; ; j++) {
				snprintf(pathname, sizeof(pathname),
					"%s/%d/port%dcounters", mntpnt, i, j);
				if(stat(pathname, &st))
					break;
			}
			nports[i] = j - 1;
		}
		if(!nports[i]) { /* not fatal */
			fprintf(stderr, "No port counter info for unit %u!\n",
				i);
			continue;
		}
	}

	drstatfd = -1;
	for(unit = 0; unit < nunits; unit++) {
		devcntrfd[unit] = -1;
		cntrsz[unit] = getcntr_names(unit);
		if(!cntrsz[unit]) {
			printf("Skipping unit %u cntrs\n", unit);
			continue;
		}
		cntrs[unit] = calloc(cntrsz[unit], sizeof(*cntrs));
		if(nports[unit]) {
			portcntrs[unit] = calloc(nports[unit], sizeof(**portcntrs));
			portcntrfd[unit] = calloc(nports[unit], sizeof(**portcntrfd));
		}
		if(!cntrs[unit] || (nports[unit] && (!portcntrs || !portcntrfd[unit]))) {
			errx(3, "No memory for unit %u port counters: %s\n",
				unit, strerror(errno));
		}
		if(changes) {
			last_cntrs[unit] = calloc(cntrsz[unit], sizeof(*cntrs));
			last_portcntrs[unit] = calloc(nports[unit],
				sizeof(**last_portcntrs));
			if(!last_cntrs[unit] || !last_portcntrs[unit])
				errx(3, "No memory for unit %u port "
					"lastcounters: %s\n",
					unit, strerror(errno));
		}
		portcntrsz[unit] = getportcntr_names(unit);
		if(!portcntrsz[unit]) {
			printf("Skipping unit %u port %u cntrs\n",
				unit, j);
			continue;
		}
		for(j=0; j < nports[unit]; j++) {
			portcntrfd[unit][j] = -1;
			portcntrs[unit][j] = calloc(portcntrsz[unit],
				sizeof(**portcntrs));
			if(!portcntrs[unit][j])
				errx(3, "No memory for unit %u port %u "
					"counters: %s\n",
					unit, j, strerror(errno));
			if(changes) {
				last_portcntrs[unit][j] = calloc(portcntrsz[unit],
					sizeof(**portcntrs));
				if(!last_portcntrs[unit][j])
					errx(3, "No memory for unit %u "
						"port %u counters: %s\n",
						unit, j, strerror(errno));
			}
		}
	}
	free(excludes);
	check_complained = 0;
}

void shandle(int param)
{
	(void)param;
}

void build_excludes(char *excl)
{
	int i, n;
	char **e;

	e = calloc(1 + strlen(excl)/2, sizeof(*e)); /* max possible strings */
	if (!e)
		err(1, "Unable to allocate memory for exclusion list");
	for(n=0; ; n++) {
		if (!(e[n] = strtok(excl, ",")))
		    break;
		if (!n)
			excl = 0;
	}
	excludes = calloc(n + 1, sizeof(*excludes));
	if (!excludes)
		err(1, "Unable to allocate memory for exclusion list");
	for(i=0; i < n; i++)
		excludes[i] = e[i];
	free(e);
}

int max_strlen_filtered_stats(int count)
{
	int i, len;

	maxstrlen = 0;
	for (i = 0; i < count; i++) {
		if (sdma || dc || cce || misc || pio || pcic || send)
			continue;
		if (!drstatnames[i])
			continue;
		len = strlen(drstatnames[i]);
		if (len > maxstrlen)
			maxstrlen = len;
	}
	return maxstrlen;
}

int max_strlen_filtered_cntrnames(int unit, int counters)
{
	int i, isvalid, len;

	maxstrlen = 0;
	for (i = 0; i < counters; i++) {
		if (!cntrnames[unit][i])
			continue;
		isvalid = 1;
		if (filter) {
			if (strncasecmp("SDMA", cntrnames[unit][i], 4) == 0
				&& !sdma)
				isvalid = 0;
			if (strncasecmp("Dc", cntrnames[unit][i], 2) == 0
				&& !dc)
				isvalid = 0;
			if (strncasecmp("Cce", cntrnames[unit][i], 3) == 0
				&& !cce)
				isvalid = 0;
			if (strncasecmp("Rx", cntrnames[unit][i], 2) == 0
				&& !rcv)
				isvalid = 0;
			if (strncasecmp("Tx", cntrnames[unit][i], 2) == 0
				&& !tx)
				isvalid = 0;
			if (strncasecmp("MISC", cntrnames[unit][i], 4) == 0
				&& !misc)
				isvalid = 0;
			if (strncasecmp("Pio", cntrnames[unit][i], 3) == 0
				&& !pio)
				isvalid = 0;
			if (strncasecmp("Pcic", cntrnames[unit][i], 4) == 0
				&& !pcic)
				isvalid = 0;
			if (strncasecmp("Send", cntrnames[unit][i], 4) == 0
				&& !send)
				isvalid = 0;
		}
		if (isvalid) {
			len = strlen(cntrnames[unit][i]);
			if (len > maxstrlen)
				maxstrlen = len;
		}
	}
	return maxstrlen;
}

int max_strlen_filtered_portcntrnames(int unit, int counters)
{
	int i, isvalid, len;

	maxstrlen = 0;
	for (i = 0; i < counters; i++) {
		if (!portcntrnames[unit][i])
			continue;
		isvalid = 1;
		if (filter) {
			if ((sdma || dc || cce) && !(tx || rcv || xmit))
				isvalid = 0;
			if (strncmp("Tx", portcntrnames[unit][i], 2) == 0
				&& !tx)
				isvalid = 0;
			if (strncmp("Rcv", portcntrnames[unit][i], 3) == 0
				&& !rcv)
				isvalid = 0;
			if (strncmp("Xmit", portcntrnames[unit][i], 4) == 0
				&& !xmit)
				isvalid = 0;
		}
		if (isvalid) {
			len = strlen(portcntrnames[unit][i]);
			if (len > maxstrlen)
				maxstrlen = len;
		}
	}
	return maxstrlen;
}

int calc_num_columns(void)
{
	int i = 0;
	struct winsize term_window_size;

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_window_size);
	i = term_window_size.ws_col / (maxstrlen+15);

	if (i == 0)
		i = 1;
	return i;
}

int main(int cnt, char **args)
{
	int l, r, unit, port, nret = 0, delta_only = 0;
	struct timespec nsleep = {0, 0};
	float fsec;
	char *nxt;
	char *excl = NULL;
	char *subopt = NULL;
	char *value = NULL;
	enum {
		SDMA = 0,
		DC,
		CCE,
		RX,
		TX,
		XMIT,
		MISC,
		PIO,
		PCIC,
		SEND,
	};
	char *const token[] = {
		[SDMA] = "SDMA",
		[DC] = "DC",
		[CCE] = "CCE",
		[TX] = "TX",
		[RX] = "RX",
		[XMIT] = "XMIT",
		[MISC] = "MISC",
		[PIO] = "PIO",
		[PCIC] = "PCIC",
		[SEND] = "SEND",
		NULL
	};

	while ((l = getopt(cnt, args, "c:ehi:m:ovVn:zE:F:j")) > 0)
	switch (l) {
	case 'n':
		numcols = strtod(optarg, &nxt);
		if (nxt == optarg || numcols < 1)
			usage(2);
		autosize = 0;
		break;
	case 'e':
		errors_only++;
		break;
	case 'E':
		excl = optarg;
		break;
	case 'c':
		changes = 1;
		fsec = (float) strtod(optarg, &nxt);
		if(nxt == optarg || fsec < 0.0001)
			usage(2);
		nsleep.tv_sec = floorf(fsec);
		nsleep.tv_nsec =
		    floorf(1e9 * (fsec - (float) nsleep.tv_sec));
		break;
	case 'm':
		mntpnt = optarg;
		break;
	case 'o':
		delta_only = 1;
		break;
	case 'h':
		usage(0);
		break;
	case 'i':
		hca = strtoul(optarg, &nxt, 0);
		if(nxt == optarg) {
			if(!strncmp("qib", optarg, 3)) {
				char *p = optarg + 3;
				hca = strtoul(p, &nxt, 0);
				if(nxt == p)
					usage(2);
			}
			else
				usage(2);
		}
		if(*nxt == ':') {
			char *p = nxt + 1;
			hcaport = strtoul(p, &nxt, 0);
			if(nxt == p || !hcaport) /* 1, not 0-based ... */
				usage(2);
			hcaport--; /* simplify rest of code */
			portset = 1;
		}
		hcaset = 1;
		break;
	case 'v':
		verbose++;
		break;
	case 'V':
		printf("%s %s\n", __DIAGTOOLS_GIT_VERSION,
			__DIAGTOOLS_GIT_DATE);
		exit(0);
		break;
	case 'z':
		nozero++;
		break;
	case 'F':
		filter++;
		subopt = strdup(optarg);
		do{
			switch(getsubopt(&subopt,token,&value)){
			case SDMA:
				sdma++;
				break;
			case DC:
				dc++;
				break;
			case CCE:
				cce++;
				break;
			case RX:
				rcv++;
				break;
			case TX:
				tx++;
				break;
			case XMIT:
				xmit++;
				break;
			case MISC:
				misc++;
				break;
			case PIO:
				pio++;
				break;
			case PCIC:
				pcic++;
				break;
			case SEND:
				send++;
				break;
			default:
				usage(2);
				break;
			}
		} while(*subopt!='\0');
		break;
	case 'j':
		json_format = 1;
		break;
	default:
		usage(2);
		break;
	}

	if(optind != cnt || (delta_only && !changes))
		usage(2);

	if (excl)
		build_excludes(excl);

	/* if continuous display intervals is used, don't allow JSON output */
	if (changes && json_format)
		json_format = 0;

re_init:
	initialize();

	if((!nunits || !nstats) && !changes)
		exit(0); /* messages already printed */

	if(nunits && hcaset && hca >= nunits)
		errx(4, "Unit %u not found", hca);
	if(nunits && portset)
		if(hcaport >= nports[hca])
			errx(4, "IB port %u:%u not found", hca, hcaport+1);

	if (json_format)
		printf("{\n"); /* First JSON brace */
	if((r = getdrstats(stats, nstats)) != -1) {
		maxstrlen = max_strlen_filtered_stats(r);
		if (autosize == 1)
			numcols = calc_num_columns();
		if(!delta_only) {
			showstats(NULL, stats, r);
		}
	}

	if (json_format)
		printf("\t\"units\" : {\n");
	for(unit = 0; unit < nunits; unit++) {
		if (json_format) {
			printf("\t\t\"unit%d\" : {\n", unit);
		}
		if(hcaset && unit != hca)
			continue;
		if((r = getcntrs(unit, cntrs[unit], cntrsz[unit])) <= 0)
			break;
		maxstrlen = max_strlen_filtered_cntrnames(unit, r);
		if (autosize == 1)
			numcols = calc_num_columns();
		if(!delta_only) {
			if (json_format)
				printf("\t\t\t\t\"unit_cntrs\" : {\n");
			showcntrs(unit, NULL, cntrs[unit], r, 1);
			if (json_format)
				printf("\t\t\t\t},\n");
		}
		for(port = 0; port < nports[unit]; port++) {
			if (json_format)
				printf("\t\t\t\"port_cntrs\" : {\n");
			if(portset && port != hcaport)
				continue;
			if((r = getportcntrs(unit, port,
				portcntrs[unit][port], portcntrsz[unit])) <= 0)
				continue;
			maxstrlen = max_strlen_filtered_portcntrnames(unit, r);
			if (autosize == 1)
				numcols = calc_num_columns();
			if(!delta_only)
				showportcntrs(unit, port, NULL,
					portcntrs[unit][port], r, 1);
			if (json_format) {
				if ((port + 1) < nports[unit])
					printf("\t\t\t},\n");
				else
					printf("\t\t\t}\n");
			}
		}
		if (json_format) {
			if ((unit + 1) < nunits)
				printf("\t\t},\n");
			else
				printf("\t\t}\n");
		}
	}

	if (json_format) {
		printf("\t}\n"); /* units */
		printf("}\n"); /* Last JSON brace */
	}

	/* chosen based on -c, not on actual elapsed */
	deltafmt = nsleep.tv_nsec == 0 ? deltafmts : deltafmtms;
	numcols = 1;

	if (changes && !nret) {
		signal(SIGINT, shandle);
		signal(SIGHUP, shandle);
		signal(SIGTERM, shandle);
	}
	nozero = 1; /* for re-init, just want to show non-zero after this */
	while (changes && !nret) {	// maybe add a loop count some time
		int deltas = 0;
		struct timeval tvnow, tvlast;

		/* copy previous stats values */
		memcpy(last_stats, stats, nstats * sizeof(*stats));
		gettimeofday(&tvlast, NULL);
		nret = nanosleep(&nsleep, NULL);
		gettimeofday(&tvnow, NULL);
		udelta.tv_sec += tvnow.tv_sec - tvlast.tv_sec;
		udelta.tv_usec +=
		    (long) tvnow.tv_usec - (long) tvlast.tv_usec;
		if(udelta.tv_usec > 1000000) {
			udelta.tv_sec += udelta.tv_usec / 1000000;
			udelta.tv_usec %= 1000000;
		} else if(udelta.tv_usec < 0) {
			udelta.tv_sec--;
			udelta.tv_usec += 1000000;
		}
		if ((r = getdrstats(stats, nstats)) != -1) {
			maxstrlen = max_strlen_filtered_stats(r);
			if (autosize == 1)
				numcols = calc_num_columns();
			deltas = showstats(last_stats, stats, r);
		}

		if (json_format)
			printf("\t\t\"unit_cntrs\" : {\n");
		for(unit = 0; unit < nunits; unit++) {
			if(hcaset && unit != hca)
				continue;
			memcpy(last_cntrs[unit], cntrs[unit],
				sizeof(*cntrs) * cntrsz[unit]);
			if((r = getcntrs(unit, cntrs[unit], cntrsz[unit])) == -1)
				break;
			maxstrlen = max_strlen_filtered_cntrnames(unit, r);
			if (autosize == 1)
				numcols = calc_num_columns();

			if (json_format)
				printf("\t\t\t\"unit_cntrs\" : {\n");
			deltas = showcntrs(unit, last_cntrs[unit], cntrs[unit],
				r, deltas);
			if (json_format) {
				if ((unit + 1) < nunits)
					printf("\t\t\t},");
				else
					printf("\t\t\t}\n");
			}

			for(port = 0; port < nports[unit]; port++) {
				if(portset && port != hcaport)
					continue;
				memcpy(last_portcntrs[unit][port],
					portcntrs[unit][port],
					sizeof(**portcntrs) * portcntrsz[unit]);
				if((r = getportcntrs(unit, port,
					portcntrs[unit][port],
					portcntrsz[unit])) <= 0)
					continue;
				maxstrlen = max_strlen_filtered_portcntrnames(unit, r);
				if (autosize == 1)
					numcols = calc_num_columns();

				if (json_format)
					printf("\t\t\t\"port%d:%d\" : {\n", unit, port+1);
				showportcntrs(unit, port,
					last_portcntrs[unit][port],
					portcntrs[unit][port], r, deltas);
				if (json_format) {
					if ((port + 1) < nports[unit])
						printf("\t\t\t},\n");
					else
						printf("\t\t\t}\n");
				}
			}
		}
		if (json_format)
			printf("\t\t},\n");

		fflush(stdout);
		if(check_complained) {	/* re-init */
			if(verbose) {
				fprintf(stderr,
					"Re-initializing after error\n");
				fflush(stdout);
			}
			cleanup();
			goto re_init;
		}
	}

	return 0;
}
