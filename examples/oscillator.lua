-- LuaJack example: oscillator.lua
-- 
-- Generates a pure sinusoid at a given frequency and sends it to
-- a playback port.

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options]" ..
"\nGenerates a pure sinusoid and sends it to a playback port."..
"\nOptions:" ..
"\n  -f, --frequency <hz>      the frequency of the sinusoid (def: 440 hz)" ..
"\n  -d, --duration <sec>      duration of the sinusoid (def: 3 s)" ..
"\n  -o, --output <portname>   the playback port (def: system:playback_1)" ..
"\n  -h, --help                display this help message" ..
"\n  -v, --version             display version information and exit" ..
"\n\n"

function ShowVersion()
   print(fmt("%s: %s, %s", my_name, jack._VERSION, jack._JACK_VERSION))
end

function ShowUsage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   if errmsg then os.exit() end
end

short_opts = "f:d:o:hv"
long_opts = {
   frequency ='f',
   duration = 'd',
   output = 'o',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

local playback_port = optarg.o or "system:playback_1"
local f = optarg.f or 440 -- frequency (Hz)
local dur = optarg.d or 3 -- duration (s)


-- open the 'oscillator' client:
c = jack.client_open("oscillator") 

-- register the output port:
out = jack.output_audio_port(c, "out")

jack.process_load(c, [[
local pi = math.pi
local sin = math.sin

local c, out, f = table.unpack(arg)

local y = {} -- output samples
local n = 0  -- frame number

-- get the sample rate and compute the radian frequency:
fs = jack.sample_rate(c)
wT = 2*pi*f/fs -- radian frequency (rad/sample) = 2*pi*f/fs
print("sample rate = " .. fs .. " hz")
print("frequency = " .. f .. " hz")
print("radian frequency = " .. wT .. " rad/sample")

function process(nframes)
   -- get the port buffer
   jack.get_buffer(out)
   -- compute the samples
   for i=1,nframes do
      y[i] = .5*sin(wT*n)
      n = n+1
   end
   -- write the samples to the output port
   jack.write(out, table.unpack(y))
end

jack.process_callback(c, process);

]], c, out, f)

-- register other callbacks:
jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.xrun_callback(c, function() print("xrun") end)

-- activate the client:
jack.activate(c)

-- connect the output port to the playback port:
jack.port_connect(out, playback_port)

-- sleep while waiting for jack to call back:
jack.sleep(dur)

-- disconnect the port (superfluous because client:close() does it)
jack.port_disconnect(out) 
-- close the client (superfluous because it is automatically closed at exit)
jack.client_close(c)

print("bye!")

