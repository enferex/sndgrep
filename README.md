sndgrep - Search for specific frequencies (tones) in PCM audio
==============================================================


What
----
sndgrep searches a provided input file or direct PCM stream of audio data for a
specified tone.  This tool was originally designed to find DTMF tones in audio
streams.

This tool can also generate tones (including dtmf) and write that output to a
file.

This is pre-alpha and there is a ton of testing that needs to be done.
Primarily, I only care about DTMF, so traditional tones have not been tested
(e.g., you can feel free to test these by NOT specifying the --dtmf command
line argument).


Limitations
-----------
Currently this tool only supports PCM as input to search, or output to create.
Specifically, 64bit PCM at an 8kHz sampling rate.


Dependencies
------------
libfftw: Fourier Transform Library
http://www.fftw.org

libalsa: (optional) Advanced Linux Sound Architecture
http://www.alsa-project.org/


Optional Build Settings
-----------------------
Generated audio can be directly played from sndgrep, but sndgrep must be built
and the optional "libalsa" dependency is required.  To achieve this annoying
feature build sndgrep as:
    make EXTRA_CFLAGS="-DDEBUG_PLAYSOUND -lasound"


Examples
--------
* Generate a 1 second DTMF tone of the '5' key:
sndgrep --dtmf --generate -d 1 -t 5 myoutput.pcm

* Search for the DTMF '5' tone in a provided input file:
sndgrep --dtmf --search -t 5 somefile.pcm

* Search for the DTMF '5' tone in an audio stream:
sndgrep --dtmf --search -t 5 < /dev/mystream


Super Awesome Resources
-----------------------
* Using ALSA to generate sound: http://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm_min_8c-example.html
* Getting a frequency from an FFT result: https://stackoverflow.com/questions/4364823/how-to-get-frequency-from-fft-result
* DTMF: https://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling */


Contact
=======
enferex 
mattdavis9@gmail.com
