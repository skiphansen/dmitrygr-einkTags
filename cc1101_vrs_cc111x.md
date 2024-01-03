## Differences between CC1101 and CC111x

The CC1101 has 3 physical general purpose pins GDO0, GDO1, and GDO2 whereas
the CC111x can optionally output the same signals as the CC1101 on the
general purpose pins P1.7 (GDO2), P1.6 (GDO1) and P1.5 (GDO0).

## Configuration Register differences

The order of registers following SYNC1 are the same on the CC1101 and
CC111x but SYNC1 is at offset 0x04 on the CC1101 and offset 0x0 on the CC1110.

The CC1101 has several registers that do not exist on the CC111x and there
a few differences in the supported features.

| Register | CC1101 | CC111x |
| - | - | - |
| IOCFG0.7 | TEMP_SENSOR_ENABLE | Not used |
| PKTCTRL1.3 | CRC_AUTOFLUSH | Not used |
| IOCFG2 | Adr 0x00 | Offset 0x2f |
| IOCFG1 | Adr 0x01 | Offset 0x30 |
| IOCFG0 | Adr 0x02 | Offset 0x31 |
| FIFOTHR | Adr 0x03 | does not exist | 
| PKTCTRL0[5:4] | | Async serial mode n/a |
| PKTCTRL0[1:0] | | Infinite packet len mode n/a |
| MDMCFG2[6:4] || 4-FSK mode n/a |
| MCSM0.0 | XOSC_FORCE_ON | CLOSE_IN_RX[0] |
| MCSM0.1 | PIN_CTRL_EN| CLOSE_IN_RX[1] |
| MCSM0[3:2] | PO_TIMEOUT | "Reserved. Refer to SmartRF Studio software [9] for settings" |
| WOREVT1 | Adr 0x1e | SFR 0xa5 |
| WOREVT0 | Adr 0x1f | SFR 0xa3 |
| WORCTRL | Adr 0x20 | SFR 0xa2 |
| RCCTRL1 | Adr 0x27 | does not exist |
| RCCTRL0 | Adr 0x28 | does not exist |
| FSTEST | Adr 0x29 | does not exist |
| PTEST | Adr 0x2a | does not exist |
| AGCTEST | Adr 0x2b | does not exist |
| PA_TABLEx | Adr 0x3e | offset 0x27 - 0x2e |
| PARTNUM | 0x00 | 0x01 |
| MARCSTATE | | State XOFF does not exist|
| WORTIME1 | Adr 0x36 | SFR 0xa6 |
| WORTIME0 | Adr 0x37 | SFR 0xa5
|PKTSTATUS.2 | GDO2 value | Not used |
|PKTSTATUS.0 | GDO0 value | Not used |
| TXBYTES | Adr 0x3a | does not exist|
| RXBYTES | Adr 0x3b | does not exist|
| RCCTRL1_STATUS | Adr 0x3c | does not exist|
| RCCTRL0_STATUS | Adr 0x3d | does not exist|

## CC1101 and CC1110 Command Strobe Differences

On the CC1101 the command strobes are defined by the register address written (0x30 to 0x3d).
On the CC1110 the command strobes are defined by the value written to the SFST register.

13 command strobes are defined for the CC1101, but only 5 are defined for the CC1110.

| Command strobe | CC1101 address | CC1110 SFST value |
| - | - | - |
|SRES|0x30| - |
|SFSTXON|0x31| 0x00 |
|SXOFF|0x32| - |
|SCAL|0x33| 0x01 |
|SRX|0x34| 0x02 |
|STX|0x35| 0x03 |
|SIDLE|0x36| 0x04 |
|SWOR|0x38| - |
|SPWD| 0x39|-|
|SFRX|0x3A|-|
|SFTX|0x3B|-|
|SWORRST|0x3C|-|
|SNOP|0x3D|0x05 - 0xff |

