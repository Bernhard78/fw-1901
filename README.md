# Firmware Baugruppe 1901 (DPL-02, STM32F103C8T6)

Relais über Zweipunktregler (Heizen) mit Hysterese, Sensorwerte (BME280, Si7006) per CAN senden,
OLED-Anzeige mit 3 Tastern.

## Bauen / Flashen
```
pio run                 # bauen
pio run -t upload       # per ST-Link (SWD-Stecker CN1)
```
`include/stm32f1xx_hal_conf.h` aktiviert nur die benötigten HAL-Module.

## Einstellungen per DIP-Schalter SW1 (beim Start gelesen)
| Schalter | Bedeutung |
|---|---|
| DI_ADR0..6 | Knoten-ID 1..127 (binär, EIN = 1). ID 0 = CAN bleibt aus, Anzeige „Adresse?“ |
| DI_ADR7 | frei (früher Fail-safe für CAN-Relaisbefehle) |

## Zweipunktregler (Heizen)
- Istwert: Si7006, bei Fehler BME280. Beide ausgefallen → Relais AUS.
- Relais EIN bei T < Soll − H/2, AUS bei T > Soll + H/2, dazwischen bleibt der Zustand.
- Ausgewertet wird bei jeder Messung (alle 10 s).
- Sollwert 15,0–25,0 °C (0,5-°C-Schritte, Standard 20,0), Hysterese 0,2–5,0 °C
  (0,1-°C-Schritte, Standard 1,0). Beide liegen im Flash (Seite 0x0800FC00) und
  überstehen einen Neustart.

## Bedienung
- T1 (DI1): nächste Anzeigeseite (Messwerte / CAN / Regler)
- T2 (DI2) kurz: Sollwert einstellen
- T2 (DI2) 3 s halten: Hysterese einstellen
- T3 (DI3): Anzeige an/aus

Im Einstellmodus: T1 = −, T3 = +, T2 = übernehmen und speichern.
10 s ohne Taste → Änderung wird verworfen.

## CAN-Protokoll (250 kbit/s, 11-bit-IDs, Little Endian)
| ID | Richtung | Inhalt |
|---|---|---|
| `0x200 + ID` | → Knoten | Byte0: `0x10` Daten sofort senden; `0x20` Sollwert setzen, `0x21` Hysterese setzen (Byte1–2 int16 [0,01 °C], außerhalb des Bereichs ignoriert, wird auf Raster gerundet) |
| `0x200` | → alle | Broadcast, gleiche Befehle |
| `0x180 + ID` | Knoten → | BME280, 8 Byte: T int16 [0,01 °C], rF uint16 [0,01 %], p uint32 [Pa] |
| `0x280 + ID` | Knoten → | Si7006, 4 Byte: T int16 [0,01 °C], rF uint16 [0,01 %] |
| `0x700 + ID` | Knoten → | Heartbeat 1 s, 7 Byte: Relais (0/1), Fehlerbits (0x01 BME, 0x02 Si, 0x04 Bus-Off, 0x08 kein Istwert), Zähler, Sollwert int16 [0,01 °C], Hysterese int16 [0,01 °C] |

Beispiel: Sollwert 20,5 °C an Knoten 5 → ID `0x205`, Daten `20 02 08` (2050 = 0x0802).

Bitrate: `CAN_BITRATE_KBIT` in `include/app.h` (125/250/500; 1 Mbit/s erst mit R9 = 0 Ω).
CANH = LineB, CANL = LineA (Klemme U1 Pin 2/3 auf der Kontaktebene).

## Pinbelegung
Siehe `include/board.h`. Wichtig: JTAG wird abgeschaltet (`__HAL_AFIO_REMAP_SWJ_NOJTAG`),
damit PA15 (DI_ADR7) und PB4 (DI1) als GPIO funktionieren. SWD bleibt aktiv.

## Hardware-Hinweise aus der Schaltplanprüfung
Die Firmware umgeht einige Punkte (Relais-Pin sofort Low, Pullups für DIP), behebt sie aber nicht:
1. Relais-FET Q1 (Kontaktebene) ohne Gate-Pulldown → 100k Gate–GND ergänzen.
2. MCP16301 nur bis 30 V → für 24 V MCP16331 (50 V) vorsehen.
3. SM6T12CA an CAN zu hohe Kapazität → CAN-ESD-Diode (PESD2CAN o. ä.).
4. SWD-Stecker ohne NRST/VTref, BOOT0 fest auf GND → NRST herausführen, BOOT0-Jumper.
