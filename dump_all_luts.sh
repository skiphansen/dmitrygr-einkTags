#/bin/bash
# dump all eeprom images

# set -x

dumpfile=logs/all_luts.lst

eeproms=`find . -name "*eeprom.bin"`

rm $dumpfile
for file in $eeproms; do
    args="-c dump_lut $file"
    ./chroma_shell/build/chroma_shell "$args" >> $dumpfile
done

