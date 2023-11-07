This subdirectory contains scripts to build the correct version of SDCC (4.0.7) for compiling 8051 based firmware.

To use simply source setup_sdcc.sh before building.  The first time setup_sdcc.sh is run proper version of SDCC sources will be checkout from github and built locally.
Finally the environment variable CC will be set to 'sdcc' and the path to the local version of sdcc will be added to the beginning of the path.

The script unset_sdcc.sh can be sourced to unset the CC environment variable and to restore the path.

For example:

````
skip@Dell-7040:~dmitrygr-einkTags$ . ../sdcc/setup_sdcc.sh 
Cloning into 'sdcc'...

(truncated...)

Added /home/skip/dmitrygr-einkTags/sdcc/sdcc-4.0.7/bin to PATH
skip@Dell-7040:~dmitrygr-einkTags$ #verify correct version of sdcc is run
skip@Dell-7040:~dmitrygr-einkTags$ sdcc -v
SDCC : mcs51/gbz80 4.0.7 #0 (Linux)
published under GNU General Public License (GPL)
skip@Dell-7040:~dmitrygr-einkTags$
````

