/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026
 */

#ifndef _ZHAMEL_ZFSKEY_H_
#define	_ZHAMEL_ZFSKEY_H_

#include <sys/param.h>
#include <sys/types.h>

#define	ZHAMEL_ZFSKEY_PRELOAD_TYPE	"zhamel_zfs_key"
#define	ZHAMEL_ZFSKEY_MAGIC		"ZHMZKEY\0"
#define	ZHAMEL_ZFSKEY_MAGIC_LEN		8
#define	ZHAMEL_ZFSKEY_VERSION		1
#define	ZHAMEL_ZFSKEY_WKEY_LEN		32

/*
 * Binary payload loaded by zhamel as MODINFO_TYPE ZHAMEL_ZFSKEY_PRELOAD_TYPE.
 * Multibyte integers are little-endian. The payload following this header is:
 *
 *   uint8_t dataset[dataset_len];  // NUL-terminated ZFS encryption root name
 *   uint8_t wkey[wkey_len];        // raw OpenZFS wrapping key, currently 32 B
 *
 * The kernel module wipes the whole preloaded payload after processing.
 */
struct zhamel_zfskey_header {
	uint8_t		magic[ZHAMEL_ZFSKEY_MAGIC_LEN];
	uint32_t	version_le;
	uint32_t	header_size_le;
	uint32_t	dataset_len_le;
	uint32_t	wkey_len_le;
	uint32_t	flags_le;
	uint8_t		reserved[12];
};

#endif /* !_ZHAMEL_ZFSKEY_H_ */
