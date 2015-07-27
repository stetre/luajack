-- LuaJack example: thru.lua
-- 
-- Just copies data from an input port to an output port
-- (works with audio ports as well as with MIDI ports).

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options]" ..
"\nRedirects input to output'"..
"\nOptions:" ..
"\n        -m, --midi         use MIDI ports" ..
"\n        -h, --help         display this help message" ..
"\n        -v, --version      display version information and exit" ..
"\n\n"

function ShowVersion()
   print(fmt("%s: %s, %s", my_name, jack._VERSION, jack._JACK_VERSION))
end

function ShowUsage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   if errmsg then os.exit() end
end

short_opts = "mhv"
long_opts = {
   midi = 'm',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

c = jack.client_open(my_name, { no_start_server=true })

jack.verbose("on")

if optarg.m then
	port_in = jack.input_midi_port(c, "in")
	port_out = jack.output_midi_port(c, "out")
else
	port_in = jack.input_audio_port(c, "in")
	port_out = jack.output_audio_port(c, "out")
end

jack.process_load(c, [[
c, port_in, port_out = table.unpack(arg)

function process(nframes)
	jack.get_buffer(port_in)
	jack.get_buffer(port_out)
	jack.copy(port_out, port_in)
end

jack.process_callback(c, process)
]], c, port_in, port_out)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code..")")
end)

jack.activate(c)

jack.sleep() -- endless loop
