#!/bin/bash
#
# This file is part of libzbc.
#
# Copyright (C) 2018, Western Digital. All rights reserved.
#
# This software is distributed under the terms of the BSD 2-clause license,
# "as is," without technical support, and WITHOUT ANY WARRANTY, without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. You should have received a copy of the BSD 2-clause license along
# with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.

. scripts/zbc_test_lib.sh

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: non-activation CONV or SOBR to unsupported type ${test_actv_type:-"sequential"} (zone addressing)" $*

# Set expected error code
expected_sk="${ERR_ZA_SK}"
expected_asc="${ERR_ZA_ASC}"
expected_err_za="${ZA_STAT_UNSUPP}"

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

seq_type=${test_actv_type:-${smr_type}}

# Find the first CONV or SOBR realm that cannot be activated as SWR or SWP
zbc_test_search_realm_by_type_and_actv_or_NA "${ZT_NON_SEQ}" "noseq"

# Start testing
lba=$(zbc_realm_start ${seq_type})
if [ $? -ne 0 ]; then
   zbc_test_print_passed_lib "No realm_smr_start"
   exit
fi

len=$(zbc_realm_smr_len)
if [ $? -ne 0 ]; then
   zbc_test_print_passed_lib "No realm_smr_len"
   exit
fi

zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${lba} ${len} ${seq_type}

if [ $? -eq 2 ]; then
   zbc_test_print_passed_lib "zbc_test_zone_activate"
   exit
fi

# Check result
zbc_test_get_sk_ascq
zbc_test_check_err
