/* SH1106 128x64 OLED per I2C (0x3C) - einfacher Textpuffer 21x8 Zeichen */
#include <string.h>
#include "board.h"
#include "app.h"
#include "font5x7.h"

#define W 128
#define PAGES 8
#define COL_OFFSET 2          /* SH1106 hat 132 Spalten RAM, Panel beginnt bei 2 */

static uint8_t fb[PAGES][W];

static bool cmd(uint8_t c)
{
    uint8_t b[2] = {0x00, c};
    return i2c_write(I2C_ADDR_SH1106, b, 2);
}

bool sh1106_init(void)
{
    static const uint8_t seq[] = {
        0xAE,             /* Display aus */
        0xD5, 0x80,       /* Taktteiler */
        0xA8, 0x3F,       /* Multiplex 64 */
        0xD3, 0x00,       /* Offset */
        0x40,             /* Startzeile 0 */
        0xAD, 0x8B,       /* DC-DC an */
        0xA1,             /* Segment-Remap */
        0xC8,             /* COM-Scan absteigend */
        0xDA, 0x12,
        0x81, 0x80,       /* Kontrast */
        0xD9, 0x22,
        0xDB, 0x35,
        0xA4, 0xA6,
    };
    for (unsigned i = 0; i < sizeof seq; i++) {
        if (!cmd(seq[i])) {
            return false;
        }
    }
    sh1106_clear();
    sh1106_flush();
    return cmd(0xAF);
}

void sh1106_power(bool on)
{
    cmd(on ? 0xAF : 0xAE);
}

void sh1106_clear(void)
{
    memset(fb, 0, sizeof fb);
}

void sh1106_text(uint8_t col, uint8_t row, const char *s)
{
    if (row >= PAGES) {
        return;
    }
    unsigned x = (unsigned)col * 6U;
    while (*s && x + 5 <= W) {
        char ch = *s++;
        if (ch < 0x20 || ch > 0x7E) {
            ch = '?';
        }
        memcpy(&fb[row][x], font5x7[ch - 0x20], 5);
        fb[row][x + 5] = 0;
        x += 6;
    }
}

void sh1106_flush(void)
{
    uint8_t buf[1 + W];
    buf[0] = 0x40;                     /* Datenstrom */
    for (uint8_t p = 0; p < PAGES; p++) {
        if (!cmd((uint8_t)(0xB0 | p)) ||
            !cmd((uint8_t)(0x00 | (COL_OFFSET & 0x0F))) ||
            !cmd((uint8_t)(0x10 | (COL_OFFSET >> 4)))) {
            return;
        }
        memcpy(&buf[1], fb[p], W);
        if (!i2c_write(I2C_ADDR_SH1106, buf, sizeof buf)) {
            return;
        }
    }
}
