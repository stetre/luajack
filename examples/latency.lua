-- LuaJack example: latency.lua
-- 
-- Modified version of thru.lua, to show the use of the latency API.

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options]" ..
"\nRedirects input to output'"..
"\nOptions:" ..
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

short_opts = "hv"
long_opts = {
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

c = jack.client_open(my_name, { no_start_server=true })

port_in = jack.input_audio_port(c, "in")
port_out = jack.output_audio_port(c, "out")

jack.process_load(c, [[
c, port_in, port_out = table.unpack(arg)

function process(nframes)
-- just copy the frames from in to out:
	jack.get_buffer(port_in)
	jack.get_buffer(port_out)
	jack.copy(port_out, port_in)
end

jack.process_callback(c, process)
]], c, port_in, port_out)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code..")")
end)

function latency(_, mode)
	local port = mode == "capture" and port_out or port_in
	-- Notice that, although the client gets samples from port_in and writes
	-- them to port_out, from the JACK server's perspective port_out is a
	-- 'capture' port because it captures samples from it, and port_in is a
	-- 'playback' port, because  it sends samples to it.
	-- So, if mode is 'capture', we must recompute latencies for port_out,
	-- otherwise for port_in.
	-- Since this is just an example, we just add 1 frame to both min and max,
	-- just to see that we can actually set the latency range.
	local min, max = jack.latency_range(port, mode)
	print("latency callback: "..mode.." min="..min.." max="..max)
	jack.set_latency_range(port, mode, min+1, max+1)
end

jack.latency_callback(c, latency)

jack.activate(c)

jack.sleep(3)

-- this will trigger the latency callback for all ports:
jack.recompute_total_latencies(c) 

jack.sleep(3)

-- let's do it once more...
jack.recompute_total_latencies(c)

jack.sleep(3)

print("bye!")
