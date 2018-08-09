# Copyright (c) 2014 Intel Corporation.  All rights reserved.
Summary: Intel HFI1 User Tools
Name: hfi1-diagtools-sw
Version: 0.8
Release: 101
License: GPL or BSD
Group: System Environment/Base
URL: http://www.intel.com/
Source0: %{name}-%{version}-%{release}.tar.gz
Prefix: /usr
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: libpsm2-devel

Requires: libpsm2

%description
User utilities for the Intel Host Fabric Interface

%prep
%setup -q -n %{name}-%{version}-%{release}

%build
%{__make}

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
export DESTDIR=$RPM_BUILD_ROOT

%{__make} install
mkdir -p $RPM_BUILD_ROOT/usr/bin

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/bin/hfi1_pkt_test
/usr/bin/hfi1_pkt_send
/usr/bin/hfi1_control
/usr/bin/hfi1stats
/usr/share/man/man8/hfi1_pkt_test.8.gz
/usr/share/man/man8/hfi1_pkt_send.8.gz
/usr/share/man/man8/hfi1_control.8.gz
/usr/share/man/man8/hfi1stats.8.gz
/usr/share/man/man5/hfi_pkt_files.5.gz
/usr/share/man/man4/hfi1.4.gz
/usr/share/hfi-diagtools-sw/test_packets

%changelog
* Mon Feb 5 2018 <niroop.gade@intel.com>
- hfi1_eprom: Remove hfi1_eprom from hfi1-diagtools-sw package

* Thu Jan 25 2018 <mitko.haralanov@intel.com>
- hfidiags: Fix check for unit index on multi-unit systems

* Mon Dec 11 2017 <john.fleck@intel.com>
- hfi1_control: Fix minor spelling error

* Mon Nov 20 2017 +0100 <andrzej.kacprowski@intel.com>
- hfi1_eprom: Add update -u option and hide dangerous ops

* Mon Nov 6 2017 <tymoteusz.kielan@intel.com>
- fixup! Update wfr-diagtools-sw Ubuntu build artifacts for IFS 10.7

* Mon Nov 6 2017 <arkadiusz.palinski@intel.com>
- wfr_oem_tool: Fix klokwork issues

* Mon Nov 6 2017 <tymoteusz.kielan@intel.com>
- Update wfr-diagtools-sw Ubuntu build artifacts for IFS 10.7

* Thu Nov 2 2017 <sebastian.sanchez@intel.com>
- hfidiags: Fix device list creation for multi-HFI systems

* Tue Oct 24 2017 <jay.p.patel@intel.com>
- hfi1_eprom: Validate array index before accessing pci_device_addrs array

* Wed Oct 18 2017 <ira.weiny@intel.com>
- hfi1_eprom: Add optional meta data print

* Wed Oct 18 2017 <ira.weiny@intel.com>
- hfi1_eprom: Decode v4 of platform config format

* Fri Oct 13 2017 +0200 <tymoteusz.kielan@intel.com>
- hfi1_eprom: Fix warning of possible string truncation

* Thu Oct 12 2017 <ira.weiny@intel.com>
- hfi1_eprom: Add warning/confirmation of write operations

* Thu Oct 12 2017 <ira.weiny@intel.com>
- hfi1_eprom: Add Partition #defines for use in later patches

* Thu Oct 12 2017 <ira.weiny@intel.com>
- hfi1_eprom: Add operation string function

* Fri Sep 1 2017 <grzegorz.morys@intel.com>
- hfi1_eprom: separate files for collective reads

* Fri Sep 1 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: help message update

* Wed Aug 23 2017 <jay.p.patel@intel.com>
- hfidiags: Appropriate packaging of readline library

* Fri Aug 18 2017 <mitko.haralanov@intel.com>
- hfidiags: Use correct comparison when checking resource range

* Fri Aug 18 2017 <mitko.haralanov@intel.com>
- hfidiags: Add send buffers to memory maps

* Thu Aug 10 2017 <mitko.haralanov@intel.com>
- hfidiags: 'info' command correctly decodes CSR addresses

* Tue Aug 1 2017 <grzegorz.morys@intel.com>
- hfidiags: fix device selection

* Tue Jul 25 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: clear progress message

* Tue Jul 25 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: select device by index

* Mon Jul 24 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: print device identifier

* Fri Jul 14 2017 <john.fleck@intel.com>
- hfi1_control: fix opa version string location

* Thu Jul 13 2017 <mitko.haralanov@intel.com>
- hfidiags: Break resource mappings into sections

* Wed Jul 12 2017 <navinx.soni@intel.com>
- Modifications in hfidiags due to readline library incompatibility.

* Tue Jul 11 2017 +0200 <grzegorz.morys@intel.com>
- hfidiags: fix parsing of PCI ID numbers

* Wed Jul 5 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: build fix for old glibc

* Tue Jul 4 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: device selection enhancements

* Mon Jul 3 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: progress reporting

* Thu Jun 29 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: fix arguments parsing

* Mon Jun 26 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: print version for all EPROM components

* Mon Jun 26 2017 +0200 <grzegorz.morys@intel.com>
- hfi1_eprom: remove unnecessary help text print

* Mon Jun 26 2017 <grzegorz.morys@intel.com>
- hfi1_eprom: add tool version information

* Wed Jun 21 2017 <bartlomiej.dudek@intel.com>
- hfi1_control: Fix errors regarding SIGINT occurence

* Thu Jun 15 2017 <harish.chegondi@intel.com>
- Add /usr/include/uapi to the list of include directories to search headers

