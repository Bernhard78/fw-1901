/* Modulschnittstellen */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Konfiguration ---- */
#define CAN_BITRATE_KBIT        250
#define SENSOR_PERIOD_MS        10000
#define HEARTBEAT_PERIOD_MS     1000
#define LONG_PRESS_MS           3000    /* T2 lang: Hysterese-Menue */
#define EDIT_TIMEOUT_MS         10000   /* Edit ohne Taste -> verwerfen */

/* Zweipunktregler (Heizen), alle Werte in 0,01 degC */
#define SETPOINT_MIN_C100       1500
#define SETPOINT_MAX_C100       2500
#define SETPOINT_STEP_C100      50
#define SETPOINT_DEFAULT_C100   2000
#define HYST_MIN_C100           20
#define HYST_MAX_C100           500
#define HYST_STEP_C100          10
#define HYST_DEFAULT_C100       100

/* ---- relay.c ---- */
void relay_init(void);
void relay_set(bool on);
bool relay_get(void);

/* ---- inputs.c ---- */
void     inputs_init(void);
uint8_t  inputs_read_dip(void);     /* Bit n = DI_ADRn, 1 = Schalter EIN */
void     inputs_task_10ms(void);
uint8_t  inputs_take_presses(void); /* Bit0..2 = Taster 1..3 seit letztem Aufruf gedrueckt
                                       (T2: kurz, beim Loslassen) */
uint8_t  inputs_take_long_presses(void); /* Bit1 = T2 laenger als LONG_PRESS_MS gehalten */

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

/* ---- thermostat.c ---- */
typedef enum { TEMP_SRC_NONE, TEMP_SRC_SI, TEMP_SRC_BME } temp_src_t;
void        thermostat_init(void);                     /* Werte aus Flash laden */
void        thermostat_update(const sensor_data_t *d); /* nach jeder Messung */
bool        thermostat_set_setpoint(int16_t c100);     /* false = ausserhalb Bereich */
bool        thermostat_set_hyst(int16_t c100);
int16_t     thermostat_setpoint(void);
int16_t     thermostat_hyst(void);
temp_src_t  thermostat_source(void);
int16_t     thermostat_actual(void);                   /* nur gueltig, wenn Quelle != NONE */
int16_t     thermostat_clamp_setpoint(int32_t c100);   /* Bereich + Raster (fuer Edit) */
int16_t     thermostat_clamp_hyst(int32_t c100);

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
