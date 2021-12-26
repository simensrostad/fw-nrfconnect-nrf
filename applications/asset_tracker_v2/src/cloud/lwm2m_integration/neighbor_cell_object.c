/*
 * Copyright (c) 2019 Foundries.io
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME app_lwm2m_obj_neighbor_cell
#define LOG_LEVEL CONFIG_CLOUD_INTEGRATION_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <init.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"

#define NCELL_VERSION_MAJOR 1
#define NCELL_VERSION_MINOR 0

#define NCELL_OBJECT_ID	3371

/* resource IDs */
#define NCELL_SYS_FRAME_NUMBER_ID 6037
#define NCELL_SUB_FRAME_NUMBER_ID 6038
#define NCELL_PCI_ID 6034
#define NCELL_RSRP_ID 6035
#define NCELL_RSRQ_ID 6036
#define NCELL_DL_EARFCN_ID 6032

#define NCELL_MAX_ID 7

/*
 * Calculate resource instances as follows:
 * start with NCELL_MAX_ID
 */
#define RESOURCE_INSTANCE_COUNT (NCELL_MAX_ID)

/* resource state */
static double sys_frame_number = 23;
static double sub_frame_number = 23;
static double pci = 23;
static double rsrp = 23;
static int32_t rsrq = 23;

static struct lwm2m_engine_obj ncell;
static struct lwm2m_engine_obj_field fields[] = {
	OBJ_FIELD_DATA(NCELL_SYS_FRAME_NUMBER_ID, R, INT),
	OBJ_FIELD_DATA(NCELL_SUB_FRAME_NUMBER_ID, R, INT),
	OBJ_FIELD_DATA(NCELL_PCI_ID, R, INT),
	OBJ_FIELD_DATA(NCELL_RSRP_ID, R, INT),
	OBJ_FIELD_DATA(NCELL_RSRQ_ID, R, INT),
	OBJ_FIELD_DATA(NCELL_DL_EARFCN_ID, R, INT),
};

static struct lwm2m_engine_obj_inst inst;
static struct lwm2m_engine_res res[NCELL_MAX_ID];
static struct lwm2m_engine_res_inst res_inst[RESOURCE_INSTANCE_COUNT];

static struct lwm2m_engine_obj_inst *ncell_create(uint16_t obj_inst_id)
{
	int i = 0, j = 0;

	if (inst.resource_count) {
		LOG_ERR("Only 1 instance of Location object can exist.");
		return NULL;
	}

	init_res_instance(res_inst, ARRAY_SIZE(res_inst));

	/* initialize instance resource data */
	INIT_OBJ_RES_DATA(NCELL_SYS_FRAME_NUMBER_ID, res, i, res_inst, j, &sys_frame_number, sizeof(sys_frame_number));
	INIT_OBJ_RES_DATA(NCELL_SUB_FRAME_NUMBER_ID, res, i, res_inst, j, &sub_frame_number, sizeof(sub_frame_number));
	INIT_OBJ_RES_DATA(NCELL_PCI_ID, res, i, res_inst, j, &pci, sizeof(pci));
	INIT_OBJ_RES_DATA(NCELL_RSRP_ID, res, i, res_inst, j, &rsrp, sizeof(rsrp));
	INIT_OBJ_RES_DATA(NCELL_RSRQ_ID, res, i, res_inst, j, &rsrq, sizeof(rsrq));
	INIT_OBJ_RES_DATA(NCELL_DL_EARFCN_ID, res, i, res_inst, j, &rsrq, sizeof(rsrq));

	inst.resources = res;
	inst.resource_count = i;

	LOG_DBG("Create Location instance: %d", obj_inst_id);

	return &inst;
}

static int ipso_ncell_init(const struct device *dev)
{
	int ret;
	struct lwm2m_engine_obj_inst *obj_inst = NULL;

	ncell.obj_id = NCELL_OBJECT_ID;
	ncell.version_major = NCELL_VERSION_MAJOR;
	ncell.version_minor = NCELL_VERSION_MINOR;
	ncell.is_core = true;
	ncell.fields = fields;
	ncell.field_count = ARRAY_SIZE(fields);
	ncell.max_instance_count = 1U;
	ncell.create_cb = ncell_create;
	lwm2m_register_obj(&ncell);

	/* auto create the only instance */
	ret = lwm2m_create_obj_inst(NCELL_OBJECT_ID, 0, &obj_inst);
	if (ret < 0) {
		LOG_DBG("Create LWM2M instance 0 error: %d", ret);
	}

	return ret;
}

SYS_INIT(ipso_ncell_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