* Wed Jun 7 2017 +0200 <bartlomiej.dudek@intel.com>
- hfi1_control: Support rdmavt trace events

* Tue May 23 2017 +0200 <andrzej.kacprowski@intel.com>
- Add support for Spansion EEPROM in hfi1_eprom tool

* Mon May 22 2017 <jakub.byczkowski@intel.com>
- hfi1_control: Use pci.ids to read board description

* Thu May 18 2017 +0200 <andrzej.kacprowski@intel.com>
- Add support for Micron devices in hfi1_eprom tool

* Thu May 11 2017 <sebastian.sanchez@intel.com>
- hfi1_eprom: Use the last instance of magic bits on read operations

* Thu May 11 2017 <sebastian.sanchez@intel.com>
- Revert "hfi1_eprom: Read and write the new EPROM file format"

* Fri Apr 21 2017 <tymoteusz.kielan@intel.com>
- hfidiags/hfi_control: Fix manpage grammar

* Fri Apr 21 2017 <tymoteusz.kielan@intel.com>
- hfidiags: fix running on Ubuntu

* Tue Apr 18 2017 <easwar.hariharan@intel.com>
- wfr_oem_tool: Delete dead 'emitheaders' code

* Thu Apr 13 2017 <jakub.byczkowski@intel.com>
- hfi1_eprom: Fix error information in 'no HFI discrete card' scenario

* Mon Apr 10 2017 +0200 <tymoteusz.kielan@intel.com>
- Add Ubuntu packaging

* Fri Apr 7 2017 <jakub.byczkowski@intel.com>
- hfi1_eprom: Remove deprecated driver accessor method.

* Thu Mar 30 2017 <jakub.byczkowski@intel.com>
- hfi1_eprom: Rework default resource file and enable file logic

* Thu Mar 9 2017 <jakub.byczkowski@intel.com>
- hfi1_eprom: Add output information for -V argument in case of missing partition.

* Fri Feb 24 2017 <neerajx.jain@intel.com>
- hfi1stats: Memory out of bounds for counter name

* Thu Feb 16 2017 <michael.j.ruhl@intel.com>
- hfi1_pkt_send: hfi1_pkt_test: Incorrect command used for setting hfi_cmd_write().

* Tue Jan 31 2017 <easwar.hariharan@intel.com>
- hfi1_eprom: Remove deprecated access mechanisms from usage and man text

* Wed Dec 21 2016 <neerajx.jain@intel.com>
- hfi1stats: Uninitialized variable causing compilation error

* Wed Dec 14 2016 <easwar.hariharan@intel.com>
- Remove hfidiags dependency from hfi1-tools-oem RPM

* Tue Nov 22 2016 <sebastian.sanchez@intel.com>
- hfi1stats: Add JSON output support

* Fri Nov 11 2016 <dean.luick@intel.com>
- eprom_unit_test: remove eprom_test.in

* Wed Nov 9 2016 <jianxin.xiong@intel.com>
- hfi1stats: Allow non-root users to access the statistics counters

* Tue Nov 8 2016 <dean.luick@intel.com>
- hfi1_eprom: Read and write the new EPROM file format

* Tue Oct 25 2016 <mitko.haralanov@intel.com>
- hfidiags: Use debugfs lcb file for LCB CSR access

* Fri Oct 14 2016 <dean.luick@intel.com>
- hfidiags: Read dc8051 memory from debugfs

* Fri Oct 14 2016 <dean.luick@intel.com>
- hfidiags: Make Resource variables per instance

* Fri Oct 14 2016 <dean.luick@intel.com>
- hfidiags: Consolidate Resource class methods "map" and "open"

* Tue Sep 27 2016 <ofed@ph-gbld1-host-01.ph.intel.com>
- Branch of  09/27/16 14:19 Parent Tag: master Branch Tag: opa-10_3 Anchor Tag: opa-10_3-anchor

* Tue Sep 27 2016 <dennis.dalessandro@intel.com>
- Make eprom unit test flexible as to executable

* Mon Sep 26 2016 <dean.luick@intel.com>
- hfi1_eprom: Correct mmap length to a full CSR length

* Wed Sep 21 2016 <dennis.dalessandro@intel.com>
- hfi_utils: Remove conflicting pacakge

* Mon Sep 19 2016 <dean.luick@intel.com>
- eprom_unit_test: Clean files after unit test

* Mon Sep 19 2016 <dean.luick@intel.com>
- hfi1_eprom: Exit with an error when mmap fails

* Mon Sep 19 2016 <dean.luick@intel.com>
- man: Remove man Makefile circular dependency

* Thu Sep 8 2016 <sebastian.sanchez@intel.com>
- hfidiags: Pass non-readable register list to parsing code

* Wed Aug 24 2016 <easwar.hariharan@intel.com>
- wfr_oem_tool: Do not byte swap text field in bin2text operation

* Tue Aug 16 2016 <sebastian.sanchez@intel.com>
- hfi1_eprom: Enable device when needed

* Fri Aug 5 2016 <sebastian.sanchez@intel.com>
- hfidiags: Add "do not read" field to registers

* Tue Aug 2 2016 <sebastian.sanchez@intel.com>
- hfidiags: Prevent hfidiags from running when driver is not loaded

* Fri Jul 29 2016 <sebastian.sanchez@intel.com>
- hfidiags: Fix diags_db_gen initialization

* Wed Jul 13 2016 <john.s.summers@intel.com>
- Add magic bits to the end of images written using hfi1_eprom

