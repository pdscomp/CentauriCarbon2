# Main code build rules

ifneq ($(CONFIG_BOOTLOADER),y)
src-y += sched.c command.c basecmd.c debugcmds.c por.c
src-$(CONFIG_HAVE_GPIO) += initial_pins.c gpiocmds.c stepper.c endstop.c \
    trsync.c
src-$(CONFIG_HAVE_GPIO_ADC) += adccmds.c
src-$(CONFIG_HAVE_GPIO_SPI) += spicmds.c
src-$(CONFIG_HAVE_GPIO_SDIO) += sdiocmds.c
src-$(CONFIG_HAVE_GPIO_I2C) += i2ccmds.c
src-$(CONFIG_HAVE_GPIO_HARD_PWM) += pwmcmds.c

src-$(CONFIG_WANT_GPIO_BITBANGING) += buttons.c tmcuart.c neopixel.c \
    pulse_counter.c
src-$(CONFIG_WANT_DISPLAYS) += lcd_st7920.c lcd_hd44780.c
src-$(CONFIG_WANT_SOFTWARE_SPI) += spi_software.c
src-$(CONFIG_WANT_SOFTWARE_I2C) += i2c_software.c
sensors-src-$(CONFIG_HAVE_GPIO_SPI) := thermocouple.c sensor_adxl345.c \
    sensor_angle.c
sensors-src-$(CONFIG_HAVE_GPIO_I2C) += sensor_mpu9250.c
src-$(CONFIG_WANT_SENSORS) += $(sensors-src-y)
src-$(CONFIG_WANT_LIS2DW) += sensor_lis2dw.c
src-$(CONFIG_WANT_LDC1612) += sensor_ldc1612.c
src-$(CONFIG_WANT_HX71X) += sensor_hx71x.c
src-$(CONFIG_WANT_CS123X) += sensor_cs123x.c
src-$(CONFIG_WANT_ADS1220) += sensor_ads1220.c
src-$(CONFIG_NEED_SENSOR_BULK) += sensor_bulk.c
src-$(CONFIG_WANT_LOAD_CELL_ENDSTOP) += load_cell_endstop.c sos_filter.c
else
src-y += sched.c bootloader.c bootloader_com.c
endif
# SEGGER_RTT
dirs-y += src/RTT
src-y += RTT/SEGGER_RTT_printf.c RTT/SEGGER_RTT.c