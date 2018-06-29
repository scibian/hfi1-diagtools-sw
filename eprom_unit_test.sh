#!/bin/bash

if [[ -z $1 ]]; then
	# Check the old location
	eprom_cmd="./build/targ-x86_64/utils/hfi1_eprom"
	echo "No hfi1_eprom specified using local build dir"
else
	# Use user specified
	eprom_cmd="$1"
	echo "Using selected $eprom_cmd"
fi

#device="/sys/devices/pci0000:00/0000:00:03.0/0000:05:00.0/resource0"
#eprom_cmd="$eprom_cmd -d $device"

function clean_files
{
	rm -f eprom_bulk.save eprom_bulk.save1 eprom_bulk.text eprom_config.save eprom_config.save1 eprom_config.text eprom_op.save eprom_op.save1 eprom_op.text eprom_test.in
}

echo ""
echo "--------------------------"
echo "Testing Basic Info Commnad"
echo "--------------------------"
$eprom_cmd
if [[ $? -ne 0 ]]; then
	echo "Failed basic info cmd"
	exit $?
fi

echo ""
echo "-----------------------"
echo "Saving original content"
echo "-----------------------"
$eprom_cmd -r -o eprom_op.save -c eprom_config.save -b eprom_bulk.save
if [[ $? -ne 0 ]]; then
	echo "Could not save contents!"
	exit 1
fi

echo ""
echo "----------"
echo "Erase chip"
echo "----------"
$eprom_cmd -e
if [[ $? -ne 0 ]]; then
	echo "Could not erase chip"
	exit 1
fi

echo ""
echo "-------------------"
echo "Reading erased info"
echo "-------------------"
$eprom_cmd -r -o eprom_op.erased -c eprom_config.erased -b eprom_bulk.erased
if [[ $? -ne 0 ]]; then
	echo "Could not read erased partitions"
	exit 1
fi

echo ""
echo "------------------------"
echo "Writing to each parition"
echo "------------------------"
content="The quick red fox jumped over the lazy dog"
echo "$content" > eprom_test.in
$eprom_cmd -w -o eprom_test.in -c eprom_test.in -b eprom_test.in
if [[ $? -ne 0 ]]; then
	echo "Could not write partitions"
	exit 1
fi

echo ""
echo "-----------------"
echo "Reading text back"
echo "-----------------"
$eprom_cmd -r -o eprom_op.text -c eprom_config.text -b eprom_bulk.text
if [[ $? -ne 0 ]]; then
	echo "Could not read partitions"
	exit 1
fi

line=`head -n 1 eprom_op.text`
if [[ $content != $line ]]; then
	echo "Did not validate OP"
	exit 1
fi
line=`head -n 1 eprom_config.text`
if [[ $content != $line ]]; then
	echo "Did not validate CONFIG"
	exit 1
fi

line=`head -n 1 eprom_bulk.text`
if [[ $content != $line ]]; then
	echo "Did not validate BLK"
	exit 1
fi

echo ""
echo "---------"
echo "Restoring"
echo "---------"

$eprom_cmd -e -o
if [[ $? -ne 0 ]]; then
	echo "Unable to erase OP"
	exit 1
fi

$eprom_cmd -e -c
if [[ $? -ne 0 ]]; then
	echo "Unable to erase CONFIG"
	exit 1
fi

$eprom_cmd -e -b
if [[ $? -ne 0 ]]; then
	echo "Unable to erase BULK"
	exit 1
fi

$eprom_cmd -w -o eprom_op.save -c eprom_config.save -b eprom_bulk.save
if [[ $? -ne 0 ]]; then
	echo "Could not restore!!!!!!"
	exit 1
fi

$eprom_cmd -r -o eprom_op.save1 -c eprom_config.save1 -b eprom_bulk.save1
if [[ $? -ne 0 ]]; then
	echo "Could not read eprom"
	exit 1
fi

diff eprom_op.save eprom_op.save1
if [[ $? -ne 0 ]]; then
	echo "Differences in OP"
	exit 1
fi

diff eprom_config.save eprom_config.save1
if [[ $? -ne 0 ]]; then
	echo "Differences in CONFIG"
	exit 1
fi

diff eprom_bulk.save eprom_bulk.save1
if [[ $? -ne 0 ]]; then
	echo "Differences in BULK"
	exit 1
fi

clean_files

echo "EPROM restored"

exit 0

