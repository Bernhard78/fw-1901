/* Relais K1 auf der Kontaktebene: PA0 (DO1) -> Ext6 -> NDS355AN -> HF41F
   Geschaltet wird nur noch vom Zweipunktregler (thermostat.c). */
#include "board.h"
#include "app.h"

static bool s_on;

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

void relay_set(bool on)
{
    s_on = on;
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool relay_get(void)
{
    return s_on;
}
