# Sound
A command-line tool for generating .wav files of variable frequency, sample rate, wave function, and more.

## Build

```shell
> make
```

## CLI interface

```shell
> ./sound --help
usage: ./sound [-f|--file <file=stdout>]
               [-a|--append <file>]
               [-d|--duration <duration=1000>]
               [-v|--volume <volume=33.333333>]
               [-s|--sample-rate <sample-rate=44100]
               [-w|--wave-function <wave=sine>]
               [-o|--overtones <overtones=0>]
               frequency [frequency ...]
```

## Quick demo

1. Install [Sox](http://sox.sourceforge.net/)
2. Run: `./playsound -d 2000 -w triangle -o 2 -v 20 440 550 660`
