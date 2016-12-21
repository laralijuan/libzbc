/*
 * This file is part of libzbc.
 *
 * Copyright (C) 2009-2014, HGST, Inc. All rights reserved.
 * Copyright (C) 2016, Western Digital. All rights reserved.
 *
 * This software is distributed under the terms of the BSD 2-clause license,
 * "as is," without technical support, and WITHOUT ANY WARRANTY, without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. You should have received a copy of the BSD 2-clause license along
 * with libzbc. If not, see  <http://opensource.org/licenses/BSD-2-Clause>.
 *
 * Authors: Damien Le Moal (damien.lemoal@wdc.com)
 *          Christoph Hellwig (hch@infradead.org)
 */

#include "zbc.h"

#include <string.h>

/*
 * Log level.
 */
int zbc_log_level = ZBC_LOG_ERROR;

/*
 * Backend drivers.
 */
static struct zbc_ops *zbc_ops[] = {
	&zbc_block_ops,
	&zbc_scsi_ops,
	&zbc_ata_ops,
	&zbc_fake_ops,
	NULL
};

/**
 * Sense key strings.
 */
static struct zbc_sg_sk_s {
	enum zbc_sk	sk;
	const char	*sk_name;
} zbc_sg_sk_list[] = {
	{ ZBC_SK_ILLEGAL_REQUEST,	"Illegal-request"	},
	{ ZBC_SK_DATA_PROTECT,		"Data-protect"		},
	{ ZBC_SK_ABORTED_COMMAND,	"Aborted-command"	},
	{ 0,				NULL }
};

/**
 * Sense code qualifiers strings.
 */
static struct zbc_sg_asc_ascq_s {
	enum zbc_asc_ascq	asc_ascq;
	const char		*ascq_name;
} zbc_sg_asc_ascq_list[] = {

	{
		ZBC_ASC_INVALID_FIELD_IN_CDB,
		"Invalid-field-in-cdb"
	},
	{
		ZBC_ASC_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE,
		"Logical-block-address-out-of-range"
	},
	{
		ZBC_ASC_UNALIGNED_WRITE_COMMAND,
		"Unaligned-write-command"
	},
	{
		ZBC_ASC_WRITE_BOUNDARY_VIOLATION,
		"Write-boundary-violation"
	},
	{
		ZBC_ASC_ATTEMPT_TO_READ_INVALID_DATA,
		"Attempt-to-read-invalid-data"
	},
	{
		ZBC_ASC_READ_BOUNDARY_VIOLATION,
		"Read-boundary-violation"
	},
	{
		ZBC_ASC_ZONE_IS_READ_ONLY,
		"Zone-is-read-only"
	},
	{
		ZBC_ASC_INSUFFICIENT_ZONE_RESOURCES,
		"Insufficient-zone-resources"
	},
	{
		0,
		NULL
	}
};

/***** Definition of public functions *****/

/**
 * zbc_set_log_level - Set the library log level
 */
void
zbc_set_log_level(char *log_level)
{
	if (!log_level) {
		/* Set default */
		zbc_log_level = ZBC_LOG_ERROR;
		return;
	}

	if (strcmp(log_level, "none") == 0)
		zbc_log_level = ZBC_LOG_NONE;
	else if (strcmp(log_level, "error") == 0)
		zbc_log_level = ZBC_LOG_ERROR;
	else if (strcmp(log_level, "info") == 0)
		zbc_log_level = ZBC_LOG_INFO;
	else if (strcmp(log_level, "debug") == 0)
		zbc_log_level = ZBC_LOG_DEBUG;
	else if (strcmp(log_level, "vdebug") == 0)
		zbc_log_level = ZBC_LOG_VDEBUG;
	else
		fprintf(stderr, "Unknown log level \"%s\"\n",
			log_level);
}

/**
 * zbc_disk_type_str - Returns a disk type name
 */
const char *zbc_disk_type_str(enum zbc_dev_type type)
{
	switch (type) {
	case ZBC_DT_BLOCK:
		return( "Zoned block device" );
	case ZBC_DT_SCSI:
		return( "SCSI ZBC device" );
	case ZBC_DT_ATA:
		return( "ATA ZAC device" );
	case ZBC_DT_FAKE:
		return( "Emulated zoned block device" );
	case ZBC_DT_UNKNOWN:
	default:
		return "Unknown-disk-type";
	}
}

/**
 * zbc_disk_model_str - Returns a disk model name
 */
