# Copyright (c) 2014-2015 Intel Corporation.  All rights reserved.
# Copyright (c) 2006, 2007 QLogic Coporation. All rights reserved.
# Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc.  All rights reserved.
# Unpublished -- rights reserved under the copyright laws of the United States.
# USE OF A COPYRIGHT NOTICE DOES NOT IMPLY PUBLICATION OR DISCLOSURE.
# THIS SOFTWARE CONTAINS CONFIDENTIAL INFORMATION AND TRADE SECRETS OF
# PATHSCALE, INC.  USE, DISCLOSURE, OR REPRODUCTION IS PROHIBITED
# WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF PATHSCALE, INC.

RPM_NAME ?= hfi1-diagtools-sw
BASEVERSION := 0.1

DISTRO_NAME := $(shell if [ -e /etc/os-release ]; then sed -n '/^ID=/'p /etc/os-release | sed 's/ID=//g'; elif [ -n $(cat /etc/redhat-release | grep "6.7") ]; then  echo "rhel67"; fi)

# The desired version number comes from the most recent tag starting with "v"
VERSION := $(shell if [ -e .git ] ; then  git  describe --tags --abbrev=0 --match='v*' | sed -e 's/^v//' -e 's/-/_/'; else echo "version" ; fi)
#
# The desired release number comes the git describe following the version which
# is the number of commits since the version tag was planted suffixed by the g<commitid>
RELEASE := $(shell if [ -e .git ] ; then git describe --tags --long --match='v*' | sed -e 's/v[0-9.]*-\(.*\)/\1/' -e 's/-/_/' | sed -e 's/_g.*$$//'; else echo "release" ; fi)
#

# Concatenated version and release
VERSION_RELEASE := $(VERSION)-$(RELEASE)

GIT_VERSION := "0.8-104"
GIT_DATE := "2018-02-22 11:15:35 -0800"

# so can do, e.g. 'make SUBDIRS=util' to build just one dir
ifneq (,${BUILDING_32BIT})
# when building 32 bit, we only build the libs and test programs
SUBDIRS:= testlib tests scripts-enduser
else
#SUBDIRS:= testlib utils tests man
SUBDIRS:= testlib utils tests man scripts-enduser
endif

NOSHIP_SUBDIRS := hfidiags itp doc wfr_oem_tool
ALL_DISTROLIB := -regex \".*/lib/readline/libedit2.*\" -prune -o -regex \".*/lib/readline/rhel67.*\" -prune -o
RHEL_67LIB := -regex \".*/lib/readline/libedit2.*\" -prune -o -regex \".*/lib/readline/default.*\" -prune -o
EXCLUDE_LIB := $(shell if [[ ${DISTRO_NAME} == "rhel67" ]]; then echo ${RHEL_67LIB}; elif [[ ${DISTRO_NAME} == "rhel" ||  ${DISTRO_NAME} == "sles" ]]; then echo ${ALL_DISTROLIB}; fi)

# set reasonable defaults
export TOP := $(shell pwd)
export targ_arch := x86_64
export build_dir := $(TOP)/build/targ-$(targ_arch)
export CFLAGS += -D__DIAGTOOLS_GIT_VERSION='${GIT_VERSION}' -D__DIAGTOOLS_GIT_DATE='${GIT_DATE}'

# expect PSM to be at the same level as this directory
ifeq (,${PSMDIR})
export PSMDIR=/usr
endif

export top_builddir := $(TOP)/infinipath-install/$(targ_arch)/usr
export top_libdir := $(TOP)/infinipath-install/$(targ_arch)/$(ipath_instlibdir)

all .DEFAULT:
	for subdir in $(SUBDIRS); do \
		if [ -f $$subdir/Makefile ]; then \
			mkdir -p ${build_dir}/$$subdir; \
			$(MAKE) -C $$subdir $@ || exit 1; \
		fi ; \
	done

install: all
	for subdir in $(SUBDIRS); do \
		if [ -f $$subdir/Makefile ]; then \
			$(MAKE) -C $$subdir $@ ;\
		fi ; \
	done

install-noship:
	for subdir in $(NOSHIP_SUBDIRS); do \
		if [ -f $$subdir/Makefile ]; then \
			$(MAKE) -C $$subdir $(@:%-noship=%) ; \
		fi ; \
	done

clean:
	for subdir in $(SUBDIRS); do \
		if [ -f $$subdir/Makefile ]; then \
			$(MAKE) -C $$subdir $@ ;\
		fi ; \
	done
	rm -rf build/targ-* build/topdrivers build/drivers