* Thu Jun 30 2016 <mitko.haralanov@intel.com>
- hfidiags: Use resource0 CSR interface

* Thu Jun 30 2016 <paul.j.reger@intel.com>
- hfi_pkt_send/test: Handle removal of hfi1_alg struct member.

* Tue Jun 28 2016 <dennis.dalessandro@intel.com>
- hfi1_eprom: Add a unit test script

* Tue Jun 21 2016 <dennis.dalessandro@intel.com>
- hfi1_eprom: Convert to using resource0

* Thu Jun 9 2016 <dennis.dalessandro@intel.com>
- hfi1_eprom: Update copyright

* Fri May 13 2016 <john.fleck@intel.com>
- hfi1-diagtools-sw.spec.in: Update reference from hfi-psm to libpsm2

* Sat May 7 2016 <dennis.dalessandro@intel.com>
- Remove man2html from makefile

* Fri Apr 29 2016 <dean.luick@intel.com>
- hfi1_eprom: Add ability to display versions

* Fri Apr 29 2016 <dean.luick@intel.com>
- wfr_oem_tool: Do not byte swap a text field

* Wed Apr 27 2016 <mitko.haralanov@intel.com>
- hfidiags: Fix bug with iterating over a list

* Mon Apr 11 2016 +0200 <jakub.pawlak@intel.com>
- hfi1_control: Change description of the serial number field

* Tue Mar 29 2016 <sebastian.sanchez@intel.com>
- hfidiags: Fix for printing bitfields

* Fri Mar 18 2016 <jianxin.xiong@intel.com>
- hfi1_pkt_test: fix definition of RHF error flags

* Mon Mar 14 2016 <sebastian.sanchez@intel.com>
- hfi1_pkt_test/hfi1_pkt_send: Set P_KEY before sending any packets

* Mon Mar 7 2016 <dean.luick@intel.com>
- ui: Correct PCI Device Id for -F devices

* Wed Mar 2 2016 <sebastian.sanchez@intel.com>
- hfidiags: Fix for case sensitivity for "state switch" command

* Wed Mar 2 2016 <easwar.hariharan@intel.com>
- hfi1_control: Remove unsupported -b option from usage and getopts

* Tue Mar 1 2016 <sebastian.sanchez@intel.com>
- hfi1stats: Fix for segfault when exclusion flag is used

* Wed Feb 10 2016 <sebastian.sanchez@intel.com>
- hfi1_control: -v flag removal

* Wed Jan 20 2016 <john.fleck@intel.com>
- hfidiags: Add LICENSE files to the hfidiags and hfi1-tools-oem RPMs

* Tue Jan 19 2016 <mitko.haralanov@intel.com>
- hfidiags: Handle all outstanding pre-PRQ requests

* Tue Jan 5 2016 <vennila.megavannan@intel.com>
- Makefile: Exclude wfr-oem_tools source from RPMs that will be shipped

* Thu Dec 17 2015 <vennila.megavannan@intel.com>
- hfi1_control: make changes to hfi1_control documentation

* Mon Dec 7 2015 <vennila.megavannan@intel.com>
- hfi1stats: make changes to hfi1stats man page

* Wed Nov 18 2015 <sebastian.sanchez@intel.com>
- hfi1stats: Fix for 32-bit counter overflow in driver and hfi1stats

* Tue Nov 17 2015 <dean.luick@intel.com>
- wfr_oem_tool: Add stdafx.cpp copyright

* Wed Nov 11 2015 <dean.luick@intel.com>
- wfr_oem_tool: Add wfr_oem_tool to diagtools RPM

* Tue Nov 10 2015 <mitko.haralanov@intel.com>
- hfidiags: Remove kernel-devel requirement from SPEC file

* Tue Nov 10 2015 <easwar.hariharan@intel.com>
- wfr_oem_tool: Clean up unwanted code and fix up for functionality

* Fri Nov 6 2015 <easwar.hariharan@intel.com>
- wfr_oem_tool: Import supporting files from PrrOemTool

* Fri Oct 16 2015 <easwar.hariharan@intel.com>
- wfr_oem_tool: Initial import from CVS

* Wed Oct 14 2015 <ignacio.hernandez@intel.com>
- hfidiags: Add support for B1 stepping

* Fri Oct 9 2015 <harish.chegondi@intel.com>
- hfi1diags: Fix decoding of little endian structures PBC, RHF and SDMADESC

* Fri Oct 9 2015 <joel.b.rosenzweig@intel.com>
- hfi1stats: Dynamically fits columns to fit the screen + new filters

* Mon Oct 5 2015 <kaike.wan@intel.com>
- hfi1_pkt_test: wide variations in buffer copy test reported

* Mon Sep 28 2015 <vennila.megavannan@intel.com>
- Add options to help user generate proper cap mask values

* Wed Sep 23 2015 <dean.luick@intel.com>
- hfi1_control: print out board_id

* Fri Sep 18 2015 <vennila.megavannan@intel.com>
- klocwork issues in wfr-diagtools-sw repo

* Thu Sep 17 2015 <vennila.megavannan@intel.com>
- Filtering of hfi1stats info

* Wed Sep 16 2015 <dean.luick@intel.com>
- hfi1_eprom: partition changes

* Wed Sep 16 2015 <mike.marciniszyn@intel.com>
- Add cnp packet test

* Tue Sep 15 2015 <dean.luick@intel.com>
- Change names: remove WFR, rename HFI

* Thu Sep 10 2015 <vennila.megavannan@intel.com>
- hfi1_control -iv gives unhelpful software version information

