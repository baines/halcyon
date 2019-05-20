HALCYON is a C platform library in the vein of SDL, except with
a focus on extreme API minimalism and artificial limitation.

*NEW* single header micro version `hc0_linux.h` for Linux (no kbd/audio)
 - used by included demo program "spectre"

Features include:

 - Single window locked to 480p (240p upscaled 2x)
 - Fixed 60fps
 - Software rendering primitives
   - Point
   - Line
   - Filled Triangle
   - Textured Triangle
 - 4 channel sound (GameBoy inspired)
   - 2 pulse wave channels
   - 1 4-bit sample channel
   - 1 LSFR noise channel
 - Keyboard and MIDI input available in extension header
 - No mouse input

A demonstration "intro" is included.    
A tracker is also under development, to be included soon™.

To build everything, type: `make`
