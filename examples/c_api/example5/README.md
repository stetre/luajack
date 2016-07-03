
####LuaJack C-API - Example 5

This example also implements an echo effect, as Example 4, but using a
different approach where the process callback is implemented as a Lua
function, while the only part implemented in C is the signal processing.

This approach is less efficient than the one used in the previous examples,
because it involves executing Lua code in the RT thread. On the other hand,
it is easier to handle things such as ringbuffers or MIDI messages in Lua, so
this approach is maybe the most suitable for applications where the only
computational demanding part of the callback is really the audio signal processing.

The example is composed of the following parts:

- **myecho.c**: this is the Lua module that implements the signal processing 
in C. It exports a few functions for initializations and parameters control,
and a signal processing function (_myecho.process_()) to be called by the
Lua RT callback at every round. This function receives the pointers to the
input and output buffers to operate on (plus their dimension in number of frames).
The Lua callback can retrieve the pointers by means of the _jack.raw_buffer_()
function. The module is to be _require_()d and used only by the process chunk
(controlling it directly also from other threads, e.g. from the GUI, is a very 
very very bad idea).

- **process.lua**: this is the process chunk, implementing the process callback
in Lua. The callback's job is to handle the communication with the GUI (via 
ringbuffers), control the above mentioned C module on behalf of the GUI, and 
invoke the signal processing function.

- **gui.lua**: this script implements the GUI, using [MoonFLTK](https://github.com/stetre/moonfltk)
(the use of MoonFLTK is not mandatory).
The script is executed in a separate (non RT) client thread, and communicates with the
process thread via ringbuffers.

- **main.lua**: this is the main chunk that glues all together. It creates the JACK
client, the ringbuffers, registers the audio ports (and optionally connects them), 
executes the process and GUI threads, and finally enters the _jack.sleep_() endless loop.

