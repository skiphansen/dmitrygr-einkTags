<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/two_cats.png">

# Dmitry Grinberg's eInk price tag project

[https://github.com/skiphansen/dmitrygr-einkTags](https://github.com/skiphansen/dmitrygr-einkTags)

This code was taken from einkTags_0001.zip and einkTags_0002_8051.zip downloaded from 
[dmitry.gr](https://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags).

The tag code is from inkTags_0002_8051.zip, everything else is from inkTags_0001_8051.zip.
These two code bases were slightly incompatible with each other, but I've reconciled them.

The code has been modified to get it to compile and work with my debugger.
I've also been adding documentation as I learn.

The station and tag code has been sucessfully tested with a Chroma 74 tag.

## My Goal

My eventual goals are:
1. Port the subgig code to the [OpenEPaperLink](https://github.com/jjwbruijn/OpenEPaperLink) project.
2. Add support for the Chroma 42 and Chroma 60 tags which I recently bought on ebay.

The reason I've interested in ePaper displays is that I have several applications
that I'd like to use them for:

1. Monitoring the water level in my septic tank to prevent unpleasant surprises!
1. Charting daily tide levels.
1. Displaying the current [weather](https://github.com/G6EJD/ESP32-e-Paper-Weather-Display) and predictions.

I am only interested in the subgig tags based on the TI CC1110 since that's 
what I have and the [OpenEPaperLink](https://github.com/jjwbruijn/OpenEPaperLink) 
project has the 2.4 ghz zbs based tags well in hand.

See [Chroma.md](docs/Chroma.md) for more information on the Chroma series of tags.

See [Station.md](docs/Station.md) for information on building a base station / 
access port.

See [Chroma29.md](docs/Chroma29.md) for more information on the Chroma 29 tag.

## Status

| Target | Status |  
|-|-|  
|Station | Works! |
|chroma74r | Works! |
|chroma74y | Builds and probably works.<br>I don't have one for testing |
|chroma29r  | Works? (see below) |
|EPOP50  | Builds, but untested |
|EPOP900  | Builds, but untested  |
|zbs29v025  |Builds, but untested |
|zbs29v026|Builds, but untested |

## Chroma29r Status

The chroma29r target runs but no image is displayed on the screen on two
of the tags that I have tried.  The debug output shows the code running
as expected.

Dimitry says that the problem is probably that my tag has a different 
display type than the ones he has.  If you have a Chroma29r you might give
it a try and let me know if it works for you.

## License

Dimitry didn't include a LICENSE file or copyright headers in the source code
but the web page containing the ZIP file I downloaded says:

"The license is simple: This code/data/waveforms are free for use in **hobby and 
other non-commercial products**." 

For commercial use, <a href="mailto:licensing@dmitry.gr">contact him</a>.

