// fl2000_monitor.c
//
// (c)Copyright 20017, Fresco Logic, Incorporated.
//
// The contents of this file are property of Fresco Logic, Incorporated and are strictly protected
// by Non Disclosure Agreements. Distribution in any form to unauthorized parties is strictly prohibited.
//
// Purpose:
//

#include "fl2000_include.h"

/////////////////////////////////////////////////////////////////////////////////
// P R I V A T E
/////////////////////////////////////////////////////////////////////////////////
//
void fl2000_monitor_ratio_to_dimension(
	uint8_t x,
	uint8_t aspect_ratio,
	uint32_t* width,
	uint32_t* height)
{
	uint32_t temp_width;
	uint32_t temp_height;

	temp_width = (x + 31) * 8;
	switch (aspect_ratio) {
	case IMAGE_ASPECT_RATIO_16_10:
		temp_height = (temp_width / 16) * 10;
		break;

	case IMAGE_ASPECT_RATIO_4_3:
		temp_height = (temp_width / 4) * 3;
		break;

	case IMAGE_ASPECT_RATIO_5_4:
	    temp_height = (temp_width / 5) * 4;
	    break;

	case IMAGE_ASPECT_RATIO_16_9:
	default:
	    temp_height = ( temp_width / 16 ) * 9;
	    break;

	}

	*width = temp_width;
	*height = temp_height;
}

bool fl2000_monitor_read_edid_dsub(struct dev_ctx * dev_ctx)
{
	uint8_t index;
	uint32_t data;
	bool ret_val;
	int read_status;

	ret_val = false;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	// EDID Header check.
	//

	for (index = 0; index < EDID_SIZE; index += 4) {
		read_status = fl2000_i2c_read(
			dev_ctx, I2C_ADDRESS_DSUB, (uint8_t) index, &data);
		if (read_status < 0) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"ERROR Read Edid table failed.");
			goto exit;
		}

		memcpy(&dev_ctx->monitor_edid[0][index], &data, 4);

		// Because I2C is slow, we have to delay a while.
		//
		DELAY_MS(10);
	}
	ret_val = true;

exit:
    return ret_val;
}


/* ULLI : Fresco Logic does some verify after write monitor register
 * add some helper to simplify/reduce code
 */

int _fl2000_reg_write_verify(struct dev_ctx * dev_ctx, uint32_t offset,
			     uint32_t *data)
{
	int ret;
	uint32_t read_back = 0;

	ret = fl2000_reg_write(dev_ctx, offset, data);
	if (ret < 0)
		return ret;

	ret = fl2000_reg_read(dev_ctx, offset, &read_back);
	if (ret < 0)
		return ret;

	if (*data != read_back)
		return -1;

	return 0;
}

static void _fl2000_set_video_mode(struct dev_ctx * dev_ctx)
{
	int ret;
	uint32_t value;

	// REG_OFFSET_8004

	ret = fl2000_reg_read(dev_ctx, FL2K_REG_FORMAT, &value);
	if (ret < 0)
		return;

	// Clear bit 28, Default setting.
	//

	value &= ~FL2K_MON_RESET_DEFAULT;

	// Clear bit 6( 565 ) & 31( 555 ), 16 bit color mode.

	value &= ~(FL2K_MON_RGB_565_MODE | FL2K_MON_RGB_555_MODE);

	// Clear bit 24, Disable compression.

	value &= ~FL2K_MON_COMPRESSION;

	// Clear bit 25, Disable 8 bit color mode.

	value &= ~FL2K_MON_8BIT_RGB;

	// Clear bit 26, Disable 256 color palette.

	value &= ~FL2K_MON_256COLOR_PALETTE;

	// Clear bit 27, Disable first byte mask.

	value &= ~FL2K_MON_FIRST_BYTE_MASK;

	// Set bit 0, Reset VGA CCS.

	value |= FL2K_MON_RESET_VGA_CSS;

	if (dev_ctx->vr_params.use_compression) {
		// Set bit 24, Enable compression mode.
		
		value |= FL2K_MON_COMPRESSION;
	}

	if (OUTPUT_IMAGE_TYPE_RGB_16 == dev_ctx->vr_params.output_image_type) {
		if (VR_16_BIT_COLOR_MODE_555 ==
		    dev_ctx->vr_params.color_mode_16bit) {
			// Bit 31 for 555 mode.
			//
			value |= FL2K_MON_RGB_555_MODE;
		}
		else {
			// Bit 6 for 565 mode.
			//
			value |= FL2K_MON_RGB_565_MODE;
		}
	}
#if 0	/* ULLI : disabled, code is kept here only for consistently */	
	else if (OUTPUT_IMAGE_TYPE_RGB_8 ==
		 dev_ctx->vr_params.output_image_type) {
		// Bit 25 for enable eight bit color mode.
		//
		value |= FL2K_MON_8BIT_RGB;
	}
#endif

	// External DAC Control
	//
	// Set bit 7, Enable external DAC.
	//
	value |= FL2K_MON_EXTERNAL_DAC;

	ret = fl2000_reg_write(dev_ctx, FL2K_REG_FORMAT, &value);
}

