/* BME280 (0x76) und Si7006-A20 (0x40) auf I2C1 */
#include "board.h"
#include "app.h"

static sensor_data_t s_data;

/* ---------------- BME280 ---------------- */
static struct {
    uint16_t T1; int16_t T2, T3;
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
    uint8_t H1; int16_t H2; uint8_t H3; int16_t H4, H5; int8_t H6;
} cal;
static bool s_bme_init;

static bool bme_init(void)
{
    uint8_t id = 0, c[26], h[7];
    if (!i2c_reg_read(I2C_ADDR_BME280, 0xD0, &id, 1) || id != 0x60) {
        return false;
    }
    if (!i2c_reg_read(I2C_ADDR_BME280, 0x88, c, 26) ||
        !i2c_reg_read(I2C_ADDR_BME280, 0xE1, h, 7)) {
        return false;
    }
    cal.T1 = (uint16_t)(c[0] | c[1] << 8);
    cal.T2 = (int16_t)(c[2] | c[3] << 8);
    cal.T3 = (int16_t)(c[4] | c[5] << 8);
    cal.P1 = (uint16_t)(c[6] | c[7] << 8);
    cal.P2 = (int16_t)(c[8] | c[9] << 8);
    cal.P3 = (int16_t)(c[10] | c[11] << 8);
    cal.P4 = (int16_t)(c[12] | c[13] << 8);
    cal.P5 = (int16_t)(c[14] | c[15] << 8);
    cal.P6 = (int16_t)(c[16] | c[17] << 8);
    cal.P7 = (int16_t)(c[18] | c[19] << 8);
    cal.P8 = (int16_t)(c[20] | c[21] << 8);
    cal.P9 = (int16_t)(c[22] | c[23] << 8);
    cal.H1 = c[25];
    cal.H2 = (int16_t)(h[0] | h[1] << 8);
    cal.H3 = h[2];
    cal.H4 = (int16_t)(((int16_t)(int8_t)h[3] << 4) | (h[4] & 0x0F));
    cal.H5 = (int16_t)(((int16_t)(int8_t)h[5] << 4) | (h[4] >> 4));
    cal.H6 = (int8_t)h[6];

    /* Feuchte x1, dann Standby 1000 ms / Filter aus, dann T x1, P x1, Normal-Mode */
    return i2c_reg_write(I2C_ADDR_BME280, 0xF2, 0x01) &&
           i2c_reg_write(I2C_ADDR_BME280, 0xF5, 0xA0) &&
           i2c_reg_write(I2C_ADDR_BME280, 0xF4, 0x27);
}

