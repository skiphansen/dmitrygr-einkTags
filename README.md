# Dmitry Grinberg's Custom firmware for various eInk price tags

This code was taken from einkTags_0001.zip and einkTags_0002_8051.zip downloaded from 
[dmitry.gr](https://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags).

The tag code is from inkTags_0002_8051.zip, everything else is from inkTags_0001_8051.zip.
These two code bases are slightly incompatible with each other, but I'm working on
reconciling them.

The code has been modified to get it to compile by adding a header file (proto.h)
and excluding the missing datamatrix.h header unless DATAMATRIX is defined.

## License

Dimitry didn't include a LICENSE file or copyright headers in the source code
but the web page containing the ZIP file I downloaded says:

"The license is simple: This code/data/waveforms are free for use in **hobby and 
other non-commercial products**." 

For commercial use, <a href="mailto:licensing@dmitry.gr">contact him</a>.

## My Goal

My eventually goals are:
1. Port the code to the [OpenEPaperLink](https://github.com/jjwbruijn/OpenEPaperLink) project.
2. Add support for Chroma 42 and Chroma 60 tags which I recently bought on ebay.

The reason I've interested in ePaper displays is that I have three applications
that I'd like to use them for:

1. Monitoring the water level in my septic tank to prevent unpleasant surprises!
1. Charting daily tide levels.
1. Displaying current [weather](https://github.com/G6EJD/ESP32-e-Paper-Weather-Display) and predictions.

## Status

| Target | Status |  
|-|-|  
|chroma74r | Works! |
|chroma29r  | Works? (see below) |
|chroma74y | Builds, but untested |
|EPOP50  | Builds, but untested |
|EPOP900  | Builds, but untested  |
|zbs29v025  |Builds, but untested |
|zbs29v026|Builds, but untested |


## Chroma74r Status

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/chroma74_first_contact.png" width=50%>

The Chroma74r target works at least as far as putting an image on the screen. I haven't built a station
yet so I don't know if it's fully functional or not.

## Chroma29r Status

The chroma29r target runs but no image is displayed on the screen on two
of the tags that I have tried.  The debug output shows the code running
as expected.

Dimitry says that the problem is probably that my tag has a different 
display type than the ones he has.  If you have a Chroma29r you might give
it a try and let me know if it works for you.

## Displaydata Chroma Aeon Series Specs

| Model | Screen size | Resolution | EEPROM size | Supported |
| -| -| - | - | -|
| Chroma 16 | 1.08 x 1.08 in | 152 x 152 |? | N |
| Chroma 21 | 1.9 x 1.00  in | 212 x 104 |? | N |
| Chroma 27 | 2.4 x 1.20  in | 296 x 152 |? | N |
| Chroma 29 | 2.6 x 1.10  in | 296 x 128 | 128K | Y (some versions) |
| Chroma 37 | 3.2 x 1.85  in | 416 x 240 |? | N |
| Chroma 42 | 3.3 x 2.50  in | 400 x 300 | 1024K | N |
| Chroma 60 | 4.7 x 3.50  in | 648 x 480 |? | N |
| Chroma 74 | 6.4 x 3.50  in | 800 x 480 | 1024K | Y |
| Chroma 125 | 10.0 x 7.50  in | 1304 x 984 | | N |



