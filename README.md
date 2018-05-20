## LuaJack: Lua bindings for the JACK Audio Connection Kit

LuaJack is a Lua binding library for the [JACK Audio Connection Kit](http://jackaudio.org/).

It runs on GNU/Linux and requires [Lua](http://www.lua.org/) (>=5.3)
and [JACK](http://jackaudio.org/downloads) (API >= v0.124.1).

_Authored by:_ _[Stefano Trettel](https://www.linkedin.com/in/stetre)_

[![Lua logo](./doc/powered-by-lua.gif)](http://www.lua.org/)

#### License

MIT/X11 license (same as Lua). See [LICENSE](./LICENSE).

#### Documentation

See the [Reference Manual](https://stetre.github.io/luajack/doc/index.html).

#### Getting and installing (Ubuntu)

Setup the build environment as described [here](https://github.com/stetre/moonlibs), then:

```sh
$ git clone https://github.com/stetre/luajack
$ cd luajack
luajack$ make
luajack$ sudo make install
```

#### Example

The example below creates a JACK client that simply copies samples from an
input port to an output port. Other examples can be found in the **examples/**
directory contained in the release package.


```lua
-- Script: example.lua
jack = require("luajack")

-- Open a JACK client:
c = jack.client_open("myclient")

-- Create two ports:
p_in = jack.input_audio_port(c, "port_in")
p_out = jack.output_audio_port(c, "port_out")

-- Load the 'process' chunk:
jack.process_load(c, [[
c, p_in, p_out = table.unpack(arg)

function process(nframes)
   -- Copy the samples from the input port to the output port:
   jack.get_buffer(p_in)
   jack.get_buffer(p_out)
   jack.copy(p_out, p_in) 
end

-- Register the (rt) process callback:
jack.process_callback(c, process)
]], c, p_in, p_out)

-- Register a non-rt callback:
jack.shutdown_callback(c, function() error("shutdown from server") end)

-- Activate the client:
jack.activate(c)

-- Sleep, waiting for JACK to call back: 
jack.sleep()
```

The script can be executed at the shell prompt with the standard Lua interpreter:

```shell
$ lua example.lua
```

#### See also

* [MoonLibs - Graphics and Audio Lua Libraries](https://github.com/stetre/moonlibs).