* Tue Sep 8 2015 <mark.f.brown@intel.com>
- Replaced Qlogic references in man pages with Intel

* Thu Sep 3 2015 <paul.j.reger@intel.com>
- Fix build breakage due to my changes in opa_debug.h from wfr-psm git repo.

* Mon Aug 31 2015 <alex.estrin@intel.com>
- hfi1-utils: remove udev rules for /dev/hfi1.

* Wed Aug 26 2015 <vennila.megavannan@intel.com>
- hfi1_control -iv does not give correct info about the link status

* Thu Aug 20 2015 <sadanand.warrier@intel.com>
- Removed incorrect string at the end of port info

* Fri Aug 7 2015 <mike.marciniszyn@intel.com>
- Add back in driver version

* Wed Aug 5 2015 <sadanand.warrier@intel.com>
- Remove references to removed sysfs files in hfi1_control

* Tue Aug 4 2015 <mitko.haralanov@intel.com>
- Update source RPM and copyrights/licenses

* Mon Aug 3 2015 <jareer.h.abdel-qader@intel.com>
- Add driver srcversion to hfi1_control

* Fri Jul 31 2015 <easwar.hariharan@intel.com>
- IB/hfi1: Fix hfi1_control after removal of ENABLE_SMA bit

* Thu Jul 30 2015 <john.fleck@intel.com>
- hfi1-diagtools-sw: remove requirement for hfi1-psm-devel-noship

* Thu Jul 16 2015 <adam.goldman@intel.com>
- hfi1_pkt_send: Fix when sending packets through kernel interface

* Tue Jul 14 2015 <vennila.megavannan@intel.com>
- Update hfi tools and scripts with KNL-F device id

* Tue Jul 14 2015 <christian.gomez@intel.com>
- hfidiags: Prompt doesn't follow ASIC naming convention

* Mon Jul 13 2015 <mitko.haralanov@intel.com>
- hfi1_control: Fix error with parsing capability bits

* Fri Jul 10 2015 <mitko.haralanov@intel.com>
- hfidiags: Add copyright/license text to hfidiags files

* Tue Jun 30 2015 <mitko.haralanov@intel.com>
- hfidiags: Prepare hfidiags for distribution

* Fri Jun 26 2015 <mitko.haralanov@intel.com>
- hfidiags: Extended 'state' command for DC8051 memory

* Mon Jun 22 2015 <john.fleck@intel.com>
- Change -d reference in makefile to be -e. For STL2, the teamforge repo will host what is in CVS currently. Rather than using an import method to get external repos, we will use git submodules to track external repos. The catch is the .git are files in the submodules. To build, we need to change the makefile to look for the existence of the .git (-e) rather than the existence of the directory .git (-d)

* Thu Jun 18 2015 <mitko.haralanov@intel.com>
- Update hfi1_control man page

* Thu Jun 4 2015 <mitko.haralanov@intel.com>
- hfidiags: Fix handling of 'state' command arguments

* Thu Jun 4 2015 <easwar.hariharan@intel.com>
- diags: Fix hfi1_control to address correct sysfs files

* Wed Jun 3 2015 <kaike.wan@intel.com>
- Fix git_version.h when making srpm

* Fri May 29 2015 <alex.estrin@intel.com>
- hfi-diagtools: remove extra path to exported hfi1_user.h

* Wed May 13 2015 <dennis.dalessandro@intel.com>
- Fix build breakage due to header file changes.

* Mon May 4 2015 <mike.marciniszyn@intel.com>
- diags: handle rename of ipath_user.h to opa_user.h

* Wed Apr 22 2015 <mitko.haralanov@intel.com>
- Fix the example script

* Tue Apr 21 2015 <mitko.haralanov@intel.com>
- Fix CSR namespace for "common" commands

* Mon Apr 20 2015 <mitko.haralanov@intel.com>
- Fix bad TTY behavior with libedit

* Mon Apr 20 2015 <mitko.haralanov@intel.com>
- Correctly display chip revision and implementation

* Mon Apr 20 2015 <dennis.dalessandro@intel.com>
- Fix path that hfi1_pkt_send uses to determine lid.

* Fri Apr 17 2015 <mitko.haralanov@intel.com>
- Replace libreadline with libedit

* Wed Apr 15 2015 <alex.estrin@intel.com>
- hfi-diagtools: add location path to exported hfi1_user.h

* Fri Apr 3 2015 <dennis.dalessandro@intel.com>
- Handle hfi to hfi1 name updates.

* Wed Mar 25 2015 <mitko.haralanov@intel.com>
- Enable harness on WFR A0 HW

* Tue Mar 24 2015 <mitko.haralanov@intel.com>
- Allow CSR validation based on namespace

* Wed Mar 18 2015 <mitko.haralanov@intel.com>
- Improve command aliasing scheme

* Tue Mar 17 2015 <mitko.haralanov@intel.com>
- Improve output of 'status' command

* Mon Mar 16 2015 <mitko.haralanov@intel.com>
- Fix bug with removal of feature bits

* Mon Mar 9 2015 <todd.rimmer@intel.com>
- diags: add makesrpm.sh to allow simplication of build processes

* Fri Feb 27 2015 <mitko.haralanov@intel.com>
- Add abitlity to control driver features

* Thu Feb 26 2015 <dennis.dalessandro@intel.com>
- Remove reading of Infiniband sys fs file for link speed.

* Wed Feb 18 2015 <dean.luick@intel.com>
- IB/hfi: Recognize more EPROM IDs

