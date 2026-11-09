/* Pinbelegung DPL-02 (aus Schaltplan 1901-DPL-02 Rev 2.0) */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx_hal.h"

/* Digitale Ausgaenge (DOx -> Ext-Netze zur Kontaktebene) */
#define RELAY_PORT      GPIOA       /* DO1 -> Ext6 -> Q1/K1 (Kontaktebene) */
#define RELAY_PIN       GPIO_PIN_0
#define DO_UNUSED_PINS  (GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5) /* DO2..DO6 */

/* DIP-Schalter SW1: DI_ADR0..7, Schalter EIN = GND, keine externen Pullups */
#define ADR_PORT_A_PINS (GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | \
                         GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15)

/* Taster auf der Display-Ebene (10k Pullup, Low-aktiv) */
#define BTN1_PORT GPIOB
#define BTN1_PIN  GPIO_PIN_4    /* DI1 (PB4 = NJTRST -> JTAG muss aus sein) */
#define BTN2_PORT GPIOB
#define BTN2_PIN  GPIO_PIN_5    /* DI2 */
#define BTN3_PORT GPIOB
#define BTN3_PIN  GPIO_PIN_15   /* DI3 */
#define DI_UNUSED_PINS (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14) /* DI6, DI5, DI4 */

/* I2C1: PB6 = SCL, PB7 = SDA (Standardbelegung, 4k7 Pullups auf DPL) */
#define I2C_SCL_PIN GPIO_PIN_6
#define I2C_SDA_PIN GPIO_PIN_7

/* CAN: PB8 = RX, PB9 = TX (Remap 2), SN65HVD230, CANH = LineB, CANL = LineA */
#define CAN_RX_PIN GPIO_PIN_8
#define CAN_TX_PIN GPIO_PIN_9

/* I2C-Adressen (7 bit) */
#define I2C_ADDR_SH1106  0x3C
#define I2C_ADDR_SI7006  0x40
#define I2C_ADDR_BME280  0x76   /* SDO an GND */

extern I2C_HandleTypeDef hi2c1;
extern CAN_HandleTypeDef hcan;

#endif
