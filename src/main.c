/* Firmware Baugruppe 1901 - DPL-02 (STM32F103C8T6)
 * Relais per CAN, Sensorwerte per CAN, OLED-Anzeige mit 3 Tastern. */
#include <stdio.h>
#include "board.h"
#include "app.h"

static IWDG_HandleTypeDef hiwdg;

static void clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.LSIState = RCC_LSI_ON;            /* fuer IWDG */
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;        /* 8 MHz x 9 = 72 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* HSE startet nicht: mit HSI (64 MHz) weiterlaufen, CAN-Teiler passt dann nicht */
        osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
        osc.HSEState = RCC_HSE_OFF;
        osc.HSIState = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
        osc.PLL.PLLMUL = RCC_PLL_MUL16;
        HAL_RCC_OscConfig(&osc);
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;   /* 36 MHz (CAN, I2C) */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

static void watchdog_init(void)
{
    __HAL_DBGMCU_FREEZE_IWDG();           /* steht beim Debuggen still */
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_32;   /* ~40 kHz / 32 = 1,25 kHz */
    hiwdg.Init.Reload = 2500;                   /* ~2 s */
    HAL_IWDG_Init(&hiwdg);
}

/* ---------- Anzeige ---------- */
enum { PAGE_SENSORS, PAGE_CAN, PAGE_RELAY, PAGE_COUNT };
typedef enum { UI_NORMAL, UI_EDIT_SETPOINT, UI_EDIT_HYST } ui_mode_t;

static void fmt_c100(char *out, size_t n, const char *label, int32_t v, const char *unit)
{
    const char *sign = v < 0 ? "-" : "";
    if (v < 0) v = -v;
    snprintf(out, n, "%s%s%ld.%02ld%s", label, sign, (long)(v / 100), (long)(v % 100), unit);
}

/* eine Nachkommastelle (Soll/Hysterese/Ist im Regler) */
static void fmt_c10(char *out, size_t n, const char *label, int32_t v, const char *unit)
{
    const char *sign = v < 0 ? "-" : "";
    if (v < 0) v = -v;
    v = (v + 5) / 10;
    snprintf(out, n, "%s%s%ld.%ld%s", label, sign, (long)(v / 10), (long)(v % 10), unit);
}

static void draw_actual(char *l, size_t n, uint8_t row)
{
    switch (thermostat_source()) {
    case TEMP_SRC_SI:  fmt_c10(l, n, "Ist  ", thermostat_actual(), " C (Si)");  break;
    case TEMP_SRC_BME: fmt_c10(l, n, "Ist  ", thermostat_actual(), " C (BME)"); break;
    default:           snprintf(l, n, "Ist  -- Sensorfehler"); break;
    }
    sh1106_text(0, row, l);
}

static void draw_edit(ui_mode_t mode, int16_t value)
{
    char l[24];
    sh1106_clear();
    sh1106_text(0, 0, mode == UI_EDIT_SETPOINT ? "Sollwert einstellen" : "Hysterese einstellen");
    fmt_c10(l, sizeof l, "", value, " C");
    sh1106_text(7, 2, l);
    if (mode == UI_EDIT_SETPOINT) {
        sh1106_text(0, 3, "     (15.0 - 25.0)");
    } else {
        sh1106_text(0, 3, "      (0.2 - 5.0)");
    }
    draw_actual(l, sizeof l, 5);
    sh1106_text(0, 7, "T1 -   T3 +   T2 OK");
    sh1106_flush();
}

static void draw(uint8_t page, uint8_t node, bool can_ok)
{
    char l[24];
    const sensor_data_t *d = sensors_get();
    const can_stats_t *c = can_proto_stats();

    sh1106_clear();
    switch (page) {
    case PAGE_SENSORS:
        sh1106_text(0, 0, "Messwerte");
        if (d->bme_ok) {
            fmt_c100(l, sizeof l, "BME T  ", d->bme_t_c100, " C");   sh1106_text(0, 2, l);
            fmt_c100(l, sizeof l, "BME rF ", d->bme_rh_p100, " %");  sh1106_text(0, 3, l);
            snprintf(l, sizeof l, "BME p  %lu.%lu hPa",
                     (unsigned long)(d->bme_p_pa / 100), (unsigned long)(d->bme_p_pa % 100 / 10));
            sh1106_text(0, 4, l);
        } else {
            sh1106_text(0, 2, d->bme_present ? "BME280 Fehler" : "BME280 -");
        }
        if (d->si_ok) {
            fmt_c100(l, sizeof l, "Si  T  ", d->si_t_c100, " C");    sh1106_text(0, 6, l);
            fmt_c100(l, sizeof l, "Si  rF ", d->si_rh_p100, " %");   sh1106_text(0, 7, l);
        } else {
            sh1106_text(0, 6, "Si7006 Fehler");
        }
        break;

    case PAGE_CAN:
        sh1106_text(0, 0, "CAN");
        if (node == 0) {
            sh1106_text(0, 2, "Adresse? DIP 1-7");
        } else {
            snprintf(l, sizeof l, "Knoten %u  %u kbit", node, CAN_BITRATE_KBIT); sh1106_text(0, 2, l);
        }
        snprintf(l, sizeof l, "RX %lu", (unsigned long)c->rx_count);  sh1106_text(0, 4, l);
        snprintf(l, sizeof l, "TX %lu", (unsigned long)c->tx_count);  sh1106_text(0, 5, l);
        snprintf(l, sizeof l, "Fehler %lu", (unsigned long)c->tx_errors); sh1106_text(0, 6, l);
        sh1106_text(0, 7, !can_ok ? "CAN aus" : (c->bus_off ? "BUS-OFF" : "Bus ok"));
        break;

    case PAGE_RELAY:
        sh1106_text(0, 0, "Regler (Heizen)");
        draw_actual(l, sizeof l, 2);
        fmt_c10(l, sizeof l, "Soll ", thermostat_setpoint(), " C"); sh1106_text(0, 3, l);
        fmt_c10(l, sizeof l, "Hyst ", thermostat_hyst(), " C");     sh1106_text(0, 4, l);
        snprintf(l, sizeof l, "Relais %s", relay_get() ? "EIN" : "AUS"); sh1106_text(0, 5, l);
        sh1106_text(0, 7, "T2 Soll / lang Hyst");
        break;
    default:
        break;
    }
    sh1106_flush();
}

