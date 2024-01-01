# RF Studio configuration files


## CC1101 and CC1110 Differences

The CC1101 has 3 physical general purpose pins GDO0, GDO1, and GDO2 whereas
the CC1110 can optionally output the same signals as the CC1101 on the
general purpose pins P1.7 (GDO2), P1.6 (GDO1) and P1.5 (GDO0).

The register order of registers following SYNC1 are the same on the CC1101 and
CC1110 but SYNC1 is at offset 0x04 on teh CC1101 and offset 0x0 on the CC1110.

## Configuration Register differences

| Register | CC1101 | CC1110 |
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
| MCSM0.0 | XOSC_FORCE_ON | see below |
| MCSM0.1 | PIN_CTRL_EN| see below |
| MCSM0[3:2] | PO_TIMEOUT | see below |
| MCSM0[1:0] | | CLOSE_IN_RX |
| MCSM0[3:2] | | reserved |
| WOREVT1 | Adr 0x1e | SFR 0xa5 |
| WOREVT0 | Adr 0x1f | SFR 0xa3 |
| WORCTRL | Adr 0x20 | SFR 0xa2 |
| RCCTRL1 | Adr 0x27 | does not exist |
| RCCTRL0 | Adr 0x28 | does not exist |
| FSTEST | Adr 0x29 | does not exist |
| PTEST | Adr 0x2a | does not exist |
| AGCTEST | Adr 0x2b | does not exist |
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


