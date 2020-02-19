# spc2wav

This is just a small CLI program for decoding an SPC file
into a WAV file, using Blargg's [snes_spc library](http://www.slack.net/~ant/libs/audio.html#snes_spc).

It uses a higher amplification level compared to most SPC decoders,
this can be controlled via CLI with the `--amp` flag.

The default amplification level is `512`, most players/decoders based on `snes_spc`
default to `256`

## LICENSE

MIT