* Tue Feb 10 2015 <andrew.friedley@intel.com>
- Fix 10b and 16b Length field size

* Tue Feb 10 2015 <mitko.haralanov@intel.com>
- Use 'setpci' to read/write PCI registers

* Thu Feb 5 2015 <mitko.haralanov@intel.com>
- Generate CSR databases based on configurable sets

* Wed Feb 4 2015 <mitko.haralanov@intel.com>
- Send output to the proper streams

* Tue Feb 3 2015 <mitko.haralanov@intel.com>
- Update hfidiags documentation

* Tue Feb 3 2015 <mitko.haralanov@intel.com>
- Re-work output handling

* Mon Feb 2 2015 <mitko.haralanov@intel.com>
- Properly find the header in the RcvHdrQ entry

* Mon Feb 2 2015 <mitko.haralanov@intel.com>
- Use correct global names in 'status' command

* Fri Jan 30 2015 <andrew.friedley@intel.com>
- Create hfi-utils RPM

* Tue Jan 27 2015 <cq.tang@intel.com>
- Change PSM/driver API

* Mon Jan 26 2015 <mitko.haralanov@intel.com>
- Add support for PCIe config space access

* Mon Jan 26 2015 <mitko.haralanov@intel.com>
- Fix issues with hfi-diagtools-sw-noship RPM

* Thu Jan 15 2015 <mitko.haralanov@intel.com>
- Include CSR field access attributes

* Wed Jan 14 2015 <cq.tang@intel.com>
- Change to use new PSM/drive API.

* Mon Jan 12 2015 <mitko.haralanov@intel.com>
- Add a warning about JTAG HW and state captures

* Fri Jan 9 2015 <mitko.haralanov@intel.com>
- Add updated A0 and new B0 CSR databases

* Thu Jan 8 2015 <mitko.haralanov@intel.com>
- Accurately compute values of "complex" defs

* Tue Jan 6 2015 <mitko.haralanov@intel.com>
- Split RPMs into "ship" and "noship"

* Fri Dec 19 2014 <mitko.haralanov@intel.com>
- Allow use of databases for different HW revisions

* Fri Dec 19 2014 <mitko.haralanov@intel.com>
- Fix error when running regression on FPGAs

* Tue Dec 16 2014 <mitko.haralanov@intel.com>
- Fix database and state dump hashes

* Mon Dec 15 2014 <mike.marciniszyn@intel.com>
- Fix RHEL7 build issues with hfistats, hfi_pkt_send

* Fri Dec 12 2014 <mitko.haralanov@intel.com>
- ITP: Add ITP support

* Thu Dec 11 2014 <mitko.haralanov@intel.com>
- Fix traceback when Ctrl-C is given

* Thu Dec 11 2014 <andrew.friedley@intel.com>
- Allow hfi_pkt_test to send to context 0.

* Wed Dec 10 2014 <mitko.haralanov@intel.com>
- Fix address shown by mempeek output

* Wed Dec 10 2014 <mitko.haralanov@intel.com>
- Better command help formatting

* Tue Dec 9 2014 <mitko.haralanov@intel.com>
- Don't show CSRs with value of 0

* Tue Dec 9 2014 <dean.luick@intel.com>
- IB/hfi: Remove dual-load code in hfidiags

* Mon Dec 8 2014 <mitko.haralanov@intel.com>
- Add mempeek command

* Tue Dec 2 2014 <mike.marciniszyn@intel.com>
- Fix build issue with gcc 4.8.2.

* Thu Nov 13 2014 <mitko.haralanov@intel.com>
- Fix script mode bug

* Wed Oct 29 2014 <mike.marciniszyn@intel.com>
- Correct hfistats man pages issues

* Thu Oct 23 2014 <mitko.haralanov@intel.com>
- Detect whether the HFI module is loaded

* Mon Oct 20 2014 <dean.luick@intel.com>
- Add wait option to hfi_pkt_send

* Fri Oct 17 2014 <mitko.haralanov@intel.com>
- Fix header/structure element indexing

* Fri Oct 17 2014 <mitko.haralanov@intel.com>
- Make "watch" command more intelligent

* Thu Oct 16 2014 <mitko.haralanov@intel.com>
- Add SDMA AHG descriptor decoding

* Thu Oct 16 2014 <andrew.friedley@intel.com>
- Print warning when value would be truncated

* Wed Oct 15 2014 <dean.luick@intel.com>
- Add hfi_eprom

* Mon Oct 13 2014 <mitko.haralanov@intel.com>
- Add Hfidiags scripting example

* Mon Oct 13 2014 <mitko.haralanov@intel.com>
- Re-work structures to move away from Dwords

* Tue Oct 7 2014 <dean.luick@intel.com>
- Change hfidiags to look for hfi_ui0 and hfi0_ui

* Tue Oct 7 2014 <dean.luick@intel.com>
- Add urgent wait option and test packet

* Thu Oct 2 2014 <mitko.haralanov@intel.com>
- More informative command prompt

* Wed Oct 1 2014 <mitko.haralanov@intel.com>
- Import UI driver code

* Mon Sep 22 2014 <mitko.haralanov@intel.com>
- Accept floating point watch intervals

* Mon Sep 22 2014 <mitko.haralanov@intel.com>
- Change LRH.VL to LRH.SC as per WFR HAS

* Mon Sep 22 2014 <mitko.haralanov@intel.com>
- Fix crash of hfidiags when HW in freeze or pause

* Thu Sep 18 2014 <mitko.haralanov@intel.com>
- Add "watch" command

