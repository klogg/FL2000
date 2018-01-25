// fl2000_monitor.h
//
// (c)Copyright 20017, Fresco Logic, Incorporated.
//
// The contents of this file are property of Fresco Logic, Incorporated and are strictly protected
// by Non Disclosure Agreements. Distribution in any form to unauthorized parties is strictly prohibited.
//
// Purpose:
//

#ifndef _FL2000_MONITOR_H_
#define _FL2000_MONITOR_H_

enum fl2k_monitor_config {
	FL2K_MON_RESET_VGA_CSS = BIT(0),
	FL2K_MON_RGB_565_MODE      = BIT(6),
	FL2K_MON_EXTERNAL_DAC  = BIT(7),     /*  HDMI ? */
	FL2K_MON_COMPRESSION = BIT(24),
	FL2K_MON_8BIT_RGB = BIT(25),         /*  really  only 8 bit, so maybe 222 RGB*/
	FL2K_MON_256COLOR_PALETTE = BIT(26), /*  like old VGA displays ?? */

	FL2K_MON_FIRST_BYTE_MASK = BIT(27),
	FL2K_MON_RESET_DEFAULT = BIT(28),    /*  active low */
	FL2K_MON_RGB_555_MODE    = BIT(31),
};

enum fl2k_usb_config_ctrl {
	FL2K_USB_BIA = BIT(22),					/* USB BIA ??? */
	FL2K_USB_ISO_ERR_INT = BIT(24),				/* USB isoch err interrupt */
	FL2K_USB_HDMI_CTRL = BIT(25),		/* HDMI control for resulution change ? */
	FL2K_USB_ISO_AUTO_RECOVER = (BIT(21) | BIT(19)),	/* 2 bits why */
	FL2K_USB_ISO_FRAME_FEEDBACK = BIT(13),			/* Feedback interrupt ? see REG_OFFSET_8000 */
	FL2K_USB_END_ZERO_BULK = BIT(28),	/* send zero bulk at end of picture frame */
	FL2K_USB_END_PENDIG_BIT = BIT(29),	/* send pending ? bit at end of frame */
};

#define FL2K_USB_END_MASK	GENMASK(29,27)

void fl2000_monitor_read_edid(struct dev_ctx * dev_ctx);

bool fl2000_monitor_resolution_in_white_table(
	uint32_t width,
	uint32_t height,
	uint32_t freq);

void fl2000_monitor_manual_check_connection(struct dev_ctx * dev_ctx);
int fl2000_dongle_set_params(struct dev_ctx * dev_ctx, struct vr_params * vr_params);
void fl2000_monitor_vga_status_handler(
	struct dev_ctx * dev_ctx, uint32_t raw_status
	);

#endif // _FL2000_MONITOR_H_

// eof: fl2000_monitor.h
//
