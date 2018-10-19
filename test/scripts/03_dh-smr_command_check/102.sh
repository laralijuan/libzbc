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

zbc_test_init $0 "ZONE ACTIVATE${zbc_test_actv_flags}: LBA crossing out of range (zone addressing)" $*

# Get information
zbc_test_get_device_info
zbc_test_get_zone_realm_info

zbc_test_fsnoz_check_or_NA "${zbc_test_actv_flags}"

# Select the realm containing the last zone
zbc_test_search_realm_by_lba ${last_zone_lba}

zbc_test_get_target_zone_from_slba ${last_zone_lba}
if [[ ${target_type} == ${ZT_SWP} ]]; then
    target_type="seqp"
elif [[ ${target_type} == ${ZT_SWR} ]]; then
    target_type="seq"
elif [[ ${target_type} == ${ZT_SOBR} ]]; then
    target_type="sobr"
elif [[ ${target_type} == ${ZT_CONV} ]]; then
    target_type="conv"
else
    zbc_test_fail_exit \
	"WARNING: Zone ${last_zone_lba} unexpected type=${target_type}"
fi

# Use double the size of the realm when trying ACTIVATE across End of Medium
nzone=$(( 2 * $(zbc_realm_len ${target_type}) ))
target_lba=$(zbc_realm_start ${target_type})

expected_sk="Illegal-request"
expected_asc="Invalid-field-in-cdb"	# ZONE ID + NUMBER OF ZONES out of range

# Start testing
# Try to activate a zone range crossing the maximum device LBA
zbc_test_run ${bin_path}/zbc_test_zone_activate -v ${zbc_test_actv_flags} -z \
			${device} ${target_lba} ${nzone} ${cmr_type}

# Check result
zbc_test_get_sk_ascq
zbc_test_check_sk_ascq