* Wed Sep 17 2014 <mitko.haralanov@intel.com>
- Fix logging warning message

* Wed Sep 17 2014 <mitko.haralanov@intel.com>
- Rework handling of Ctrl-C events

* Mon Sep 15 2014 <mitko.haralanov@intel.com>
- Add tracing enable/disable capabilities to hfi_control

* Thu Sep 11 2014 <mitko.haralanov@intel.com>
- Report read/write errors

* Thu Sep 11 2014 <mitko.haralanov@intel.com>
- Fix main control loop event handling for Ctrl-C

* Tue Aug 19 2014 <mitko.haralanov@intel.com>
- Add logging support to hfidiags

* Thu Aug 14 2014 <mitko.haralanov@intel.com>
- Add SDma Descriptor decoding

* Mon Aug 11 2014 <andrew.friedley@intel.com>
- Abort if -k is specified and diagpkt is unusable

* Thu Aug 7 2014 <andrew.friedley@intel.com>
- Show bypass quadword padding and allow padding to be disabled.

* Thu Aug 7 2014 <cq.tang@intel.com>
- Fixed build issue after PSM PBC shift/mask changes

* Fri Aug 1 2014 <mitko.haralanov@intel.com>
- Fix hfidiags crash at startup with no HW

* Thu Jul 31 2014 <mitko.haralanov@intel.com>
- Remove HSD290940 workaround

* Wed Jul 30 2014 <andrew.friedley@intel.com>
- Fix JKEY and PKEY handling in hfi_pkt_test

* Tue Jul 29 2014 <andrew.friedley@intel.com>
- Fix hfi_pkt_test build due to PSM KDETH changes

* Tue Jul 29 2014 <andrew.friedley@intel.com>
- Update hfi_pkt_files manpage for PBC macro

* Thu Jul 24 2014 <mitko.haralanov@intel.com>
- Handle keyboard interrupts correctly

* Wed Jul 23 2014 <mitko.haralanov@intel.com>
- Add support for register name wildcards

* Mon Jul 21 2014 <mitko.haralanov@intel.com>
- Update hfidiags documentation

* Fri Jul 18 2014 <mitko.haralanov@intel.com>
- Fix detection of terminal size

* Thu Jul 17 2014 <mitko.haralanov@intel.com>
- WFR Flop Reduction and Security Updates

* Wed Jul 2 2014 <mitko.haralanov@intel.com>
- Add paging UI option

* Wed Jul 2 2014 <andrew.friedley@intel.com>
- Add PBC macro; honor VL when LRH not specified

* Mon Jun 30 2014 <mitko.haralanov@intel.com>
- Print implementation revision in hex

* Fri Jun 27 2014 <mitko.haralanov@intel.com>
- Fix a bug with zero-filling structure values

* Mon Jun 23 2014 <mitko.haralanov@intel.com>
- Add support for writing to hardware by using addresses

* Mon Jun 23 2014 <mike.marciniszyn@intel.com>
- diags: correct build issue on clean repo

* Fri Jun 20 2014 <andrew.friedley@intel.com>
- Add version information to C-based tools

* Tue Jun 17 2014 <mitko.haralanov@intel.com>
- Improve performance of state compares

* Thu Jun 12 2014 <mitko.haralanov@intel.com>
- Add a test harness for the hfidiags

* Wed Jun 11 2014 <mitko.haralanov@intel.com>
- Allow for use of variables in filenames

* Wed Jun 11 2014 <mitko.haralanov@intel.com>
- Add more support for symbolic register indexes

* Tue Jun 10 2014 <mitko.haralanov@intel.com>
- Fix XML data parsing for registers which end in numbers

* Mon Jun 9 2014 <mitko.haralanov@intel.com>
- Add UI property for interactive mode

* Mon Jun 9 2014 <mitko.haralanov@intel.com>
- UI can accept "comment" lines

* Fri Jun 6 2014 <mitko.haralanov@intel.com>
- Fix a crash in the 'info' command

* Fri Jun 6 2014 <mitko.haralanov@intel.com>
- Fix a UI crash with unknown options

* Fri Jun 6 2014 <andrew.friedley@intel.com>
- Check and disable receive check when using diagpkt

* Thu Jun 5 2014 <mitko.haralanov@intel.com>
- Add hfidiags documentation

* Thu Jun 5 2014 <mitko.haralanov@intel.com>
- Accept commands from file.

* Thu Jun 5 2014 <mitko.haralanov@intel.com>
- Fix a set of UI bugs.

* Thu Jun 5 2014 <mitko.haralanov@intel.com>
- Update to v40 CSR definitions.

* Tue Jun 3 2014 <mitko.haralanov@intel.com>
- Fix issues left from re-org of modules.

* Fri May 30 2014 <andrew.friedley@intel.com>
- Fix trailing whitespace.

* Fri May 30 2014 <andrew.friedley@intel.com>
- Rework VL and SL handling in hfi_pkt_send.

* Fri May 30 2014 <mike.marciniszyn@intel.com>
- Correct spec file issue

* Thu May 29 2014 <mitko.haralanov@intel.com>
- Add copyright notices to all files. Move Python version functions to the library file so it can be used by all files that need them.

* Thu May 22 2014 <mitko.haralanov@intel.com>
- Add support for parsing and displaying WFR headers.

* Wed May 21 2014 <mike.marciniszyn@intel.com>
- diag: adapt to psm2 changes

* Tue May 20 2014 <mitko.haralanov@intel.com>
- Re-organize the files into a package and data subdirectory. This change also renames some of the files in order to make their purpose a bit clearer.

