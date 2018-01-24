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
