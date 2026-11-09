/* I2C1 mit Bus-Recovery (STM32F1-Errata: BUSY-Flag kann haengen bleiben) */
#include "board.h"
#include "app.h"

I2C_HandleTypeDef hi2c1;

#define I2C_TIMEOUT_MS 20

static void delay_us(uint32_t us)
{
    uint32_t n = us * (SystemCoreClock / 1000000U) / 4U;
    while (n--) { __NOP(); }
}

/* 9 Takte auf SCL, dann STOP - gibt einen haengenden Slave frei */
static void i2c_bus_recover(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_OUTPUT_OD;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_WritePin(GPIOB, I2C_SCL_PIN | I2C_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &g);

    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, I2C_SCL_PIN, GPIO_PIN_RESET);
        delay_us(10);
        HAL_GPIO_WritePin(GPIOB, I2C_SCL_PIN, GPIO_PIN_SET);
        delay_us(10);
    }
    /* STOP: SDA low -> high waehrend SCL high */
    HAL_GPIO_WritePin(GPIOB, I2C_SDA_PIN, GPIO_PIN_RESET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOB, I2C_SDA_PIN, GPIO_PIN_SET);
    delay_us(10);
}

static void i2c_periph_init(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = I2C_SCL_PIN | I2C_SDA_PIN;
    g.Mode = GPIO_MODE_AF_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &g);

    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_I2C1_FORCE_RESET();
    __HAL_RCC_I2C1_RELEASE_RESET();

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;   /* 100 kHz: sicher mit Modul-Pullups + Stapelverbinder */
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

void i2c_bus_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    i2c_bus_recover();
    i2c_periph_init();
}

static bool check(HAL_StatusTypeDef st)
{
    if (st == HAL_OK) {
        return true;
    }
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY) || st == HAL_TIMEOUT) {
        HAL_I2C_DeInit(&hi2c1);
        i2c_bus_init();
    }
    return false;
}

bool i2c_write(uint8_t addr, const uint8_t *data, uint16_t len)
{
    return check(HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(addr << 1), (uint8_t *)data, len, I2C_TIMEOUT_MS));
}

bool i2c_read(uint8_t addr, uint8_t *data, uint16_t len)
{
    return check(HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(addr << 1), data, len, I2C_TIMEOUT_MS));
}

bool i2c_reg_read(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
    return check(HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), reg, I2C_MEMADD_SIZE_8BIT, data, len, I2C_TIMEOUT_MS));
}

bool i2c_reg_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    return check(HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr << 1), reg, I2C_MEMADD_SIZE_8BIT, &val, 1, I2C_TIMEOUT_MS));
}
