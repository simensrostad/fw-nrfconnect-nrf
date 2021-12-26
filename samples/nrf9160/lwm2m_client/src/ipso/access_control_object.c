/*
 * Copyright (c) 2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME app_lwm2m_obj_access_control
#define LOG_LEVEL CONFIG_CLOUD_INTEGRATION_LOG_LEVEL

#include <zephyr/types.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <init.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"

#define ACCESS_CONTROL_VERSION_MAJOR 1
#define ACCESS_CONTROL_VERSION_MINOR 0
#define ACCESS_CONTROL_OBJECT_ID 2
#define ACCESS_CONTROL_MAX_ID 5

/*
 * Calculate resource instances as follows:
 * start with ACCESS_CONTROL_MAX_ID
 */
#define RESOURCE_INSTANCE_COUNT (ACCESS_CONTROL_MAX_ID)

/* resource state */
static uint16_t object_id = 3304;
static uint16_t object_instance = 0;
static uint16_t acl = 0;
static uint16_t acl2 = 0;
static uint16_t access_cntrl_owner = 101;

static struct lwm2m_engine_obj access_control;
static struct lwm2m_engine_obj_field fields[] = {
	OBJ_FIELD_DATA(0, R, U16),
	OBJ_FIELD_DATA(1, R, U16),
	OBJ_FIELD_DATA(2, RW_OPT, U16),
	OBJ_FIELD_DATA(3, R, U16),
};

static struct lwm2m_engine_obj_inst inst;
static struct lwm2m_engine_res res[ACCESS_CONTROL_MAX_ID];
static struct lwm2m_engine_res_inst res_inst[RESOURCE_INSTANCE_COUNT];

static struct lwm2m_engine_obj_inst *access_control_create(uint16_t obj_inst_id)
{
	int i = 0, j = 0;

	if (inst.resource_count) {
		LOG_ERR("Only 1 instance of Location object can exist.");
		return NULL;
	}

	init_res_instance(res_inst, ARRAY_SIZE(res_inst));

	/* initialize instance resource data */
	INIT_OBJ_RES_DATA(0, res, i, res_inst, j, &object_id, sizeof(object_id));
	INIT_OBJ_RES_DATA(1, res, i, res_inst, j, &object_instance, sizeof(object_instance));
	INIT_OBJ_RES_MULTI_OPTDATA(2, res, i, res_inst, j, 2, false);
	INIT_OBJ_RES_DATA(3, res, i, res_inst, j, &access_cntrl_owner, sizeof(access_cntrl_owner));

	inst.resources = res;
	inst.resource_count = i;

	LOG_DBG("Create Location instance: %d", obj_inst_id);

	return &inst;
}

static int ipso_access_control_init(const struct device *dev)
{
	int ret;
	struct lwm2m_engine_obj_inst *obj_inst = NULL;

	access_control.obj_id = ACCESS_CONTROL_OBJECT_ID;
	access_control.version_major = ACCESS_CONTROL_VERSION_MAJOR;
	access_control.version_minor = ACCESS_CONTROL_VERSION_MINOR;
	access_control.is_core = true;
	access_control.fields = fields;
	access_control.field_count = ARRAY_SIZE(fields);
	access_control.max_instance_count = 1U;
	access_control.create_cb = access_control_create;
	lwm2m_register_obj(&access_control);

	/* auto create the only instance */
	ret = lwm2m_create_obj_inst(ACCESS_CONTROL_OBJECT_ID, 0, &obj_inst);
	if (ret < 0) {
		LOG_DBG("Create LWM2M instance 0 error: %d", ret);
	}

	acl;
	acl2 = 101;

	lwm2m_engine_create_res_inst("2/0/2/0");
	lwm2m_engine_create_res_inst("2/0/2/1");

	lwm2m_engine_set_res_data("2/0/2/0", &acl, sizeof(acl), 0);
	lwm2m_engine_set_res_data("2/0/2/1", &acl2, sizeof(acl2), 0);

	return ret;
}

SYS_INIT(ipso_access_control_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