static void _fl2000_set_intrl_ctrl(struct dev_ctx * dev_ctx)
{
	int ret;
	uint32_t value;

	// REG_OFFSET_803C
	//

	ret = fl2000_reg_read(dev_ctx, FL2K_REG_INT_CTRL, &value);
	if (ret < 0)
		return;

	// Clear bit 22 - Disable BIA.
	
	value &= ~FL2K_USB_BIA;

	// Clear bit 24 - Disable isoch error interrupt.

	value &= ~FL2K_USB_ISO_ERR_INT;

	// Clear bit 19,21 - Disable isoch auto recover.

	value &= ~FL2K_USB_ISO_AUTO_RECOVER;

	// Clear bit 13 - Disable isoch feedback interrupt.

	value &= ~FL2K_USB_ISO_FRAME_FEEDBACK;

	// Clear bit 27:29 - End Of Frame Type

	value &= ~FL2K_USB_END_MASK;

#if 0	/* ULLI : remains only as remark */
	if (dev_ctx->vr_params.end_of_frame_type == EOF_ZERO_LENGTH) {
		// Zero Length Bulk.
		//
#endif
		value |= FL2K_USB_END_ZERO_BULK;
#if 0	/* ULLI : remains only as remark */
	}
	else  {
		// Pending Bit.
		//
		value |= FL2K_USB_END_PENDIG_BIT;
	}
#endif

	ret = fl2000_reg_write(dev_ctx, FL2K_REG_INT_CTRL, &value);

}

static int _fl2000_set_video_timing(struct dev_ctx * dev_ctx,
				    struct fl2000_timing_entry const * entry)
{
	bool ret_val;
	uint32_t h_sync_reg_1 = entry->h_sync_reg_1;
	uint32_t h_sync_reg_2 = entry->h_sync_reg_2;
	uint32_t v_sync_reg_1 = entry->v_sync_reg_1;
	uint32_t v_sync_reg_2 = entry->v_sync_reg_2;

	ret_val = true;

	if (dev_ctx->hdmi_chip_found) {
	        if (dev_ctx->vr_params.width == 640 &&
	            dev_ctx->vr_params.height == 480 &&
	            dev_ctx->vr_params.freq == 60) {
			h_sync_reg_2 = 0x600091;
	                v_sync_reg_2 = 0x2420024;
	        } else if (dev_ctx->vr_params.width == 1280 &&
	                   dev_ctx->vr_params.height == 720 &&
	                   dev_ctx->vr_params.freq == 60) {
	                v_sync_reg_2 = 0x1A5001A;
	        } else {
	                // No adjustment.
	                //
	        }
	}

	// REG_OFFSET_8008
	//
	if (_fl2000_reg_write_verify(dev_ctx, FL2K_REG_H_SYNC1, &h_sync_reg_1)) {
		ret_val = false;
		goto exit;
	}

	// REG_OFFSET_800C
	//
	if (_fl2000_reg_write_verify(dev_ctx, FL2K_REG_H_SYNC2, &h_sync_reg_2)) {
		ret_val = false;
		goto exit;
	}

	// REG_OFFSET_8010
	//
	if (_fl2000_reg_write_verify(dev_ctx, FL2K_REG_V_SYNC1, &v_sync_reg_1)) {
		ret_val = false;
		goto exit;
	}

