/*
 * Copyright 2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */

#include "core_types.h"
#include "dce/dce_hwseq.h"
#include "dcn301_hwseq.h"
#include "reg_helper.h"

#define DC_LOGGER_INIT(logger)

#define CTX \
	hws->ctx
#define REG(reg)\
	hws->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	hws->shifts->field_name, hws->masks->field_name

static enum bp_result link_transmitter_control(
		struct dc_bios *bios,
	struct bp_transmitter_control *cntl)
{
	enum bp_result result;

	result = bios->funcs->transmitter_control(bios, cntl);

	return result;
}

void dcn301_edp_power_control(
		struct dc_link *link,
		bool power_up)
{
	struct dc_context *ctx = link->ctx;
	struct bp_transmitter_control cntl = { 0 };
	enum bp_result bp_result;
	uint8_t panel_instance;
	static const char DPSinkDevString_7580[] = {'7','5','8','0',0x80,'u'};

	if (dal_graphics_object_id_get_connector_id(link->link_enc->connector)
			!= CONNECTOR_ID_EDP) {
		BREAK_TO_DEBUGGER();
		return;
	}

	if (!link->panel_cntl)
		return;

	if (power_up !=
		link->panel_cntl->funcs->is_panel_powered_on(link->panel_cntl)) {

		unsigned long long current_ts = dm_get_timestamp(ctx);
		unsigned long long time_since_edp_poweroff_ms =
				div64_u64(dm_get_elapse_time_in_ns(
						ctx,
						current_ts,
						link->link_trace.time_stamp.edp_poweroff), 1000000);
		unsigned long long time_since_edp_poweron_ms =
				div64_u64(dm_get_elapse_time_in_ns(
						ctx,
						current_ts,
						link->link_trace.time_stamp.edp_poweron), 1000000);
		DC_LOG_HW_RESUME_S3(
				"%s: transition: power_up=%d current_ts=%llu edp_poweroff=%llu edp_poweron=%llu time_since_edp_poweroff_ms=%llu time_since_edp_poweron_ms=%llu",
				__func__,
				power_up,
				current_ts,
				link->link_trace.time_stamp.edp_poweroff,
				link->link_trace.time_stamp.edp_poweron,
				time_since_edp_poweroff_ms,
				time_since_edp_poweron_ms);

		/* Send VBIOS command to prompt eDP panel power */
		if (power_up) {
			/* edp requires a min of 500ms from LCDVDD off to on */

			unsigned long long remaining_min_edp_poweroff_time_ms = 500;

			if (link->link_trace.time_stamp.edp_poweron 
					&& memcmp(&link->dpcd_caps.branch_dev_name, (int8_t *)(DPSinkDevString_7580), 6) == 0) {
				DC_LOG_DC("eDP power is on. 500ms eDP poweroff delay is not needed on 7580 device\n");
				remaining_min_edp_poweroff_time_ms = 0;
			}

			/* add time defined by a patch, if any (usually patch extra_t12_ms is 0) */
			if (link->local_sink != NULL)
				remaining_min_edp_poweroff_time_ms +=
					link->local_sink->edid_caps.panel_patch.extra_t12_ms;

			/* Adjust remaining_min_edp_poweroff_time_ms if this is not the first time. */
			if (link->link_trace.time_stamp.edp_poweroff != 0) {
				if (time_since_edp_poweroff_ms < remaining_min_edp_poweroff_time_ms)
					remaining_min_edp_poweroff_time_ms =
						remaining_min_edp_poweroff_time_ms - time_since_edp_poweroff_ms;
				else
					remaining_min_edp_poweroff_time_ms = 0;
			}

			if (remaining_min_edp_poweroff_time_ms) {
				DC_LOG_HW_RESUME_S3(
						"%s: remaining_min_edp_poweroff_time_ms=%llu: begin wait.\n",
						__func__, remaining_min_edp_poweroff_time_ms);
				msleep(remaining_min_edp_poweroff_time_ms);
				DC_LOG_HW_RESUME_S3(
						"%s: remaining_min_edp_poweroff_time_ms=%llu: end wait.\n",
						__func__, remaining_min_edp_poweroff_time_ms);
				dm_output_to_console("%s: wait %lld ms to power on eDP.\n",
						__func__, remaining_min_edp_poweroff_time_ms);
			} else {
				DC_LOG_HW_RESUME_S3(
						"%s: remaining_min_edp_poweroff_time_ms=%llu: no wait required.\n",
						__func__, remaining_min_edp_poweroff_time_ms);
			}
		}

		DC_LOG_HW_RESUME_S3(
				"%s: BEGIN: Panel Power action: %s\n",
				__func__, (power_up ? "On":"Off"));

		cntl.action = power_up ?
			TRANSMITTER_CONTROL_POWER_ON :
			TRANSMITTER_CONTROL_POWER_OFF;

		if (memcmp(&link->dpcd_caps.branch_dev_name, (int8_t *)(DPSinkDevString_7580), 6) == 0) {
			DC_LOG_DC("7580 device is capable of running without powering off power control\n");
			cntl.action = TRANSMITTER_CONTROL_POWER_ON; // Don't power off pwr cntl on 7580 device
		}

		cntl.transmitter = link->link_enc->transmitter;
		cntl.connector_obj_id = link->link_enc->connector;
		cntl.coherent = false;
		cntl.lanes_number = LANE_COUNT_FOUR;
		cntl.hpd_sel = link->link_enc->hpd_source;
		panel_instance = link->panel_cntl->inst;

		if (ctx->dc->ctx->dmub_srv &&
				ctx->dc->debug.dmub_command_table) {
			if (cntl.action == TRANSMITTER_CONTROL_POWER_ON)
				bp_result = ctx->dc_bios->funcs->enable_lvtma_control(ctx->dc_bios,
						LVTMA_CONTROL_POWER_ON,
						panel_instance);
			else
				bp_result = ctx->dc_bios->funcs->enable_lvtma_control(ctx->dc_bios,
						LVTMA_CONTROL_POWER_OFF,
						panel_instance);
		}

		bp_result = link_transmitter_control(ctx->dc_bios, &cntl);

		DC_LOG_HW_RESUME_S3(
				"%s: END: Panel Power action: %s bp_result=%u\n",
				__func__, (power_up ? "On":"Off"),
				bp_result);

		if (!power_up)
			/*save driver power off time stamp*/
			link->link_trace.time_stamp.edp_poweroff = dm_get_timestamp(ctx);
		else
			link->link_trace.time_stamp.edp_poweron = dm_get_timestamp(ctx);

		DC_LOG_HW_RESUME_S3(
				"%s: updated values: edp_poweroff=%llu edp_poweron=%llu\n",
				__func__,
				link->link_trace.time_stamp.edp_poweroff,
				link->link_trace.time_stamp.edp_poweron);

		if (bp_result != BP_RESULT_OK)
			DC_LOG_ERROR(
					"%s: Panel Power bp_result: %d\n",
					__func__, bp_result);
	} else {
		DC_LOG_HW_RESUME_S3(
				"%s: Skipping Panel Power action: %s\n",
				__func__, (power_up ? "On":"Off"));
	}
}

