// fl2000_dongle.c
//
// (c)Copyright 2009-2013, Fresco Logic, Incorporated.
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

void fl2000_dongle_init_fl2000dx(struct dev_ctx * dev_ctx)
{
	// Enable interrupt for I2C detection and external monitor.
	//

	int ret;
	uint32_t value;

	/* ULLI : I2C interrupt controller init, should be done in i2c driver */

	ret = fl2000_reg_read(dev_ctx, REG_OFFSET_8020, &value);
	if (ret < 0)
		return;

	value |= BIT(30);	/* Enable I2C VGA Detection */
	value |= BIT(28);	/* Enable external monitor detection */

	fl2000_reg_write(dev_ctx, REG_OFFSET_8020, &value);

	// BUG: We turn-off hardward reset for now.
	// But we do need it for resolve accumulate interrupt packet issue.
	// Got debug with NJ for this problem.
	//

	ret = fl2000_reg_read(dev_ctx, REG_OFFSET_8088, &value);
	if (ret < 0)
		return;

	value &= BIT(10);	/* BUG: We turn-off hardward reset for now. */

	fl2000_reg_write(dev_ctx, REG_OFFSET_8088, &value);

	// Compression registry and flags.
	//
	dev_ctx->registry.CompressionEnable = 0;
	dev_ctx->registry.Usb2PixelFormatTransformCompressionEnable = 1;
}

/////////////////////////////////////////////////////////////////////////////////
// P U B L I C
/////////////////////////////////////////////////////////////////////////////////
//

void fl2000_dongle_u1u2_setup(struct dev_ctx * dev_ctx, bool enable)
{
	int ret;
	uint32_t value;

	ret = fl2000_reg_read(dev_ctx, REG_OFFSET_0070, &value);
	if (ret < 0)
		return;

	if (enable) {
		// Set 0x0070 bit 20 = 0, accept U1.
		// Set 0x0070 bit 19 = 0, accept U2.
		//
		value &= ~(BIT(20) | BIT(19));
	} else {
		// Set 0x0070 bit 20 = 1, reject U1.
		// Set 0x0070 bit 19 = 1, reject U2.
		//
		value |= BIT(20) | BIT(19);
	}

	fl2000_reg_write(dev_ctx, REG_OFFSET_0070, &value);
}

void fl2000_dongle_reset(struct dev_ctx * dev_ctx)
{
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_INIT, ">>>>");

	// REG_OFFSET_8048(0x8048)< bit 15 > = 1, app reset, self clear.
	//
	fl2000_reg_bit_set(dev_ctx, REG_OFFSET_8048, 15);

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_INIT, "<<<<");
}

void fl2000_dongle_stop(struct dev_ctx * dev_ctx)
{
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");
	if (!dev_ctx->usb_dev) {
		// The device is not yet enumerated.
		//
		goto exit;
	}
	fl2000_dongle_reset(dev_ctx);

exit:
    dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
}


int
fl2000_set_display_mode(
	struct dev_ctx * dev_ctx,
	struct display_mode * display_mode)
{
	int ret_val = 0;
	struct vr_params vr_params;
	bool resolution_changed = false;

	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, ">>>>");

	dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
		 "Display information width:%u height:%d.",
		 display_mode->width,
		 display_mode->height);

	if ((dev_ctx->vr_params.width != display_mode->width) ||
	    (dev_ctx->vr_params.height != display_mode->height))
		resolution_changed = true;

	fl2000_render_stop(dev_ctx);
	fl2000_dongle_stop(dev_ctx);

	/*
	 * user want to turn off monitor
	 */
	if (display_mode->width == 0 && display_mode->height == 0)
	    goto exit;

	memset(&vr_params, 0, sizeof(struct vr_params));

	vr_params.width = display_mode->width;
	vr_params.height = display_mode->height;
	vr_params.freq = 60;
	switch (display_mode->input_color_format) {
	case COLOR_FORMAT_RGB_24:
		vr_params.input_bytes_per_pixel = 3;
		break;

	case COLOR_FORMAT_RGB_16_565:
	default:
		vr_params.input_bytes_per_pixel = 2;
		break;
	}

	switch (display_mode->output_color_format) {
	case COLOR_FORMAT_RGB_24:
		vr_params.output_image_type = OUTPUT_IMAGE_TYPE_RGB_24;
		break;

	case COLOR_FORMAT_RGB_16_565:
		vr_params.output_image_type = OUTPUT_IMAGE_TYPE_RGB_16;
		vr_params.color_mode_16bit = VR_16_BIT_COLOR_MODE_565;
		break;
	default:
		vr_params.output_image_type = OUTPUT_IMAGE_TYPE_RGB_16;
		vr_params.color_mode_16bit = VR_16_BIT_COLOR_MODE_555;
		break;
	}

	if (IS_DEVICE_USB2LINK(dev_ctx)) {
		/*
		 * Considering physical bw limitation, force frequency to 60Hz
		 * once user select higher frequency from panel.
		 */
		if (60 < vr_params.freq)
			vr_params.freq = 60;

		// If usb2, then force to pixeltransform ( 24->16(555) ) and compression on.
		//
		dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
			"Turn on usb2 compression.");

		vr_params.output_image_type = OUTPUT_IMAGE_TYPE_RGB_16;
		vr_params.color_mode_16bit = VR_16_BIT_COLOR_MODE_555;
		vr_params.use_compression = 1;
	}

	ret_val = fl2000_dongle_set_params(dev_ctx, &vr_params);
	if (ret_val < 0) {
		dbg_msg(TRACE_LEVEL_ERROR, DBG_PNP,
			"[ERR] fl2000_dongle_set_params failed?");
		goto exit;
	}
	fl2000_render_start(dev_ctx);

	if (dev_ctx->hdmi_chip_found)
		fl2000_hdmi_init(dev_ctx, resolution_changed);

exit:
	dbg_msg(TRACE_LEVEL_VERBOSE, DBG_PNP, "<<<<");
	return ret_val;
}

void fl2000_dongle_card_initialize(struct dev_ctx * dev_ctx)
{
	bool hdmi_chip_found;

	fl2000_dongle_reset(dev_ctx);

	hdmi_chip_found = fl2000_hdmi_find_chip(dev_ctx);
	dev_ctx->hdmi_chip_found = hdmi_chip_found;
	if (hdmi_chip_found) {
		dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
			"found ITE hdmi chip, initializing it.");
		fl2000_hdmi_reset(dev_ctx);
		if (!dev_ctx->hdmi_powered_up) {
			fl2000_hdmi_power_up(dev_ctx);
		}
		dbg_msg(TRACE_LEVEL_INFO, DBG_PNP,
			"ITE hdmi chip powered up");
	}
	fl2000_dongle_init_fl2000dx(dev_ctx);
}

// eof: fl2000_dongle.c
//
