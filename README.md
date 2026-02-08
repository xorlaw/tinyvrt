# tinyvrt

a tiny program to handle virtual machines

Tinyvrt is, as the name suggests, a small program to handle virtual machines. It is written in about 130 lines of C. This means that customising it is pretty easy since you
dont need to sort through tons of code.

The main purpose of tinyvrt is that, when I was building my own small OS for fun, QEMU and other software felt clunky to use with my OS for testing. I also just made this for
fun.

## Using

To use tinyvrt, you must go into the source code yourself and change whatever you'd like. Most of the stuff you have to change is in the `#define` statements near the start of the code.
However, depending on what you will run on tinyvrt, some stuff may just not work. You will have to change the rest of the source to fit the OS you are running on it (if that is the case).

Please note:

- **tinyvrt is a HUGE WIP and should not be considered ready to use. Use it if you are okay with changing the code.**
  
  This will most likely be the case for a while, until I add more to the code to make it easier to use.

## Licensing

tinyvrt is licensed under GPL v2.


