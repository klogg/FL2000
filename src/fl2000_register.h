// fl2000_register.h
//
// (c)Copyright 20017, Fresco Logic, Incorporated.
//
// The contents of this file are property of Fresco Logic, Incorporated and are strictly protected
// by Non Disclosure Agreements. Distribution in any form to unauthorized parties is strictly prohibited.
//
// Purpose:
//

#ifndef _FL2000_REGISTER_H_
#define _FL2000_REGISTER_H_

#define FL2K_REG_INT_STATUS	0x8000		/*  INT Status  ? */
#define FL2K_REG_FORMAT		0x8004		/*  Picture format / Colormode  ? */
#define FL2K_REG_H_SYNC1	0x8008		/*  h_sync_reg_1 */
#define FL2K_REG_H_SYNC2	0x800C		/*  h_sync_reg_2 */
#define FL2K_REG_V_SYNC1	0x8010		/*  v_sync_reg_1 */
#define FL2K_REG_V_SYNC2	0x8014		/*  v_sync_reg_2 */
#define REG_OFFSET_8018		0x8018		/*  unknwon  */
#define FL2K_REG_ISO_CTRL	0x801C		/*  ISO ? reg 14 bit value ?  */
#define FL2K_REG_I2C_CTRL	0x8020		/*  I2C Controller and I2C send */
#define FL2K_REG_I2C_DATA_RD	0x8024		/*  I2C read reg, 32 bit wide */
#define FL2K_REG_I2C_DATA_WR	0x8028		/*  I2C write reg, 2 bit wide */
#define FL2K_REG_PLL		0x802C		/*  PLL regster  */
#define REG_OFFSET_8030		0x8030		/*  unknown  */
#define REG_OFFSET_8034		0x8034		/*  unknown  */
#define REG_OFFSET_8038		0x8038		/*  unknown  */
#define FL2K_REG_INT_CTRL	0x803C		/* INT control */
#define REG_OFFSET_8040		0x8040		/*  unknown */
#define REG_OFFSET_8044		0x8044		/*  unknown  */
#define REG_OFFSET_8048		0x8048		/*  App reset  */
#define REG_OFFSET_804C		0x804C		/*  unknown  */
#define REG_OFFSET_8050		0x8050		/*  unknown  */
#define REG_OFFSET_8054		0x8054		/*  unknown  */
#define REG_OFFSET_8058		0x8058		/*  unknown  */
#define REG_OFFSET_805C		0x805C		/*  unknown  */
#define REG_OFFSET_8064		0x8064		/*  unknown  */
#define REG_OFFSET_8070		0x8070		/*  unknown  */
#define REG_OFFSET_8074		0x8074		/*  unknown  */
#define REG_OFFSET_8078		0x8078		/*  unknown  */
#define REG_OFFSET_807C		0x807C		/*  unknown  */
#define REG_OFFSET_8088		0x8088

#define REG_OFFSET_0070		0x0070		/* unknown  */
#define REG_OFFSET_0078		0x0078		/* unknown */

bool fl2000_reg_write(
	struct dev_ctx * dev_ctx,
	uint32_t offset,
	uint32_t* data);

bool fl2000_reg_read(
	struct dev_ctx * dev_ctx,
	uint32_t offset,
	uint32_t* data);

bool fl2000_reg_check_bit(
	struct dev_ctx * dev_ctx,
	uint32_t offset,
	uint32_t bit_offset);

void fl2000_reg_bit_set(
	struct dev_ctx * dev_ctx,
	uint32_t offset,
	uint32_t bit_offset);

void fl2000_reg_bit_clear(
	struct dev_ctx * dev_ctx,
	uint32_t offset,
	uint32_t bit_offset);

#endif // _FL2000_REGISTER_H_

// eof: fl2000_register.h
//