package-distclean:
	rm -rf build/rpms* build/*.spec cscope.* cscope
	rm -rf $(RPM_NAME).spec
	rm -rf $(RPM_NAME)-${VERSION_RELEASE}.tar.gz

distclean: clean
	$(MAKE) package-$@
	$(MAKE) RPM_NAME=${RPM_NAME}-noship package-$@
	$(MAKE) RPM_NAME=hfidiags package-$@
	$(MAKE) RPM_NAME=hfidiags-kmod package-$@
	-git status --short -u

specfile: $(RPM_NAME).spec.in
	sed -e 's/@VERSION@/'${VERSION}'/g' \
		-e 's/@RELEASE@/'${RELEASE}'/g' ${RPM_NAME}.spec.in > \
		${RPM_NAME}.spec
	if [ -e .git ]; then \
		echo '%changelog' >> ${RPM_NAME}.spec; \
		git log --no-merges v$(BASEVERSION)..HEAD --format="* %cd <%ae>%n- %s%n" \
		| sed 's/-[0-9][0-9][0-9][0-9] //' \
		| sed 's/ [0-9][0-9]:[0-9][0-9]:[0-9][0-9]//' \
                >> ${RPM_NAME}.spec; \
        fi

git_version:
	g_version=${GIT_VERSION}; \
	g_date=${GIT_DATE}; \
	cd ${RPM_NAME}-${VERSION_RELEASE} && \
	sed -i "s/^GIT_VERSION :=.*$$/GIT_VERSION := \"$${g_version}\"/g" Makefile && \
	sed -i "s/^GIT_DATE :=.*$$/GIT_DATE := \"$${g_date}\"/g" Makefile

.PHONY: package package-noship
package-hfi1-diagtools-sw:
	mkdir -p ${RPM_NAME}-${VERSION_RELEASE}
	for x in $$(/usr/bin/find . -name ".git" -prune -o \
			-name "cscope*" -prune -o \
			-name "*.spec.in" -prune -o \
			-name "${RPM_NAME}-${VERSION_RELEASE}" -prune -o \
			-name "*.orig" -prune -o \
			-name "*~" -prune -o \
			-name "#*" -prune -o \
			-name ".gitignore" -prune -o \
			-name "hfidiags" -prune -o \
			-name "itp" -prune -o \
			-name "ui" -prune -o \
			-name "ibportstate" -prune -o \
			-name "files" -prune -o \
			-name "Specs" -prune -o \
			-name "build" -prune -o \
			-name "ib_compliance" -prune -o \
			-name "diagsgui" -prune -o \
			-name "makesrpm.sh" -prune -o \
			-name "makesdeb.sh" -prune -o \
			-name "bitfields.h" -prune -o \
			-name "iba_packet.h" -prune -o \
			-name "qib_7322_regs.h" -prune -o \
			-name "ib_qib.4" -prune -o \
			-name "reglat.c" -prune -o \
			-name "wfr_oem_tool" -prune -o \
			-print); do \
		dir=$$(dirname $$x); \
		mkdir -p ${RPM_NAME}-${VERSION_RELEASE}/$$dir; \
		[ ! -d $$x ] && cp $$x ${RPM_NAME}-${VERSION_RELEASE}/$$dir; \
	done

package-hfidiags:
	mkdir -p ${RPM_NAME}-${VERSION_RELEASE}
	for x in $$(/usr/bin/find $(NOSHIP_SUBDIRS) -name ".git" -prune -o \
			-name "cscope*" -prune -o \
			-name "*.spec.in" -prune -o \
			-name "${RPM_NAME}-${VERSION_RELEASE}" -prune -o \
			-name "*.orig" -prune -o \
			-name "*~" -prune -o \
			-name "#*" -prune -o \
			-name ".gitignore" -prune -o \
			-name "*.pyc" -prune -o \
			-regex ".*/tests/.*" -prune -o \
			${EXCLUDE_LIB} \
			-print); do \
		dir=$$(dirname $$x); \
		mkdir -p ${RPM_NAME}-${VERSION_RELEASE}/$$dir; \
		[ ! -d $$x ] && cp $$x ${RPM_NAME}-${VERSION_RELEASE}/$$dir; \
	done
	cp ${RPM_NAME}.spec ${RPM_NAME}-${VERSION_RELEASE}
	cp Makefile ${RPM_NAME}-${VERSION_RELEASE}

package-hfidiags-kmod:
	mkdir -p ${RPM_NAME}-${VERSION_RELEASE}
	cp -a ui/* ${RPM_NAME}-${VERSION_RELEASE}
	cp ${RPM_NAME}.spec ${RPM_NAME}-${VERSION_RELEASE}

generic-dist: package-distclean specfile package-$(RPM_NAME) git_version
	if [ -e .git ] ; then \
		git log -n1 --pretty=format:%H > ${RPM_NAME}-${VERSION_RELEASE}/COMMIT ; \
	fi
	tar czvf ${RPM_NAME}-${VERSION_RELEASE}.tar.gz ${RPM_NAME}-${VERSION_RELEASE} \
		--transform=s,hfidiags/debian,debian,
	rm -rf ${RPM_NAME}-${VERSION_RELEASE}

dist:
	$(MAKE) generic-$@
	$(MAKE) RPM_NAME=hfidiags generic-$@
	$(MAKE) RPM_NAME=hfidiags-kmod generic-$@

# rebuild the cscope database, skipping sccs files, done once for
# top level
cscope:
	find * -type f ! -name '[ps].*' \( -iname '*.[cfhs]' -o \
	  -iname \\*.cc -o -name \\*.cpp -o -name \\*.f90 \) -print | cscope -bqu -i -

.PHONY: $(SUBDIRS)