	// REG_OFFSET_8014
	//
	if (_fl2000_reg_write_verify(dev_ctx, FL2K_REG_V_SYNC2, &v_sync_reg_2)) {
		ret_val = false;
		goto exit;
	}

exit:
	return ret_val;
}

/////////////////////////////////////////////////////////////////////////////////
// P U B L I C
/////////////////////////////////////////////////////////////////////////////////
//

static bool fl2000_monitor_set_resolution(struct dev_ctx * dev_ctx, bool pll_changed,
					  struct fl2000_timing_entry const * entry)
{
	uint32_t data;
	bool ret_val;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");
	ret_val = true;

	if (pll_changed) {
		// REG_OFFSET_802C
		//
		data = dev_ctx->vr_params.pll_reg;
		fl2000_reg_write(dev_ctx, FL2K_REG_PLL, &data);
	}

	// REG_OFFSET_8048 ( 0x8048 )< bit 15 > = 1, app reset, self clear.
	//
	fl2000_reg_bit_set(dev_ctx, REG_OFFSET_8048, 15);

	// Confirm PLL setting.
	//
	data = 0;
	fl2000_reg_read(dev_ctx, FL2K_REG_PLL, &data);
	if (dev_ctx->vr_params.pll_reg != data) {
		ret_val = false;
		goto exit;
	}

	_fl2000_set_intrl_ctrl(dev_ctx);
	_fl2000_set_video_mode(dev_ctx);
	_fl2000_set_video_timing(dev_ctx, entry);

	// REG_OFFSET_801C
	//

	// Clear bit 29:16 - Iso Register
	//
	if (fl2000_reg_read(dev_ctx, FL2K_REG_ISO_CTRL, &data)) {
		data &= 0xC000FFFF;
		if (!fl2000_reg_write( dev_ctx, FL2K_REG_ISO_CTRL, &data)) {
			ret_val = false;
			goto exit;
		}
	}

	fl2000_reg_bit_set(dev_ctx, REG_OFFSET_0070, 13);

exit:
    dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");

    return ret_val;
}

int
fl2000_dongle_set_params(struct dev_ctx * dev_ctx, struct vr_params * vr_params)
{
	int ret_val;
	bool ret;
	uint32_t old_pll;
	uint32_t new_pll;
	bool pll_changed;
	struct fl2000_timing_entry const * entry = NULL;
	size_t table_num;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	// FileIO thread references to parameters and need to avoid concurrent access.
	//
	ret_val = 0;
	pll_changed = false;

	// Set PLL register takes long time to stabilize, therefore, we set that only
	// found it's different to previous setting.
	//
	old_pll = dev_ctx->vr_params.pll_reg;
	memcpy(&dev_ctx->vr_params, vr_params, sizeof(struct vr_params));

	dev_ctx->vr_params.pll_reg = old_pll;

	if (dev_ctx->registry.CompressionEnable ||
	    vr_params->use_compression) {
		dev_ctx->vr_params.use_compression = 1;

		dev_ctx->vr_params.compression_mask_index_min = COMPRESSION_MASK_INDEX_MINIMUM;
		dev_ctx->vr_params.compression_mask_index_max = COMPRESSION_MASK_INDEX_MAXIMUM;

		if (dev_ctx->registry.Usb2PixelFormatTransformCompressionEnable) {
			// Bug#6346: Need more aggressive compression mask.
			//
			dev_ctx->vr_params.compression_mask = COMPRESSION_MASK_13_BIT_VALUE;
			dev_ctx->vr_params.compression_mask_index = COMPRESSION_MASK_13_BIT_INDEX;

			// Output is RGB555, and need at most the mask.
			//
			dev_ctx->vr_params.compression_mask_index_min = COMPRESSION_MASK_15_BIT_INDEX;
		}
		else {
			dev_ctx->vr_params.compression_mask = COMPRESSION_MASK_23_BIT_VALUE;
			dev_ctx->vr_params.compression_mask_index = COMPRESSION_MASK_23_BIT_INDEX;
		}
	}

	switch (dev_ctx->vr_params.output_image_type) {
	case OUTPUT_IMAGE_TYPE_RGB_16:
		table_num = VGA_BIG_TABLE_16BIT_R0;
		break;
	case OUTPUT_IMAGE_TYPE_RGB_24:
	default:
		table_num = VGA_BIG_TABLE_24BIT_R0;
		break;
	}

	entry = fl2000_table_get_entry(
		table_num,
		dev_ctx->vr_params.width,
		dev_ctx->vr_params.height,
		dev_ctx->vr_params.freq);
	if (entry == NULL) {
			dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
				"ERROR fl2000_table_get_entry failed.");
			ret_val = -EINVAL;
			goto exit;
		}

	dev_ctx->vr_params.h_total_time = entry->h_total_time;
	dev_ctx->vr_params.v_total_time = entry->v_total_time;

	new_pll = entry->bulk_asic_pll;

	if (new_pll != dev_ctx->vr_params.pll_reg) {
	    pll_changed = true;
	    dev_ctx->vr_params.pll_reg = new_pll;
	}

	ret = fl2000_monitor_set_resolution(dev_ctx, pll_changed, entry);
	if (!ret) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"[ERR] fl2000_monitor_set_resolution failed?");
		ret_val = -EIO;
		goto exit;
	}

	// Select Interface
	//
	ret_val = fl2000_dev_select_interface(dev_ctx);
	if (ret_val < 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"ERROR fl2000_dev_select_interface failed?");
		goto exit;
	}

	dev_ctx->usb_pipe_bulk_out = usb_sndbulkpipe(dev_ctx->usb_dev, 1);

