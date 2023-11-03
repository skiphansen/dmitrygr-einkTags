SOURCES += soc/cc111x/u1shared.c

FLAGS += --code-size 0x7f80

SOC = cc111x

BARCODE = barcode

# 0x7F80 and not 0x8000 to leave some space for epop update "eeprom img download status" header to fit
