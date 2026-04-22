/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/systm.h>

#include <sys/nvpair.h>
#include <sys/zfs_context.h>
#include <sys/dsl_crypt.h>

#include "zhamel_zfskey.h"

static bool zhamel_zfskey_processed;
static struct root_hold_token *zhamel_zfskey_root_token;

static bool
zhamel_zfskey_dataset_has_nul(const char *dataset, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (dataset[i] == '\0')
			return (true);
	}
	return (false);
}

static int
zhamel_zfskey_import_pool(const char *dataset)
{
	char pool[MAXNAMELEN];
	const char *slash;
	size_t pool_len;

	slash = strchr(dataset, '/');
	if (slash == NULL || slash == dataset)
		return (EINVAL);

	pool_len = (size_t)(slash - dataset);
	if (pool_len >= sizeof(pool))
		return (ENAMETOOLONG);

	memcpy(pool, dataset, pool_len);
	pool[pool_len] = '\0';

	return (spa_import_rootpool(pool, false));
}

static int
zhamel_zfskey_load_wkey(const char *dataset, const uint8_t *wkey)
{
	dsl_crypto_params_t *dcp;
	nvlist_t *hidden_args;
	int error;

	dcp = NULL;
	hidden_args = fnvlist_alloc();
	fnvlist_add_uint8_array(hidden_args, "wkeydata", wkey,
	    ZHAMEL_ZFSKEY_WKEY_LEN);

	error = dsl_crypto_params_create_nvlist(DCP_CMD_NONE, NULL,
	    hidden_args, &dcp);
	fnvlist_free(hidden_args);
	if (error != 0)
		goto out;

	error = spa_keystore_load_wkey(dataset, dcp, B_FALSE);

out:
	dsl_crypto_params_free(dcp, error != 0);
	return (error);
}

static int
zhamel_zfskey_process_blob(void *data, size_t size, bool verbose)
{
	const struct zhamel_zfskey_header *hdr;
	const char *dataset;
	const uint8_t *wkey;
	uint32_t version, header_size, dataset_len, wkey_len;
	size_t required;
	int error;

	if (size < sizeof(*hdr))
		return (EINVAL);

	hdr = data;
	if (memcmp(hdr->magic, ZHAMEL_ZFSKEY_MAGIC,
	    ZHAMEL_ZFSKEY_MAGIC_LEN) != 0)
		return (EINVAL);

	version = le32toh(hdr->version_le);
	header_size = le32toh(hdr->header_size_le);
	dataset_len = le32toh(hdr->dataset_len_le);
	wkey_len = le32toh(hdr->wkey_len_le);

	if (version != ZHAMEL_ZFSKEY_VERSION ||
	    header_size < sizeof(*hdr) || header_size > size ||
	    dataset_len == 0 || dataset_len > MAXNAMELEN ||
	    wkey_len != ZHAMEL_ZFSKEY_WKEY_LEN)
		return (EINVAL);

	required = (size_t)header_size + dataset_len + wkey_len;
	if (required < header_size || required > size)
		return (EINVAL);

	dataset = (const char *)((const uint8_t *)data + header_size);
	wkey = (const uint8_t *)dataset + dataset_len;

	if (!zhamel_zfskey_dataset_has_nul(dataset, dataset_len) ||
	    strchr(dataset, '@') != NULL || strchr(dataset, '%') != NULL)
		return (EINVAL);

	error = zhamel_zfskey_import_pool(dataset);
	if (error != 0) {
		if (verbose)
			printf("zhamel_zfskey: root pool import for %s "
			    "failed: %d\n", dataset, error);
	} else {
		error = zhamel_zfskey_load_wkey(dataset, wkey);
	}

	if (error == 0)
		printf("zhamel_zfskey: loaded ZFS wrapping key for %s\n",
		    dataset);
	else if (verbose)
		printf("zhamel_zfskey: failed to load ZFS wrapping key for "
		    "%s: %d\n", dataset, error);

	return (error);
}

static int
zhamel_zfskey_process_preloads(bool final)
{
	caddr_t mod;
	void *data;
	char *type;
	size_t size;
	int found, loaded, error;

	if (zhamel_zfskey_processed)
		return (0);

	found = 0;
	loaded = 0;
	mod = NULL;
	while ((mod = preload_search_next_name(mod)) != NULL) {
		type = preload_search_info(mod, MODINFO_TYPE);
		if (type == NULL ||
		    strcmp(type, ZHAMEL_ZFSKEY_PRELOAD_TYPE) != 0)
			continue;

		found++;
		data = preload_fetch_addr(mod);
		size = preload_fetch_size(mod);
		if (data == NULL || size == 0) {
			printf("zhamel_zfskey: preload record has no data\n");
			continue;
		}

		error = zhamel_zfskey_process_blob(data, size, final);
		if (error == 0) {
			explicit_bzero(data, size);
			loaded++;
		} else if (final) {
			explicit_bzero(data, size);
		}
	}

	if (found == 0 && final)
		printf("zhamel_zfskey: no preloaded ZFS key records found\n");
	else if (found != 0 && (loaded != 0 || final))
		printf("zhamel_zfskey: processed %d record(s), loaded %d\n",
		    found, loaded);

	if (loaded != 0 || final)
		zhamel_zfskey_processed = true;
	return (loaded);
}

static void
zhamel_zfskey_release_root_hold(void)
{
	if (zhamel_zfskey_root_token != NULL) {
		root_mount_rel(zhamel_zfskey_root_token);
		zhamel_zfskey_root_token = NULL;
	}
}

static void
zhamel_zfskey_worker(void *arg __unused)
{
	int i, loaded;

	loaded = 0;
	for (i = 0; i < 100 && loaded == 0; i++) {
		if (i != 0)
			pause("zhmkey", hz / 10);
		loaded = zhamel_zfskey_process_preloads(false);
	}
	if (loaded == 0)
		(void)zhamel_zfskey_process_preloads(true);
	zhamel_zfskey_release_root_hold();
	kproc_exit(0);
}

static int
zhamel_zfskey_modevent(module_t mod __unused, int type, void *data __unused)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		zhamel_zfskey_root_token = root_mount_hold("zhamel_zfskey");
		error = kproc_create(zhamel_zfskey_worker, NULL, NULL, 0, 0,
		    "zhamel_zfskey");
		if (error != 0) {
			printf("zhamel_zfskey: worker create failed: %d\n",
			    error);
			(void)zhamel_zfskey_process_preloads(true);
			zhamel_zfskey_release_root_hold();
		}
		return (0);
	case MOD_UNLOAD:
		zhamel_zfskey_release_root_hold();
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t zhamel_zfskey_mod = {
	"zhamel_zfskey",
	zhamel_zfskey_modevent,
	NULL
};

DECLARE_MODULE(zhamel_zfskey, zhamel_zfskey_mod, SI_SUB_ROOT_CONF,
    SI_ORDER_FIRST);
MODULE_VERSION(zhamel_zfskey, 1);
MODULE_DEPEND(zhamel_zfskey, zfsctrl, 1, 1, 1);
