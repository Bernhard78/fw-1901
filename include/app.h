/* Modulschnittstellen */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Konfiguration ---- */
#define CAN_BITRATE_KBIT        250
#define SENSOR_PERIOD_MS        1000
#define HEARTBEAT_PERIOD_MS     1000
#define RELAY_FAILSAFE_MS       10000   /* nur aktiv, wenn DIP-Bit 7 EIN */

/* ---- relay.c ---- */
typedef enum { RELAY_SRC_LOCAL, RELAY_SRC_CAN } relay_src_t;
void relay_init(void);
void relay_set(bool on, relay_src_t src);
bool relay_get(void);
void relay_task(uint32_t now_ms, bool failsafe_enabled);

/* ---- inputs.c ---- */
void     inputs_init(void);
uint8_t  inputs_read_dip(void);     /* Bit n = DI_ADRn, 1 = Schalter EIN */
void     inputs_task_10ms(void);
uint8_t  inputs_take_presses(void); /* Bit0..2 = Taster 1..3 seit letztem Aufruf gedrueckt */

/* ---- sensors.c ---- */
typedef struct {
    bool     bme_ok;
    int16_t  bme_t_c100;    /* 0,01 degC */
    uint16_t bme_rh_p100;   /* 0,01 %rF */
    uint32_t bme_p_pa;      /* Pa */
    bool     si_ok;
    int16_t  si_t_c100;
    uint16_t si_rh_p100;
} sensor_data_t;

void sensors_init(void);
void sensors_task(void);            /* im SENSOR_PERIOD_MS-Takt aufrufen */
const sensor_data_t *sensors_get(void);

/* ---- sh1106.c ---- */
bool sh1106_init(void);
void sh1106_clear(void);
void sh1106_text(uint8_t col, uint8_t row, const char *s); /* row 0..7, col in Zeichen 0..20 */
void sh1106_flush(void);
void sh1106_power(bool on);

/* ---- can_proto.c ---- */
typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t tx_errors;
    bool     bus_off;
} can_stats_t;

bool can_proto_init(uint8_t node_id);
void can_proto_poll(uint32_t now_ms);
void can_proto_send_sensors(const sensor_data_t *d);
void can_proto_send_heartbeat(void);
const can_stats_t *can_proto_stats(void);

/* ---- i2c_bus.c ---- */
void i2c_bus_init(void);
bool i2c_write(uint8_t addr, const uint8_t *data, uint16_t len);
bool i2c_read(uint8_t addr, uint8_t *data, uint16_t len);
bool i2c_reg_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);
bool i2c_reg_write(uint8_t addr, uint8_t reg, uint8_t val);

#endif