/* Kompensation nach Bosch-Datenblatt (Integer-Varianten) */
static bool bme_read(void)
{
    uint8_t r[8];
    if (!i2c_reg_read(I2C_ADDR_BME280, 0xF7, r, 8)) {
        return false;
    }
    int32_t adc_P = (int32_t)((uint32_t)r[0] << 12 | (uint32_t)r[1] << 4 | r[2] >> 4);
    int32_t adc_T = (int32_t)((uint32_t)r[3] << 12 | (uint32_t)r[4] << 4 | r[5] >> 4);
    int32_t adc_H = (int32_t)((uint32_t)r[6] << 8 | r[7]);
    if (adc_T == 0x80000) {
        return false;                   /* noch keine Messung */
    }

    int32_t v1 = ((((adc_T >> 3) - ((int32_t)cal.T1 << 1))) * cal.T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - (int32_t)cal.T1) * ((adc_T >> 4) - (int32_t)cal.T1)) >> 12) * cal.T3) >> 14;
    int32_t t_fine = v1 + v2;
    s_data.bme_t_c100 = (int16_t)((t_fine * 5 + 128) >> 8);

    int64_t p1 = (int64_t)t_fine - 128000;
    int64_t p2 = p1 * p1 * cal.P6;
    p2 += (p1 * cal.P5) * 131072;
    p2 += (int64_t)cal.P4 * 34359738368LL;
    p1 = ((p1 * p1 * cal.P3) / 256) + (p1 * cal.P2 * 4096);
    p1 = ((140737488355328LL + p1) * cal.P1) / 8589934592LL;
    if (p1 != 0) {
        int64_t p = 1048576 - adc_P;
        p = ((p * 2147483648LL - p2) * 3125) / p1;
        int64_t a = ((int64_t)cal.P9 * (p / 8192) * (p / 8192)) / 33554432;
        int64_t b = ((int64_t)cal.P8 * p) / 524288;
        p = ((p + a + b) / 256) + ((int64_t)cal.P7 * 16);
        s_data.bme_p_pa = (uint32_t)(p / 256);
    }

    int32_t x = t_fine - 76800;
    x = (((((adc_H << 14) - ((int32_t)cal.H4 << 20) - ((int32_t)cal.H5 * x)) + 16384) >> 15) *
         (((((((x * cal.H6) >> 10) * (((x * (int32_t)cal.H3) >> 11) + 32768)) >> 10) + 2097152) *
           cal.H2 + 8192) >> 14));
    x = x - (((((x >> 15) * (x >> 15)) >> 7) * (int32_t)cal.H1) >> 4);
    if (x < 0) x = 0;
    if (x > 419430400) x = 419430400;
    uint32_t h_q10 = (uint32_t)x >> 12;           /* %rF * 1024 */
    s_data.bme_rh_p100 = (uint16_t)((h_q10 * 100U) >> 10);
    return true;
}

/* ---------------- Si7006 ---------------- */
static bool s_si_pending;

static bool si_start(void)
{
    uint8_t cmd = 0xF5;                 /* RH messen, No-Hold-Master */
    return i2c_write(I2C_ADDR_SI7006, &cmd, 1);
}

static bool si_read(void)
{
    uint8_t b[2];
    if (!i2c_read(I2C_ADDR_SI7006, b, 2)) {
        return false;                   /* NACK = Messung noch nicht fertig */
    }
    uint32_t rh_code = (uint32_t)(b[0] << 8 | b[1]);
    int32_t rh = (int32_t)((12500U * rh_code) >> 16) - 600;  /* 0,01 % */
    if (rh < 0) rh = 0;
    if (rh > 10000) rh = 10000;
    s_data.si_rh_p100 = (uint16_t)rh;

    uint8_t cmd = 0xE0;                 /* Temperatur aus der RH-Messung */
    if (!i2c_write(I2C_ADDR_SI7006, &cmd, 1) || !i2c_read(I2C_ADDR_SI7006, b, 2)) {
        return false;
    }
    uint32_t t_code = (uint32_t)(b[0] << 8 | b[1]);
    s_data.si_t_c100 = (int16_t)((int32_t)((17572U * t_code) >> 16) - 4685);
    return true;
}

/* ---------------- oeffentlich ---------------- */
void sensors_init(void)
{
    s_bme_init = bme_init();
    s_data.bme_present = s_bme_init;    /* beim Start nicht gefunden = nicht bestueckt */
    s_data.bme_ok = false;
    s_si_pending = si_start();
}

void sensors_task(void)
{
    if (s_data.bme_present) {
        if (!s_bme_init) {
            s_bme_init = bme_init();
        }
        s_data.bme_ok = s_bme_init && bme_read();
        if (!s_data.bme_ok) {
            s_bme_init = false;         /* naechstes Mal neu initialisieren */
        }
    }

    /* Si7006: Ergebnis der im letzten Zyklus gestarteten Messung abholen,
       danach neue Messung starten (Messdauer ~ 20 ms << SENSOR_PERIOD_MS) */
    s_data.si_ok = s_si_pending && si_read();
    s_si_pending = si_start();
}

const sensor_data_t *sensors_get(void)
{
    return &s_data;
}
