# Dmitry Grinberg's Custom firmware for various eInk price tags

This code was taken from einkTags_0002_8051.zip downloaded from 
[dmitry.gr](https://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags).

I've modified the code to get it to compile by adding a header file (proto.h)
and excluding the missing datamatrix.h header unless DATAMATRIX is defined.

## License

Dimitry didn't include a LICENSE file or copyright headers in the source code
but the web page containing the ZIP file I downloaded says:

"The license is simple: This code/data/waveforms are free for use in **hobby and 
other non-commercial products**." 

For commercial use, <a href="mailto:licensing@dmitry.gr">contact him</a>.

## My Goal

My eventually goal is to add support for Chroma 42 tags which I recently 
bought on ebay.

