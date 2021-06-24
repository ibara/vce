VCE
===
`VCE`, the Visual Code Editor, is a small, fast, and
portable text editor for terminals.

It is designed to provide a simple, consistent editing
experience between modern
[Unix](https://www.openbsd.org/)
and retro computers.

`VCE` is the text editor for my simple retro software suite,
which includes an
[assembler](https://github.com/ibara/a80),
[linker](https://github.com/ibara/l80),
[archiver](https://github.com/ibara/ar80),
and
[disassembler](https://github.com/ibara/d80).

`VCE` is built upon
[Anthony's Editor](https://github.com/SirWumpus/ioccc-ae).

Why?
----
People seem to not like CP/M
[`ED`](http://www.cpm.z80.de/randyfiles/DRI/ED.pdf)
very much. I can somewhat understand this. After all, what
is the point of having today's video terminals if we are
still using yesterday's line editors?

Originally I was going to use
[`qe`](https://github.com/davidgiven/cpmish/blob/master/cpmtools/qe.c)
from
[CP/Mish](https://cowlark.com/cpmish/)
but I could not get `qe` to work properly on a modern
terminal.

I learned about
[`VDE`](https://sites.google.com/site/vdeeditor/Home)
from a
[video](https://www.youtube.com/watch?v=FGWshrMZcCc)
by the
[CP/Mish author](https://cowlark.com/)
but unfortunately `VDE` is not open source.

I really liked the look and feel of `VDE`. I also figured I
could go even simpler. And so, `VCE` was born.

I also wanted to experiment with the gap buffer technique
found in Anthony's Editor. Specifically, when working with
systems that are memory constrained, I wanted to have a way
of knowing how much space is remaining in memory to work
with. There is a rest counter at the top right of `VCE` with
that information.

Building
--------
`make` for Unix, `make cpm` for CP/M, `make dos` for MS-DOS.

Running
-------
```
usage: vce [file]
```

Controls
--------
* `^E`    : up
* `^S`    : left
* `^D`    : right
* `^X`    : down
* `Esc-g` : goto line
* `Esc-l` : redraw screen
* `Esc-s` : save
* `Esc-q` : quit (does not prompt saving)
* `Esc-v` : display version number

Arrow keys will also move the cursor on Unix terminals.

You must press Enter after saving to continue working.

License
-------
For those in jurisdictions that recognize the public domain,
`VCE` is placed into the public domain.

For those in jurisdictions that do not recognize the public
domain, `VCE` is released under the ISC License.

See `LICENSE` for a copy of the ISC license.

Caveats
-------
ANSI terminals and the MS-DOS console only (for now...).

No automatic newline at EOF.

Bugs
----
I don't have any CP/M terminals. I have VT52 emulation via
[tnylpo](https://gitlab.com/gbrein/tnylpo)
but that is it. More terminal support would be good.

Terminal size should be autodetected, at least on Unix.

CP/M might be real slow on real hardware.

CP/M garbles the display of the first few characters after
typing your first keystroke.

The display ticks up one if you are the very bottom of a
file and begin writing. It would be nice to fix that.

It's always possible that the rest counter is incorrect.

No Unicode. Hopefully that can be accomplished.
