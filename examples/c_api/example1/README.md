
####LuaJack C-API - Example 1

This is a simple example showing how to use the LuaJack C-API (luajack.h) to 
implement the RT callbacks in C, which may be beneficial for computational
intensive audio processing.

The idea is to implement the RT callbacks in a Lua module written in C 
(mymodule.c in this example), and to load and initialize it in the process
chunk. The main and the process chunk are written in Lua, as usual.

The C module exports an initialization function that receives the LuaJack
objects it will operate on (clients, ports, threads, ringbuffers), and 
registers its RT callback(s) using the LuaJack C-API.

By doing so, when the client is active and an RT event occurs, LuaJack 
executes the module's C callbacks instead of executing Lua code as it would
do in the usual case where the process chunk registers Lua callbacks.

#####Running the example

The example creates two audio ports (in and out) and just copies the input
samples to the output. We can test it using the metronome example (metro.lua)
to generate tones, connecting its output to our 'in', and our 'out' to a 
system output port. In order to do so:

1. Compile our example:

    ```sh
    $ make
    ```

   This will generate the mymodule.so library. Lua searches for this library when 
   the process chunk executes _require("mymodule")_.


2. Start the JACK server, using [QjackCtl](http://qjackctl.sourceforge.net/).

3. Launch the metronome:
   
    ```sh
    $ lua metro.lua
    ```  
  
4. Launch our example:

    ```sh
    $ lua main.lua
    ```

    (You may need to tell the linker the path to libluajack.so. See the configure.sh
    script.)

5. Using the QjackCtl GUI, connect the ports as shown here:

    ```
    metro.lua/120_bpm ----> mymodule/in
    mymodule/in ----------> system/playback_1
    ```

If everything goes fine, you should hear the tone from the speaker corresponding
to system/playback-1.
    

