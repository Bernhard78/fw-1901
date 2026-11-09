/* Relais K1 auf der Kontaktebene: PA0 (DO1) -> Ext6 -> NDS355AN -> HF41F */
#include "board.h"
#include "app.h"

static bool s_on;
static relay_src_t s_src;
static uint32_t s_last_can_ms;
static bool s_can_seen;

void relay_init(void)
{
    /* Wird in main() vor allem anderen aufgerufen: Pin sofort aktiv Low,
       da auf der Hardware kein Gate-Pulldown vorhanden ist. */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
    GPIO_InitTypeDef g = {0};
    g.Pin = RELAY_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_PORT, &g);
    s_on = false;
}

void relay_set(bool on, relay_src_t src)
{
    s_on = on;
    s_src = src;
    if (src == RELAY_SRC_CAN) {
        s_last_can_ms = HAL_GetTick();
        s_can_seen = true;
    }
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool relay_get(void)
{
    return s_on;
}

/* Fail-safe: per CAN eingeschaltetes Relais faellt ab, wenn laenger als
   RELAY_FAILSAFE_MS kein Befehl kam. Lokal geschaltet wird nicht ueberwacht. */
void relay_task(uint32_t now_ms, bool failsafe_enabled)
{
    if (failsafe_enabled && s_on && s_src == RELAY_SRC_CAN && s_can_seen &&
        (now_ms - s_last_can_ms) > RELAY_FAILSAFE_MS) {
        relay_set(false, RELAY_SRC_LOCAL);
    }
}
