/* Copyright 2013-2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef pr_fmt
#define pr_fmt(fmt) "STB: " fmt
#endif

#include <skiboot.h>
#include <string.h>
#include <chip.h>
#include <xscom.h>
#include <inttypes.h>
#include "secureboot.h"
#include "cvc.h"

struct container_verification_code {
	uint64_t start_addr;
	uint64_t end_addr;
	struct list_head service_list;
};

static struct container_verification_code *cvc = NULL;
static void *secure_rom_mem = NULL;

struct cvc_service {
	int id;
	uint64_t addr;    /* base_addr + offset */
	uint32_t version;
	struct list_node link;
};

static struct {
	enum cvc_service_id id;
	const char *name;
} cvc_service_map[] = {
	{ CVC_SHA512_SERVICE, "sha512" },
	{ CVC_VERIFY_SERVICE, "verify" },
};

static struct cvc_service *cvc_find_service(enum cvc_service_id id)
{
	struct cvc_service *service;
	if (!cvc)
		return NULL;

	list_for_each(&cvc->service_list, service, link) {
		if (service->id == id)
			return service;
	}
	return NULL;
}

static const char *cvc_service_map_name(enum cvc_service_id id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cvc_service_map); i++) {
		if (cvc_service_map[i].id == id)
			return cvc_service_map[i].name;
	}
	return NULL;
}

static void cvc_register(uint64_t start_addr, uint64_t end_addr)
{
	if (cvc)
		return;

	cvc = malloc(sizeof(struct container_verification_code));
	assert(cvc);
	cvc->start_addr = start_addr;
	cvc->end_addr = end_addr;
	list_head_init(&cvc->service_list);
	prlog(PR_INFO, "Found CVC @ %" PRIx64 "-%" PRIx64 "\n",
	      start_addr, end_addr);
}

static void cvc_service_register(uint32_t id, uint32_t offset, uint32_t version)
{
	struct cvc_service *service;
	const char *name;

	if (!cvc)
		return;

	/* Service already registered? */
	if (cvc_find_service(id))
		return;

	if (cvc->start_addr + offset > cvc->end_addr) {
		prlog(PR_WARNING, "CVC service @ %x out of range, "
		      "id=%d\n", offset, id);
		return;
	}

	name = cvc_service_map_name(id);
	if (!name) {
		prlog(PR_ERR, "CVC service %d not supported\n", id);
		return;
	}

	service = malloc(sizeof(struct cvc_service));
	assert(service);
	service->id = id;
	service->version = version;
	service->addr = cvc->start_addr + offset;
	list_add_tail(&cvc->service_list, &service->link);
	prlog(PR_INFO, "Found CVC-%s @ %" PRIx64 ", version=%d\n",
	      name, service->addr, service->version);
}

#define SECURE_ROM_MEMORY_SIZE		(16 * 1024)
#define SECURE_ROM_XSCOM_ADDRESS	0x02020017

#define SECURE_ROM_SHA512_OFFSET	0x20
#define SECURE_ROM_VERIFY_OFFSET	0x30

static int cvc_secure_rom_init(void) {
	const uint32_t reg_addr = SECURE_ROM_XSCOM_ADDRESS;
	uint64_t reg_data;
	struct proc_chip *chip;

	if (!secure_rom_mem) {
		secure_rom_mem = malloc(SECURE_ROM_MEMORY_SIZE);
		assert(secure_rom_mem);
	}
	/*
	 * The logic that contains the ROM within the processor is implemented
	 * in a way that it only responds to CI (cache inhibited) operations.
	 * Due to performance issues we copy the verification code from the
	 * secure ROM to RAM. We use memcpy_from_ci() to do that.
	 */
	chip = next_chip(NULL);
	xscom_read(chip->id, reg_addr, &reg_data);
	memcpy_from_ci(secure_rom_mem, (void*) reg_data,
		       SECURE_ROM_MEMORY_SIZE);
	cvc_register((uint64_t)&secure_rom_mem,
		     (uint64_t)&secure_rom_mem + SECURE_ROM_MEMORY_SIZE-1);
	cvc_service_register(CVC_SHA512_SERVICE, SECURE_ROM_SHA512_OFFSET, 1);
	cvc_service_register(CVC_VERIFY_SERVICE, SECURE_ROM_VERIFY_OFFSET, 1);
	return 0;
}

int cvc_init(void)
{
	struct dt_node *node;
	int version;
	int rc = 0;

	if (cvc)
		return 0;

	node = dt_find_by_path(dt_root, "/ibm,secureboot");
	if (!node)
		return -1;

	if (!secureboot_is_compatible(node, &version, NULL)) {
		/**
		 * @fwts-label CVCNotCompatible
		 * @fwts-advice Compatible CVC driver not found. Probably,
		 * hostboot/mambo/skiboot has updated the
		 * /ibm,secureboot/compatible without adding a driver that
		 * supports it.
		 */
		prlog(PR_ERR, "%s FAILED, /ibm,secureboot not compatible.\n",
		     __func__);
		return -1;
	}

	/* Only in P8 the CVC is stored in a secure ROM */
	if (version == IBM_SECUREBOOT_V1 &&
	    proc_gen == proc_gen_p8) {
		rc = cvc_secure_rom_init();
	} else {
		prlog(PR_ERR, "%s FAILED. /ibm,secureboot not supported\n",
		      __func__);
		return -1;
	}

	return rc;
}