exit:
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
	return (ret_val);
}

void fl2000_monitor_read_edid(struct dev_ctx * dev_ctx)
{
	uint8_t index;
	uint8_t check_sum;
	bool edid_ok;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	// Try to read EDID from two places:
	// 1. DSUB EDID
	// 2. HDMI EDID
	//
	if (dev_ctx->hdmi_chip_found) {
		unsigned int	i;
		uint8_t num_ext;

		// read the block 0 first, then determine the number of extensions
		// at offset 126.
		edid_ok = fl2000_hdmi_read_block(dev_ctx, 0);
		if (!edid_ok)
			goto edid_exit;

		num_ext = dev_ctx->monitor_edid[0][126];
		dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
			"%u EDID extensions found", num_ext);

		// ignore num_ext if greater than 7.
		if (num_ext > 7)
			num_ext = 0;
		for (i = 0; i < num_ext; i++) {
			bool read_ok;

			read_ok = fl2000_hdmi_read_block(dev_ctx, i + 1);
			dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
				"block[%u] %s", i + 1, read_ok ? "ok" : "failed");
			if (!read_ok)
				break;
		}

	}
	else {
		edid_ok = fl2000_monitor_read_edid_dsub(dev_ctx);
	}

edid_exit:
	if (!edid_ok) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"ERROR Read DSUB Edid table failed.");

		// Can't get correct EDID table from I2C
		//
		memset(dev_ctx->monitor_edid[0], 0, EDID_SIZE);
		goto exit;
	}

	check_sum = 0;
	for (index = 0; index < (EDID_SIZE - 1); index++)
	    check_sum += dev_ctx->monitor_edid[0][index];

	check_sum = -check_sum;
	dev_ctx->monitor_edid[0][127] = check_sum;

exit:
    dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}

void
fl2000_monitor_plugin_handler(
	struct dev_ctx * dev_ctx,
	bool external_connected,
	bool edid_connected)
{
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	// Bug #6147 - After hot plug VGA connector, the monitor can't display
	// We need mutex to protect plug-in and plug-out procedure.
	// Just to prevent the U1U2 step is not synchronize for each plug-in and plug-out.
	//

	// Monitor Plug-In flag.
	//
	dev_ctx->monitor_plugged_in = true;

	// Per NJ's description:
	// Register 0x78 bit17 is used to control a bug where we did not wake up U1/U2 even
	// when NRDY has been sent and ERDY is not yet due to OBUF not ready.
	// After it is ready, we do not wake up from U1/U2.
	// This Register bit should be set to 1
	// because otherwise it causes U1 exit too frequently when there is no monitor.
	//
	if (CARD_NAME_FL2000DX == dev_ctx->card_name)
		fl2000_reg_bit_clear(dev_ctx, REG_OFFSET_0078, 17);

	// Disable U1U2
	//
	fl2000_dongle_u1u2_setup(dev_ctx, false);

	memset(dev_ctx->monitor_edid, 0, sizeof(dev_ctx->monitor_edid));

	// Get EDID table.
	//
	fl2000_monitor_read_edid(dev_ctx);

	dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
		"Notify system to add monitor.");

	// wake up a sleeping process.
	//
	if (waitqueue_active(&dev_ctx->ioctl_wait_q))
		wake_up_interruptible(&dev_ctx->ioctl_wait_q);

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}