const char *zbc_disk_model_str(enum zbc_dev_model model)
{
	switch (model) {
	case ZBC_DM_HOST_AWARE:
		return( "Host-aware" );
	case ZBC_DM_HOST_MANAGED:
		return( "Host-managed" );
	case ZBC_DM_DEVICE_MANAGED:
		return( "Device-managed" );
	case ZBC_DM_STANDARD:
		return( "Regular" );
	case ZBC_DM_DRIVE_UNKNOWN:
	default:
		return "Unknown-disk-model";
	}
}

/**
 * zbc_zone_type_str - returns a string describing a zone type
 */
const char *zbc_zone_type_str(enum zbc_zone_type type)
{
	switch (type) {
	case ZBC_ZT_CONVENTIONAL:
		return( "Conventional" );
	case ZBC_ZT_SEQUENTIAL_REQ:
		return( "Sequential-write-required" );
	case ZBC_ZT_SEQUENTIAL_PREF:
		return( "Sequential-write-preferred" );
	case ZBC_ZT_UNKNOWN:
	default:
		return "Unknown-zone-type";
	}
}

/**
 * zbc_zone_cond_str - Returns a string describing a zone condition
 */
const char *zbc_zone_condition_str(enum zbc_zone_condition cond)
{
	switch (cond) {
	case ZBC_ZC_NOT_WP:
		return( "Not-write-pointer" );
	case ZBC_ZC_EMPTY:
		return( "Empty" );
	case ZBC_ZC_IMP_OPEN:
		return( "Implicit-open" );
	case ZBC_ZC_EXP_OPEN:
		return( "Explicit-open" );
	case ZBC_ZC_CLOSED:
		return( "Closed" );
	case ZBC_ZC_RDONLY:
		return( "Read-only" );
	case ZBC_ZC_FULL:
		return( "Full" );
	case ZBC_ZC_OFFLINE:
		return( "Offline" );
	default:
		return "Unknown-zone-condition";
	}
}

/**
 * zbc_errno - Get detailed error code of last operation
 */
void zbc_errno(struct zbc_device *dev, struct zbc_errno *err)
{
        memcpy(err, &dev->zbd_errno, sizeof(struct zbc_errno));
}

/**
 * zbc_sk_str - Returns a string describing a sense key
 */
const char *zbc_sk_str(enum zbc_sk sk)
{
	int i = 0;

	while (zbc_sg_sk_list[i].sk != 0) {
		if (sk == zbc_sg_sk_list[i].sk)
			return zbc_sg_sk_list[i].sk_name;
		i++;
	}

	return "Unknown-sense-key";
}

/**
 * zbc_asc_ascq_str - Returns a string describing a sense code
 */
const char *zbc_asc_ascq_str(enum zbc_asc_ascq asc_ascq)
{
	int i = 0;

	while (zbc_sg_asc_ascq_list[i].asc_ascq != 0) {
		if (asc_ascq == zbc_sg_asc_ascq_list[i].asc_ascq)
			return zbc_sg_asc_ascq_list[i].ascq_name;
		i++;
	}

	return "Unknown-additional-sense-code-qualifier";
}

/**
 * zbc_device_is_zoned - Test if a physical device is zoned.
 */
int zbc_device_is_zoned(const char *filename,
			bool fake,
			struct zbc_device_info *info)
{
	struct zbc_device *dev = NULL;
	int ret = -ENODEV, i;

	/* Test all backends until one accepts the drive. */
	for (i = 0; zbc_ops[i]; i++) {
		ret = zbc_ops[i]->zbd_open(filename, O_RDONLY, &dev);
		if (ret == 0) {
			/* This backend accepted the device */
			dev->zbd_ops = zbc_ops[i];
			break;
		}
	}

	if (dev && dev->zbd_ops) {
		if (dev->zbd_ops == &zbc_fake_ops && !fake) {
			ret = 0;
		} else {
			ret = 1;
			if (info)
				memcpy(info, &dev->zbd_info,
				       sizeof(zbc_device_info_t));
		}
		dev->zbd_ops->zbd_close(dev);
	} else {
		if ((ret != -EPERM) && (ret != -EACCES))
			ret = 0;
	}

	return ret;
}

/**
 * zbc_open - open a ZBC device
 */
int zbc_open(const char *filename, int flags, struct zbc_device **pdev)
{
	struct zbc_device *dev = NULL;
	int ret, i;

	/* Test all backends until one accepts the drive */
	for (i = 0; zbc_ops[i] != NULL; i++) {
		ret = zbc_ops[i]->zbd_open(filename, flags, &dev);
		if (ret == 0) {
			/* This backend accepted the drive */
			dev->zbd_ops = zbc_ops[i];
			*pdev = dev;
			return 0;
		}
	}

	return -ENODEV;
}