* Sat May 17 2014 <mike.marciniszyn@intel.com>
- diags: fix drop4 build issues

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Register fields were printed with one too many hex digits when "show_64bit_fields" was set to True.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- This change attempts to add a type of versioning between HW data and stored/saved states.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- The XML and header parsing code depends on Python features which were not part of Python until version 2.7.4. Therefore, we have to make sure that we run on the correct version instead of crashing the app.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Update HW data to version .38

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Rework some of the state capture/compare code so it is a little more robust and there is better seperation between captured and loaded states.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Better handling of new lines in command output.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Add a path completion fuction. Currently, used by the "state load|save" commands.

* Fri May 16 2014 <mitko.haralanov@intel.com>
- Add support for capturing, exporting, importing, and using complete hardware states.

* Mon May 12 2014 <mitko.haralanov@intel.com>
- Correctly prefix the first line of output with tablen even if the line is shorter than the width of the console.

* Mon May 12 2014 <mitko.haralanov@intel.com>
- Fix "decode" command by passing the HW object so it can properly process CSR names. Fix "status" command by using proper CSR names.

* Mon May 12 2014 <arthur.kepner@intel.com>
- Rename 'ipathstats' to 'hfistats'. Also, update the 'mntpnt' variable within hfistats.c so that it points to the correct place within debugfs.

* Mon May 12 2014 <andrew.friedley@intel.com>
- Fix compile mlid compile error.

* Mon May 12 2014 <andrew.friedley@intel.com>
- Resize LID variables from 16 to 32 bits.

* Fri May 9 2014 <root@iqa-130.sc.intel.com>
- build ipathstats, and place it in hfi-diagtools-sw-noship tarball

* Fri May 9 2014 <andrew.friedley@intel.com>
- Fix hfi_pkt_files.5 man page documentation error.

* Wed May 7 2014 <andrew.friedley@intel.com>
- Bug fixes for hfi_pkt_send.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix "decode" command by passing the HW object so it can properly process CSR names. Fix "status" command by using proper CSR names.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add the "show_register_bitfields" UI option.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add support for simple UI configuration options. These options are not persistent across restarts.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add output paging support and help text explaining the register specification as command arguments.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add "reverse" register lookup for the info command. This allows the user to get a register definition based on address instead of name, i.e. "info 0x0x1303820".

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add support for symbolic register indexes. Currently, this is only meaningful for counter registers. However, the tool does not have any special knowledge about those registers and will happily parse and process symbolic indexes for any register.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Display a warning message on startup if the chip is any Pause or Freeze modes.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix displaying of the register indexes depending on whether the registers are scalar values, one-, or two-dimensional arrays.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Remove bitfield bit offsets from register names and move them to their own field in the register output.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Allow Ctrl-C to interrupt a running command without exiting the application. Ctrl-C at the prompt will still exit the application.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Update register definition to .37 release.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Add a list of to-do items.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Implement proper handling of context register arrays as well as memory arrays. This changeset introduces a new register spec format: <register name>[<ctxt>][<index>].<bitfield> The <ctxt>, <index>, and <bitfield> fields are all optional. When <ctxt> and <index> are missing, the commands will operate on the complete range for the corresponding field. For example: read SendCtxtCtrl will read and display the values of SendCtxtCtrl register for all contexts. Supported formats for the <ctxt> and <index> fields are a single number, a range ("<start>-<end>"), or "*" for the entire range.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix issues with using binary mode when opening the WFR device file - Python 2.6 does not like the binary mode for the device file. Fix crash with printing return value from register read (if reading only 8bytes).

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix bulk memory reads when the size read is only 8 bytes. Add support for default size (8 bytes) when it is omitted.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Hand over the path where the executible is located to the HW class so it can find its data file.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Ignore Python byte-compiled files

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Rename file "commands/defaults.py" to "commands/default.py".

* Wed May 7 2014 <mitko.haralanov@intel.com>
- The 'unit' command would throw an exception if the user did not specify a unit number. It also failed to catch the HwError exception in the case that the unit open call failed.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- The regular expression based register spec parsing did not catch malformed register specs. Re-implement the parsing routine without the use of regular expressions. This give it much better control over the format of the spec.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix a copy-and-paste error.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- XML file do not contain "global" chip definitions (number of send/rcv contexts, number of tid flow tables, etc.) On the other hand the header files do contain that information. Therefore, we have to parse both to get a complete view of the chip.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- Fix an issue with showing information for non-zero index of registers from register arrays. The 'info' command now supports proper register array indexing.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- * Add support for parsing the HW data from the source XML files. In order to support both header and XML file parsing, the header parser had to be reworked, as well as some of the HW functions. * Add interface support for showing errors on the screen. All commands should be using these functions rather than print to the screen directly. * Add support for wildcarding the register index - if the register index is '*', all registers in the correspoding register array will be shown. * Add comments to diaglib functions.

* Wed May 7 2014 <mitko.haralanov@intel.com>
- initial commit

* Wed May 7 2014 <andrew.friedley@intel.com>
- Fix Bypass16 packet macro parsing.

* Mon May 5 2014 <mitko.haralanov@intel.com>
- Rename all diag* references

* Fri May 2 2014 <mitko.haralanov@intel.com>
- Remove unused parts of libipathdiags

* Fri Apr 25 2014 <dennis.dalessandro@intel.com>
- This implements hfi_control, a port of ipath_control for WFR

* Wed Apr 23 2014 <john.fleck@intel.com>
- Add install entry to the diaglib makefile

