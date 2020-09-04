#ifndef	SPI_AD_H
#define	SPI_AD_H

#include <linux/spi/spidev.h>

typedef struct {
	int32_t		fd;
	uint32_t	mode;						// spi mode
	uint8_t		bits;						// bits per word
	uint32_t	speed;						// speed (Hz)
} spi_setting;

/* RETURN STATUS */
#define	RET_ERR					1
#define	RET_NORM				0

/* SPI SLAVES */
#define	DEV_AD5752 				0
#define	DEV_ADS8684A			1

/* TRANSFER BYTES */
#define	AD5752_REG_BYTE 		3			// 24 bits
#define	ADS8684A_REG_BYTE		(2 * 2)		// 16 bits * 2

#define	ADS8684A_BYTE_H			2
#define	ADS8684A_BYTE_L			3

/*-------------------------------------------------------------------------*/
/*                               AD5752: DAC                               */
/*-------------------------------------------------------------------------*/
/* REGISTER ADDRESS [regAddr] */
#define	DAC_REG					0x00
#define	OUT_REG					0x01
#define	POW_REG					0x02
#define	CTRL_REG				0x03

/* DAC ADDRESS BITS for DAC_REG, OUT_REG [dacAddr] */
#define	DAC_A					0x00
#define	DAC_B					0x02
#define	DAC_AB					0x04

/* DAC ADDRESS BITS for CTRL_REG [dacAddr] */
#define	CTRL_NOP				0x00		// no operation instruction used in readback operations
#define	CTRL_INSTR				0x01		// combine with instruction(s)
#define	CTRL_CLEAR				0x04		// clear DAC values (as defined by CLR instruction)
#define	CTRL_LOAD				0x05		// load new DAC values

/* OUTPUT RANGE for OUT_REG [data] */
#define	OUT_P5					0x00		// +5V
#define	OUT_P10					0x01		// +10V
#define	OUT_P10_8				0x02		// +10.8V
#define	OUT_PN5					0x03		// ±5V
#define	OUT_PN10				0x04		// ±10V
#define	OUT_PN10_8				0x05		// ±10.5V

/* POWER CONTROL for POW_REG [data] */
#define	POW_PU_A				0x0001
#define	POW_PU_B				0x0004
#define	POW_TSD					0x0020
#define	POW_OC_A				0x0080
#define	POW_OC_B				0x0200

/*-------------------------------------------------------------------------*/
/*                              ADS8684A: ADC                              */
/*-------------------------------------------------------------------------*/
/* COMMAND REGISTER ADDRESS [regAddr] */
// [data] is all 0's
#define	NO_OP					0x00
#define	STDBY					0x82
#define	PWR_DN					0x83
#define	RST						0x85
#define	AUTO_RST				0xA0
#define	MAN_CH_0				0xC0
#define	MAN_CH_1				0xC4
#define	MAN_CH_2				0xC8
#define	MAN_CH_3				0xCC
#define	MAN_CH_4				0xD0
#define	MAN_CH_5				0xD4
#define	MAN_CH_6				0xD8
#define	MAN_CH_7				0xDC
#define	MAN_AUX					0xE0

/* PROGRAM REGISTER ADDRESS [regAddr] */
// ch = [0, 7]
// AUTO SCAN SEQUENCING CONTROL
#define	AUTO_EN_REG				0x01
#define	CH_PD_REG				0x02
// DEVICE FEATURES SELECTION CONTROL
#define	FEATURE_REG				0x03
// RANGE SELECT REGISTERS
#define	IN_RNG_REG(ch)			(0x05 + (uint8_t)ch)
// ALARM FLAG REGISTERS (Read-Only)
#define	ALARM_OV_TRIP_REG		0x10
#define	ALARM_CH03_TRIP_REG		0x11
#define	ALARM_CH03_ACT_REG		0x12
#define	ALARM_CH47_TRIP_REG		0x13
#define	ALARM_CH47_ACT_REG		0x14
// ALARM THRESHOLD REGISTERS
#define	HYSTERESIS_REG(ch)		(0x15 + (uint8_t)(ch * 5))
#define	H_THRES_MSB_REG(ch)		(0x16 + (uint8_t)(ch * 5))
#define	H_THRES_LSB_REG(ch)		(0x17 + (uint8_t)(ch * 5))
#define	L_THRES_MSB_REG(ch)		(0x18 + (uint8_t)(ch * 5))
#define	L_THRES_LSB_REG(ch)		(0x19 + (uint8_t)(ch * 5))
// COMMAND READ BACK (Read-Only)
#define	CMD_READBACK_REG		0x3F

/* WRITE/READ [rw] */
// for program registers
#define	PROG_REG_R				0x00
#define	PROG_REG_W				0x01

/*-------------------------------------------------------------------------*/
/*                                FUNCTION                                 */
/*-------------------------------------------------------------------------*/
int	spi_open(spi_setting *spi_set, char *device);
int	spi_close(spi_setting *spi_set);

int	spi_transfer_da(spi_setting spi_set, uint8_t regAddr, uint8_t dacAddr, uint16_t data, uint8_t *rx);
int	spi_transfer_ad(spi_setting spi_set, uint8_t rw, uint8_t regAddr, uint8_t data, uint8_t *rx);

uint16_t cal_digital_pm10(float ref_in, float analog);
float cal_analog(uint16_t digital);

#endif