/**
 * zbc_close - close a ZBC Device
 */
int zbc_close(struct zbc_device *dev)
{
	return dev->zbd_ops->zbd_close(dev);
}

/**
 * zbc_get_device_info - Get a ZBC device information
 */
void zbc_get_device_info(struct zbc_device *dev, struct zbc_device_info *info)
{
	memcpy(info, &dev->zbd_info, sizeof(zbc_device_info_t));
}

/**
 * zbc_print_device_info - Print a device information
 */
void zbc_print_device_info(struct zbc_device_info *info, FILE *out)
{
	fprintf(out,
		"    Vendor ID: %s\n",
		info->zbd_vendor_id);
	fprintf(out,
		"    %s interface, %s disk model\n",
		zbc_disk_type_str(info->zbd_type),
		zbc_disk_model_str(info->zbd_model));
	fprintf(out,
		"    %llu 512-bytes sectors\n",
		(unsigned long long) info->zbd_sectors);
	fprintf(out,
		"    %llu logical blocks of %u B\n",
		(unsigned long long) info->zbd_lblocks,
		(unsigned int) info->zbd_lblock_size);
	fprintf(out,
		"    %llu physical blocks of %u B\n",
		(unsigned long long) info->zbd_pblocks,
		(unsigned int) info->zbd_pblock_size);
	fprintf(out,
		"    %.03F GB capacity\n",
		(double)(info->zbd_sectors << 9) / 1000000000);

	if (info->zbd_model == ZBC_DM_HOST_MANAGED ||
	    info->zbd_model == ZBC_DM_HOST_AWARE)
		fprintf(out,
			"    Read commands are %s\n",
			(info->zbd_flags & ZBC_UNRESTRICTED_READ) ?
			"unrestricted" : "restricted");

	if (info->zbd_model == ZBC_DM_HOST_MANAGED) {
		fprintf(out,
			"    Maximum number of open sequential write required zones: %u\n",
		       (unsigned int) info->zbd_max_nr_open_seq_req);
	} else if (info->zbd_model == ZBC_DM_HOST_AWARE) {
		fprintf(out,
			"    Optimal number of open sequential write preferred zones: %u\n",
			(unsigned int) info->zbd_opt_nr_open_seq_pref);
		fprintf(out,
			"    Optimal number of non-sequentially written sequential write preferred zones: %u\n",
			(unsigned int) info->zbd_opt_nr_non_seq_write_seq_pref);
	}

	fflush(out);
}

/**
 * zbc_report_zones - Get zone information
 */
int zbc_report_zones(struct zbc_device *dev, uint64_t sector,
		     enum zbc_reporting_options ro,
		     struct zbc_zone *zones, unsigned int *nr_zones)
{
        unsigned int n, nz = 0;
	uint64_t last_sector;
	int ret;

	if (!zones)
		/* Get the number of zones */
		return (dev->zbd_ops->zbd_report_zones)(dev, sector,
							zbc_ro_mask(ro),
							NULL, nr_zones);

        /* Get zones information */
        while (nz < *nr_zones) {

		n = *nr_zones - nz;
		ret = (dev->zbd_ops->zbd_report_zones)(dev, sector,
					zbc_ro_mask(ro) | ZBC_RO_PARTIAL,
					&zones[nz], &n);
		if (ret != 0) {
			zbc_error("Get zones from LBA %llu failed %d (%s)\n",
				  (unsigned long long) sector,
				  ret, strerror(-ret));
			return ret;
		}

		if (n == 0)
			break;

		nz += n;
		last_sector = zones[nz - 1].zbz_start +
			zones[nz - 1].zbz_length;

		if (last_sector >= dev->zbd_info.zbd_sectors)
			break;

		sector = last_sector;

        }

	*nr_zones = nz;

	return 0;
}

/**
 * zbc_list_zones - Get zone information
 */
int zbc_list_zones(struct zbc_device *dev, uint64_t sector,
		   enum zbc_reporting_options ro,
		   struct zbc_zone **pzones, unsigned int *pnr_zones)
{
	struct zbc_zone *zones = NULL;
	unsigned int nr_zones;
	int ret;

	/* Get total number of zones */
	ret = zbc_report_nr_zones(dev, sector, zbc_ro_mask(ro), &nr_zones);
	if (ret < 0)
		return ret;