* Mon Apr 7 2014 <mike.marciniszyn@intel.com>
- wfr-diag: correct dependency rpm

* Mon Apr 7 2014 <andrew.friedley@intel.com>
- Fix BTH header macro parsing.

* Mon Mar 10 2014 <andrew.friedley@intel.com>
- Update README for build system changes.

* Mon Mar 10 2014 <andrew.friedley@intel.com>
- Update build system to depend on hfi-psm-devel-noship, which contains the internal headers from PSM that diagtools use.

* Fri Feb 28 2014 <andrew.friedley@intel.com>
- Support for sending 8/10/16b bypass packets. Add support for dumping bypass packets for debugging. Code cleanup.

* Thu Feb 27 2014 <andrew.friedley@intel.com>
- Initial support for 8b bypass packets.

* Thu Feb 27 2014 <andrew.friedley@intel.com>
- Add packet suffix options for remaining PBC bits.

* Fri Feb 14 2014 <andrew.friedley@intel.com>
- Update pkt_send man pages and build system.

* Fri Feb 14 2014 <andrew.friedley@intel.com>
- Rename man pages.

* Fri Feb 14 2014 <andrew.friedley@intel.com>
- Code cleanup; fix up usage message and parameters.

* Fri Feb 14 2014 <andrew.friedley@intel.com>
- Bug fixes for -B deferred trigger mode.

* Fri Feb 14 2014 <andrew.friedley@intel.com>
- Bug fixes: - Fix credit calculation; the length was wrong. - Fix KDETH header parsing. - Fill in default KDETH values. - psm_min generates a response from a pkt_test responder.

* Thu Feb 13 2014 <andrew.friedley@intel.com>
- Tested and updated all sample packet files.

* Wed Feb 12 2014 <andrew.friedley@intel.com>
- Code cleanup.

* Wed Feb 12 2014 <andrew.friedley@intel.com>
- Deferred-trigger support implemented. Advance the eager queue buffer when packets with payload are received. Code cleanup.

* Wed Feb 12 2014 <andrew.friedley@intel.com>
- Added/updated code to receive and discard packets.

* Tue Feb 11 2014 <andrew.friedley@intel.com>
- Checkpoint: hfi_pkt_send sends packets on WFR.

* Tue Feb 11 2014 <andrew.friedley@intel.com>
- Change default payload size from 2048 to 8192.

* Tue Feb 11 2014 <andrew.friedley@intel.com>
- Retab get_pkt() so I can read it.

* Tue Feb 11 2014 <andrew.friedley@intel.com>
- Starting on pkt_send:

* Mon Feb 10 2014 <andrew.friedley@intel.com>
- Add 'noship' to RPM name.

* Fri Feb 7 2014 <andrew.friedley@intel.com>
- Updated README.WFR for hfi_pkt_test and RPM building.

* Fri Feb 7 2014 <andrew.friedley@intel.com>
- Remove unneeded headers.

* Fri Feb 7 2014 <andrew.friedley@intel.com>
- Fix a bug introduced during code cleanup.

* Fri Feb 7 2014 <andrew.friedley@intel.com>
- Working on build system to support generating RPMs.

* Thu Feb 6 2014 <andrew.friedley@intel.com>
- Working on build system to generate an RPM: - Add Makefile install, dist, specfile rules to - Add hfi-diagtools-sw.spec.in

* Wed Feb 5 2014 <andrew.friedley@intel.com>
- Updated man page corresponding to current hfi_pkt_test.

* Wed Feb 5 2014 <andrew.friedley@intel.com>
- Code cleanup.

* Wed Feb 5 2014 <andrew.friedley@intel.com>
- Code cleanup.

* Tue Feb 4 2014 <andrew.friedley@intel.com>
- Initial implementation of -B local PIO send buffer test.

* Mon Feb 3 2014 <andrew.friedley@intel.com>
- Payload works; some code cleanup.

* Mon Feb 3 2014 <andrew.friedley@intel.com>
- Checkpoint -- streaming mode appears to work.

* Mon Feb 3 2014 <andrew.friedley@intel.com>
- Initial updates to the build system for HFI renaming and conversion from hg to git.

* Fri Jan 31 2014 <andrew.friedley@intel.com>
- Preparing to support packet payloads.

* Thu Jan 30 2014 <andrew.friedley@intel.com>
- ping-pong mode works. encountered bug in streaming mode.

* Thu Jan 23 2014 <andrew.friedley@intel.com>
- Checkpoint: can detect arrival of multiple receive packets.

* Tue Jan 21 2014 <andrew.friedley@intel.com>
- Clean up the new PIO send routine somewhat.

* Mon Jan 20 2014 <andrew.friedley@intel.com>
- Working on hfi_packet_test

* Thu Dec 12 2013 <dean.luick@intel.com>
- Clarify README.WFR

* Thu Dec 12 2013 <dean.luick@intel.com>
- Fix PBC VL15 bits for WFR

* Mon Nov 4 2013 <dean.luick@intel.com>
- Add WFR build directions

* Mon Nov 4 2013 <dean.luick@intel.com>
- Another help text formatting fix

* Mon Nov 4 2013 <dean.luick@intel.com>
- Fix help text formatting

* Mon Nov 4 2013 <dean.luick@intel.com>
- Rename ipath_pkt_send to hfi_pkt_send

* Mon Nov 4 2013 <dean.luick@intel.com>
- Update for MAD packet sends

* Thu Oct 31 2013 <dean.luick@intel.com>
- Build without buildmeister

