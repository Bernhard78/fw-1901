/* Zweipunktregler (Heizen) mit Hysterese fuer Relais K1.
 * Istwert: Si7006, Ersatz BME280; ohne gueltigen Istwert ist das Relais AUS.
 * Relais EIN bei T < Soll - H/2, AUS bei T > Soll + H/2.
 * Sollwert und Hysterese liegen in der letzten Flash-Seite. */
#include "board.h"
#include "app.h"

/* Letzte 1-KB-Seite des 64-KB-Flash; platformio.ini begrenzt das Programm davor */
#define CFG_FLASH_ADDR  0x0800FC00UL
#define CFG_MAGIC       0x1901U

typedef struct {
    uint16_t magic;
    int16_t  setpoint;
    int16_t  hyst;
    uint16_t check;     /* ~(magic ^ setpoint ^ hyst) */
} cfg_t;

static int16_t s_setpoint = SETPOINT_DEFAULT_C100;
static int16_t s_hyst = HYST_DEFAULT_C100;
static temp_src_t s_src = TEMP_SRC_NONE;
static int16_t s_actual;

static uint16_t cfg_check(const cfg_t *c)
{
    return (uint16_t)~(c->magic ^ (uint16_t)c->setpoint ^ (uint16_t)c->hyst);
}

static int16_t snap(int32_t v, int32_t lo, int32_t hi, int32_t step)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    v = lo + ((v - lo + step / 2) / step) * step;   /* auf Raster runden */
    return (int16_t)(v > hi ? hi : v);
}

int16_t thermostat_clamp_setpoint(int32_t c100)
{
    return snap(c100, SETPOINT_MIN_C100, SETPOINT_MAX_C100, SETPOINT_STEP_C100);
}

int16_t thermostat_clamp_hyst(int32_t c100)
{
    return snap(c100, HYST_MIN_C100, HYST_MAX_C100, HYST_STEP_C100);
}

static void cfg_save(void)
{
    const cfg_t *old = (const cfg_t *)CFG_FLASH_ADDR;
    if (old->magic == CFG_MAGIC && old->check == cfg_check(old) &&
        old->setpoint == s_setpoint && old->hyst == s_hyst) {
        return;                                     /* unveraendert: Flash schonen */
    }

    cfg_t c = { CFG_MAGIC, s_setpoint, s_hyst, 0 };
    c.check = cfg_check(&c);

    FLASH_EraseInitTypeDef e = {0};
    uint32_t page_err = 0;
    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.PageAddress = CFG_FLASH_ADDR;
    e.NbPages = 1;

    HAL_FLASH_Unlock();                             /* Loeschen dauert ~20 ms, IWDG = 2 s */
    if (HAL_FLASHEx_Erase(&e, &page_err) == HAL_OK) {
        const uint16_t *p = (const uint16_t *)&c;
        for (uint32_t i = 0; i < sizeof c / 2; i++) {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, CFG_FLASH_ADDR + 2 * i, p[i]) != HAL_OK) {
                break;
            }
        }
    }
    HAL_FLASH_Lock();
}

void thermostat_init(void)
{
    const cfg_t *c = (const cfg_t *)CFG_FLASH_ADDR;
    if (c->magic == CFG_MAGIC && c->check == cfg_check(c)) {
        s_setpoint = thermostat_clamp_setpoint(c->setpoint);
        s_hyst = thermostat_clamp_hyst(c->hyst);
    }
    relay_set(false);                               /* bis zur ersten Messung AUS */
}

void thermostat_update(const sensor_data_t *d)
{
    if (d->si_ok) {
        s_src = TEMP_SRC_SI;
        s_actual = d->si_t_c100;
    } else if (d->bme_ok) {
        s_src = TEMP_SRC_BME;
        s_actual = d->bme_t_c100;
    } else {
        s_src = TEMP_SRC_NONE;
        relay_set(false);
        return;
    }

    int16_t half = (int16_t)(s_hyst / 2);
    if (s_actual < s_setpoint - half) {
        relay_set(true);
    } else if (s_actual > s_setpoint + half) {
        relay_set(false);
    }
    /* im Hystereseband: Zustand halten */
}

bool thermostat_set_setpoint(int16_t c100)
{
    if (c100 < SETPOINT_MIN_C100 || c100 > SETPOINT_MAX_C100) {
        return false;
    }
    s_setpoint = thermostat_clamp_setpoint(c100);
    cfg_save();
    thermostat_update(sensors_get());
    return true;
}

bool thermostat_set_hyst(int16_t c100)
{
    if (c100 < HYST_MIN_C100 || c100 > HYST_MAX_C100) {
        return false;
    }
    s_hyst = thermostat_clamp_hyst(c100);
    cfg_save();
    thermostat_update(sensors_get());
    return true;
}

int16_t thermostat_setpoint(void)   { return s_setpoint; }
int16_t thermostat_hyst(void)       { return s_hyst; }
temp_src_t thermostat_source(void)  { return s_src; }
int16_t thermostat_actual(void)     { return s_actual; }