void
fl2000_monitor_plugout_handler(
	struct dev_ctx * dev_ctx
	)
{
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	/*
	 * Bug #6147 - After hot plug VGA connector, the monitor can't display
	 * We need mutex to protect plug-in and plug-out procedure.
	 * Just to prevent the U1U2 step is not synchronize for each plug-in and plug-out.
	 */
	dev_ctx->monitor_plugged_in = false;

	dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
		"Notify system to delete monitor.");

	// wake up any sleeping process.
	//
	if (waitqueue_active(&dev_ctx->ioctl_wait_q))
		wake_up_interruptible(&dev_ctx->ioctl_wait_q);

	/*
	 * Stop Thread, but don't do hardware reset to VGA dongle.
	 */
	fl2000_render_stop(dev_ctx);

	memset(dev_ctx->monitor_edid, 0, sizeof(dev_ctx->monitor_edid));

	// Bug #6167 : DUT screen black after S4
	// Because our dongle will compare PLL value for reduce the bootup time.
	// But for S3/S4 non-powered platform, the dongle will be toggled.
	// So we got to cleanup internal PLL value and let it at least set again at boot time.
	//
	dev_ctx->vr_params.pll_reg = 0;

	// Bug #6514 : The monitor back light is still on at FL2000 side when system shutdown
	// Turn off "Force PLL always on".
	// TODO: FL2000DX should not need this step per Stanley's description.
	//       This maybe hardware issue, and Jun is checking now.
	//
	fl2000_reg_bit_clear(dev_ctx, FL2K_REG_INT_CTRL, 26);

	// Per NJ's description:
	// Register 0x78 bit17 is used to control a bug where we did not wake up U1/U2 even
	// when NRDY has been sent and ERDY is not yet due to OBUF not ready.
	// After it is ready, we do not wake up from U1/U2.
	// This Register bit should be set to 1
	// because otherwise it causes U1 exit too frequently when there is no monitor.
	//
	if (CARD_NAME_FL2000DX == dev_ctx->card_name)
		fl2000_reg_bit_set(dev_ctx, REG_OFFSET_0078, 17);

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}

void
fl2000_monitor_vga_status_handler(
	struct dev_ctx * dev_ctx,
	uint32_t raw_status)
{
	struct vga_status *  vga_status;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	dev_info(&dev_ctx->usb_dev->dev, "FL2000 interrupt status word %08x",
		 raw_status);

	vga_status = (struct vga_status *) &raw_status;
	if (vga_status->connected) {
		/*
		 * not previously connected
		 */
		if (!dev_ctx->monitor_plugged_in) {
			bool external_connected;
			bool edid_connected;

			if ( vga_status->ext_mon_connected )
				external_connected = true;
			else
				external_connected = false;

			if (vga_status->edid_connected)
				edid_connected = true;
			else
				edid_connected = false;

			fl2000_monitor_plugin_handler(
				dev_ctx,
				external_connected,
				edid_connected);
		}
		else {
			dbg_msg(TRACE_LEVEL_WARNING, DBG_PNP,
				"WARNING Ignore MonitorPlugin event");
		}
	}
	else {
		// Monitor Plug Out.
		//
		if (dev_ctx->monitor_plugged_in)
			fl2000_monitor_plugout_handler(dev_ctx);
		else
			dbg_msg(TRACE_LEVEL_WARNING, DBG_PNP,
				"Ignore MonitorPlugout event, monitor not attached.");
	}

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}

void
fl2000_monitor_manual_check_connection(struct dev_ctx * dev_ctx)
{
	uint32_t data;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	data = 0;
	if (fl2000_reg_read(dev_ctx, FL2K_REG_INT_STATUS, &data)) {
		fl2000_monitor_vga_status_handler(dev_ctx, data);
	}

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}

// eof: fl2000_monitor.c
//