	zbc_debug("Device %s: %d zones\n",
		  dev->zbd_filename,
		  nr_zones);

	/* Allocate zone array */
	zones = (struct zbc_zone *) calloc(nr_zones, sizeof(struct zbc_zone));
	if (!zones) {
		zbc_error("No memory\n");
		return -ENOMEM;
	}

	/* Get zones information */
	ret = zbc_report_zones(dev, sector, zbc_ro_mask(ro), zones, &nr_zones);
	if (ret != 0) {
		zbc_error("zbc_report_zones failed %d\n", ret);
		free(zones);
		return ret;
	}

	*pzones = zones;
	*pnr_zones = nr_zones;

	return 0;
}

/**
 * zbc_zone_operation - Execute an operation on a zone
 */
int zbc_zone_operation(struct zbc_device *dev, uint64_t sector,
		       enum zbc_zone_op op, unsigned int flags)
{

	if ((!(flags & ZBC_OP_ALL_ZONES)) &&
	    !zbc_dev_sect_laligned(dev, sector))
		return -EINVAL;

	/* Execute the operation */
	return (dev->zbd_ops->zbd_zone_op)(dev, sector, op, flags);
}

/**
 * zbc_pread - Read sectors form a device
 */
ssize_t zbc_pread(struct zbc_device *dev, void *buf,
		  size_t count, uint64_t offset)
{
	ssize_t ret;

	if (!zbc_dev_sect_laligned(dev, count) ||
	    !zbc_dev_sect_laligned(dev, offset))
		return -EINVAL;

	if (count > dev->zbd_info.zbd_max_rw_sectors)
		return -EINVAL;

	if ((offset + count) > dev->zbd_info.zbd_sectors)
		count = dev->zbd_info.zbd_sectors - offset;
	if (!count ||
	    offset >= dev->zbd_info.zbd_sectors)
		return 0;

	ret = (dev->zbd_ops->zbd_pread)(dev, buf, count, offset);
	if (ret < 0)
		zbc_error("Read %zu sectors at sector %llu failed %zd (%s)\n",
			  count, (unsigned long long) offset,
			  -ret, strerror(-ret));

	return ret;
}

/**
 * zbc_pwrite - Write sectors to a device
 */
ssize_t zbc_pwrite(struct zbc_device *dev, const void *buf,
		   size_t count, uint64_t offset)
{
	ssize_t ret;

	if (!zbc_dev_sect_paligned(dev, count) ||
	    !zbc_dev_sect_paligned(dev, offset))
		return -EINVAL;

	if (count > dev->zbd_info.zbd_max_rw_sectors)
		return -EINVAL;

	if ((offset + count) > dev->zbd_info.zbd_sectors)
		count = dev->zbd_info.zbd_sectors - offset;
	if (!count ||
	    offset >= dev->zbd_info.zbd_sectors)
		return 0;

	ret = (dev->zbd_ops->zbd_pwrite)(dev, buf, count, offset);
	if (ret < 0)
		zbc_error("Write %zu sectors at sector %llu failed %zd (%s)\n",
			  count, (unsigned long long) offset,
			  -ret, strerror(-ret));

	return ret;
}

/**
 * zbc_flush - flush a device write cache
 */
int zbc_flush(struct zbc_device *dev)
{
	return (dev->zbd_ops->zbd_flush)(dev);
}

/**
 * zbc_set_zones - Configure zones of an emulated device
 */
int zbc_set_zones(struct zbc_device *dev,
		  uint64_t conv_sz, uint64_t zone_sz)
{

	/* Do this only if supported */
	if (!dev->zbd_ops->zbd_set_zones)
		return -ENXIO;

	if (!zbc_dev_sect_paligned(dev, conv_sz) ||
	    !zbc_dev_sect_paligned(dev, zone_sz))
		return -EINVAL;

	return (dev->zbd_ops->zbd_set_zones)(dev, conv_sz, zone_sz);
}

/**
 * zbc_set_write_pointer - Change the value of a zone write pointer
 */
int
zbc_set_write_pointer(struct zbc_device *dev,
                      uint64_t sector,
                      uint64_t wp_sector)
{

	/* Do this only if supported */
	if (!dev->zbd_ops->zbd_set_wp)
		return -ENXIO;

	if (!zbc_dev_sect_paligned(dev, sector) ||
	    !zbc_dev_sect_paligned(dev, wp_sector))
		return -EINVAL;

	return (dev->zbd_ops->zbd_set_wp)(dev, sector, wp_sector);
}

