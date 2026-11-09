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

static void fmt_c100(char *out, size_t n, const char *label, int32_t v, const char *unit)
{
    const char *sign = v < 0 ? "-" : "";
    if (v < 0) v = -v;
    snprintf(out, n, "%s%s%ld.%02ld%s", label, sign, (long)(v / 100), (long)(v % 100), unit);
}

static void draw(uint8_t page, uint8_t node, uint8_t dip, bool can_ok)
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
            sh1106_text(0, 2, "BME280 Fehler");
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
        sh1106_text(0, 0, "Relais");
        sh1106_text(0, 2, relay_get() ? "EIN" : "AUS");
        snprintf(l, sizeof l, "Fail-safe %s", (dip & 0x80) ? "an" : "aus"); sh1106_text(0, 4, l);
        sh1106_text(0, 6, "T2 = umschalten");
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
    const uint8_t node = dip & 0x7F;       /* Bit 0..6 = Knoten-ID 1..127 */
    const bool failsafe = (dip & 0x80) != 0;

    i2c_bus_init();
    HAL_Delay(100);                        /* OLED/Sensoren nach Power-On */
    bool disp_ok = sh1106_init();
    sensors_init();

    bool can_ok = (node != 0) && can_proto_init(node);

    watchdog_init();

    uint8_t page = PAGE_SENSORS;
    bool disp_on = true;
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
            relay_task(now, failsafe);

            uint8_t pr = inputs_take_presses();
            if (pr & 1) { page = (uint8_t)((page + 1) % PAGE_COUNT); t_disp = 0; }
            if (pr & 2) { relay_set(!relay_get(), RELAY_SRC_LOCAL); t_disp = 0; }
            if (pr & 4) { disp_on = !disp_on; sh1106_power(disp_on); }
        }

        if (now - t_sens >= SENSOR_PERIOD_MS) {
            t_sens = now;
            sensors_task();
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
                draw(page, node, dip, can_ok);
            }
        }
    }
}
