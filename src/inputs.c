/* DIP-Schalter (Adresse) und Taster DI1..DI3 */
#include "board.h"
#include "app.h"

static uint8_t s_stable;     /* Bit = 1: Taster gedrueckt (entprellt) */
static uint8_t s_last_raw;
static uint8_t s_presses;
static uint8_t s_long_presses;
static uint16_t s_t2_held;   /* T2 gedrueckt seit n x 10 ms */
static bool s_t2_long_done;  /* Langdruck schon gemeldet -> kein Kurzdruck beim Loslassen */

#define T2_MASK 2U

void inputs_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;              /* DIP hat keine externen Pullups */
    g.Pin = ADR_PORT_A_PINS;
    HAL_GPIO_Init(GPIOA, &g);

    g.Pin = DI_UNUSED_PINS;            /* DI4..DI6 offen auf der Display-Ebene */
    HAL_GPIO_Init(GPIOB, &g);

    g.Pull = GPIO_NOPULL;              /* 10k extern */
    g.Pin = BTN1_PIN | BTN2_PIN | BTN3_PIN;
    HAL_GPIO_Init(GPIOB, &g);

    /* ungenutzte DO2..DO6 definiert Low */
    HAL_GPIO_WritePin(GPIOA, DO_UNUSED_PINS, GPIO_PIN_RESET);
    g.Pin = DO_UNUSED_PINS;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}

uint8_t inputs_read_dip(void)
{
    static const uint16_t pins[8] = {
        GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9,
        GPIO_PIN_10, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_15
    };
    uint32_t idr = GPIOA->IDR;
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) {
        if ((idr & pins[i]) == 0) {    /* EIN = nach GND */
            v |= (uint8_t)(1U << i);
        }
    }
    return v;
}

static uint8_t read_buttons_raw(void)
{
    uint8_t r = 0;
    if (HAL_GPIO_ReadPin(BTN1_PORT, BTN1_PIN) == GPIO_PIN_RESET) r |= 1;
    if (HAL_GPIO_ReadPin(BTN2_PORT, BTN2_PIN) == GPIO_PIN_RESET) r |= 2;
    if (HAL_GPIO_ReadPin(BTN3_PORT, BTN3_PIN) == GPIO_PIN_RESET) r |= 4;
    return r;
}

/* alle 10 ms: Zustand gilt, wenn zwei Abtastungen (20 ms) uebereinstimmen.
   T1/T3 melden beim Druecken. T2 meldet kurz beim Loslassen oder einmal
   lang nach LONG_PRESS_MS gehalten. */
void inputs_task_10ms(void)
{
    uint8_t raw = read_buttons_raw();
    if (raw == s_last_raw) {
        uint8_t newly = (uint8_t)(raw & ~s_stable);
        uint8_t released = (uint8_t)(s_stable & ~raw);
        s_presses |= (uint8_t)(newly & ~T2_MASK);

        if (released & T2_MASK) {
            if (!s_t2_long_done) {
                s_presses |= T2_MASK;
            }
            s_t2_held = 0;
            s_t2_long_done = false;
        }
        s_stable = raw;
    }
    s_last_raw = raw;

    if ((s_stable & T2_MASK) && !s_t2_long_done) {
        if (++s_t2_held >= LONG_PRESS_MS / 10) {
            s_long_presses |= T2_MASK;
            s_t2_long_done = true;
        }
    }
}

uint8_t inputs_take_presses(void)
{
    uint8_t p = s_presses;
    s_presses = 0;
    return p;
}

uint8_t inputs_take_long_presses(void)
{
    uint8_t p = s_long_presses;
    s_long_presses = 0;
    return p;
}