int main(void)
{
    HAL_Init();

    /* Relais-Ausgang sofort definiert Low (kein Gate-Pulldown auf der Kontaktebene) */
    relay_init();

    /* JTAG aus, SWD an: gibt PA15 (DI_ADR7) und PB4 (DI1) frei */
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    clock_init();
    inputs_init();
    HAL_Delay(5);                          /* Pullups einschwingen lassen */

    const uint8_t dip = inputs_read_dip();
    const uint8_t node = dip & 0x7F;       /* Bit 0..6 = Knoten-ID 1..127, Bit 7 frei */

    i2c_bus_init();
    HAL_Delay(100);                        /* OLED/Sensoren nach Power-On */
    bool disp_ok = sh1106_init();
    sensors_init();
    thermostat_init();

    bool can_ok = (node != 0) && can_proto_init(node);

    watchdog_init();

    uint8_t page = PAGE_SENSORS;
    bool disp_on = true;
    ui_mode_t ui = UI_NORMAL;
    int16_t edit_val = 0;
    uint32_t t_edit = 0;
    uint32_t t10 = 0, t_sens = 0, t_hb = 0, t_disp = 0;

    for (;;) {
        uint32_t now = HAL_GetTick();
        HAL_IWDG_Refresh(&hiwdg);

        if (can_ok) {
            can_proto_poll(now);
        }

        if (now - t10 >= 10) {
            t10 = now;
            inputs_task_10ms();

            uint8_t pr = inputs_take_presses();
            uint8_t lp = inputs_take_long_presses();

            if (ui == UI_NORMAL) {
                if ((pr & 2) || (lp & 2)) {
                    ui = (lp & 2) ? UI_EDIT_HYST : UI_EDIT_SETPOINT;
                    edit_val = (ui == UI_EDIT_HYST) ? thermostat_hyst() : thermostat_setpoint();
                    t_edit = now;
                    t_disp = 0;
                    if (!disp_on) { disp_on = true; sh1106_power(true); }
                } else {
                    if (pr & 1) { page = (uint8_t)((page + 1) % PAGE_COUNT); t_disp = 0; }
                    if (pr & 4) { disp_on = !disp_on; sh1106_power(disp_on); }
                }
            } else {
                const bool sp = (ui == UI_EDIT_SETPOINT);
                const int16_t step = sp ? SETPOINT_STEP_C100 : HYST_STEP_C100;
                if (pr & 1) edit_val = (int16_t)(edit_val - step);
                if (pr & 4) edit_val = (int16_t)(edit_val + step);
                if (pr & 5) {
                    edit_val = sp ? thermostat_clamp_setpoint(edit_val) : thermostat_clamp_hyst(edit_val);
                }
                if (pr) { t_edit = now; t_disp = 0; }

                if (pr & 2) {                       /* uebernehmen und speichern */
                    if (sp) thermostat_set_setpoint(edit_val);
                    else    thermostat_set_hyst(edit_val);
                    ui = UI_NORMAL;
                    page = PAGE_RELAY;
                } else if (now - t_edit >= EDIT_TIMEOUT_MS) {
                    ui = UI_NORMAL;                 /* verwerfen */
                    t_disp = 0;
                }
            }
        }

        if (now - t_sens >= SENSOR_PERIOD_MS) {
            t_sens = now;
            sensors_task();
            thermostat_update(sensors_get());
            if (can_ok) {
                can_proto_send_sensors(sensors_get());
            }
        }

        if (can_ok && now - t_hb >= HEARTBEAT_PERIOD_MS) {
            t_hb = now;
            can_proto_send_heartbeat();
        }

        if (disp_on && (t_disp == 0 || now - t_disp >= 250)) {
            t_disp = now ? now : 1;
            if (!disp_ok) {
                disp_ok = sh1106_init();
            }
            if (disp_ok) {
                if (ui == UI_NORMAL) {
                    draw(page, node, can_ok);
                } else {
                    draw_edit(ui, edit_val);
                }
            }
        }
    }
}
