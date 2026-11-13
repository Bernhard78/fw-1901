/* Einfaches CAN-Protokoll (11-bit-IDs), siehe README.md */
#include "board.h"
#include "app.h"

CAN_HandleTypeDef hcan;

#define ID_CMD_BASE     0x200U   /* + Knoten-ID; 0x200 = Broadcast */
#define ID_BME_BASE     0x180U
#define ID_SI_BASE      0x280U
#define ID_HB_BASE      0x700U

#define CMD_REQUEST      0x10    /* Sensordaten + Heartbeat sofort senden */
#define CMD_SET_SETPOINT 0x20    /* Byte1-2: Sollwert int16 [0,01 degC] */
#define CMD_SET_HYST     0x21    /* Byte1-2: Hysterese int16 [0,01 degC] */

static uint8_t s_node;
static can_stats_t s_stats;
static uint8_t s_hb_counter;

static void can_gpio_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_CAN1_2();          /* CAN_RX = PB8, CAN_TX = PB9 */

    GPIO_InitTypeDef g = {0};
    g.Pin = CAN_RX_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &g);

    g.Pin = CAN_TX_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);
}

bool can_proto_init(uint8_t node_id)
{
    s_node = node_id;
    can_gpio_init();
    __HAL_RCC_CAN1_CLK_ENABLE();

    /* APB1 = 36 MHz; 16 Tq/Bit (1 + 13 + 2) -> Abtastpunkt 87,5 % */
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 36000U / (16U * CAN_BITRATE_KBIT);
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = ENABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = ENABLE;
    if (HAL_CAN_Init(&hcan) != HAL_OK) {
        return false;
    }

    /* Filterbank 0: zwei exakte IDs (eigene Befehls-ID + Broadcast) */
    CAN_FilterTypeDef f = {0};
    f.FilterBank = 0;
    f.FilterMode = CAN_FILTERMODE_IDLIST;
    f.FilterScale = CAN_FILTERSCALE_16BIT;
    f.FilterIdHigh = (uint32_t)(ID_CMD_BASE + s_node) << 5;
    f.FilterIdLow = (uint32_t)ID_CMD_BASE << 5;
    f.FilterMaskIdHigh = (uint32_t)(ID_CMD_BASE + s_node) << 5;
    f.FilterMaskIdLow = (uint32_t)ID_CMD_BASE << 5;
    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation = ENABLE;
    f.SlaveStartFilterBank = 14;
    if (HAL_CAN_ConfigFilter(&hcan, &f) != HAL_OK) {
        return false;
    }
    return HAL_CAN_Start(&hcan) == HAL_OK;
}

static void send(uint32_t id, const uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef h = {0};
    uint32_t mbox;
    h.StdId = id;
    h.IDE = CAN_ID_STD;
    h.RTR = CAN_RTR_DATA;
    h.DLC = len;
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0 ||
        HAL_CAN_AddTxMessage(&hcan, &h, (uint8_t *)data, &mbox) != HAL_OK) {
        s_stats.tx_errors++;
        return;
    }
    s_stats.tx_count++;
}

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;                  /* Little Endian */
    p[1] = (uint8_t)(v >> 8);
}

void can_proto_send_sensors(const sensor_data_t *d)
{
    uint8_t b[8];
    /* BME280: T [0,01 degC], RH [0,01 %], p [Pa] (32 bit) */
    put16(&b[0], (uint16_t)d->bme_t_c100);
    put16(&b[2], d->bme_rh_p100);
    put16(&b[4], (uint16_t)(d->bme_p_pa & 0xFFFF));
    put16(&b[6], (uint16_t)(d->bme_p_pa >> 16));
    if (d->bme_ok) {
        send(ID_BME_BASE + s_node, b, 8);
    }
    /* Si7006: T [0,01 degC], RH [0,01 %] */
    put16(&b[0], (uint16_t)d->si_t_c100);
    put16(&b[2], d->si_rh_p100);
    if (d->si_ok) {
        send(ID_SI_BASE + s_node, b, 4);
    }
}

void can_proto_send_heartbeat(void)
{
    const sensor_data_t *d = sensors_get();
    uint8_t b[7];
    b[0] = relay_get() ? 1 : 0;
    b[1] = (uint8_t)((d->bme_ok ? 0 : 0x01) | (d->si_ok ? 0 : 0x02) |
                     (s_stats.bus_off ? 0x04 : 0) |
                     (thermostat_source() == TEMP_SRC_NONE ? 0x08 : 0));
    b[2] = s_hb_counter++;
    put16(&b[3], (uint16_t)thermostat_setpoint());
    put16(&b[5], (uint16_t)thermostat_hyst());
    send(ID_HB_BASE + s_node, b, 7);
}

void can_proto_poll(uint32_t now_ms)
{
    (void)now_ms;
    CAN_RxHeaderTypeDef h;
    uint8_t d[8];

    s_stats.bus_off = (hcan.Instance->ESR & CAN_ESR_BOFF) != 0;

    while (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) > 0) {
        if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &h, d) != HAL_OK) {
            break;
        }
        s_stats.rx_count++;
        if (h.IDE != CAN_ID_STD || h.RTR != CAN_RTR_DATA || h.DLC < 1) {
            continue;
        }
        switch (d[0]) {
        case CMD_SET_SETPOINT:
            if (h.DLC >= 3) {
                thermostat_set_setpoint((int16_t)(d[1] | d[2] << 8));   /* ausserhalb Bereich: ignoriert */
            }
            break;
        case CMD_SET_HYST:
            if (h.DLC >= 3) {
                thermostat_set_hyst((int16_t)(d[1] | d[2] << 8));
            }
            break;
        case CMD_REQUEST:
            can_proto_send_sensors(sensors_get());
            can_proto_send_heartbeat();
            break;
        default: break;
        }
    }
}

const can_stats_t *can_proto_stats(void)
{
    return &s_stats;
}
