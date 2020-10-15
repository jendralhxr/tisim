#ifndef	GPIO_H
#define	GPIO_H

/* RETURN STATUS */
#define	GPIO_ERR		1
#define	GPIO_NORM		0

/* GPIO DIRECTION */
#define	GPIO_IN			0
#define	GPIO_OUT		1

/* GPIO PIN */						// JETSON TX2 HEADER PINOUT
#define	SPI1_MOSI		"gpio429"	// pin 19
#define	SPI1_MISO		"gpio428"	// pin 21
#define	SPI1_SCK		"gpio427"	// pin 23
#define	SPI1_CSO		"gpio430"	// pin 24

#define	DA_CLR			"gpio296"	// pin 16 (#CLR)
#define	DA_2SCOMP		"gpio398"	// pin 29 (BIM/#2sCOMP)
#define	DA_LDAC			"gpio298"	// pin 31 (#LDAC)

#define	AD_CS			"gpio481"	// pin 18 (#CS)
#define	AD_RST_PD		"gpio297"	// pin 32 (#RST/#PD)
#define	AD_ALARM		"gpio389"	// pin 33 (ALARM)

#define	PINNAME_MAX		50

/* FUNCTION */
int		gpio_export(int gpio_no);
int		gpio_unexport(int gpio_no);

int		gpio_direction(char *gpio_name, int direction);

int		gpio_set_value(char *gpio_name, int value);
int		gpio_get_value(char *gpio_name);

void	set_gpio_export();
void	set_gpio_init();
void	set_gpio_unexport();

#endif