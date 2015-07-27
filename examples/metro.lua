-- LuaJack example: metro.lua
-- 
-- LuaJack version of JACK's metro.c example (simplified, with no amplitude envelope).

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"" .. my_name .. " - A JACK-based metronome."..
"\nUsage: lua " .. my_name .. " [options]" ..
"\nOptions:" ..
"\n  -b, --bpm <bpm>           beats per minute (def: 120)" ..
"\n  -f, --frequency <hz>      the frequency of the tone (def: 880 hz)" ..
"\n  -a, --amplitude <amp>     maximum amplitude (between 0 and 1, def: 0.5)" ..
"\n  -d, --duration <sec>      duration of the tone (def: 0.1 s)" ..
"\n  -n, --name <name>         name for metronome client (def: '".. my_name .. "')" ..
"\n  -t, --transport           transport aware" ..
"\n  -p, --playback <port>     connect to playback port" ..
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

function Error(errmsg) 
   print("Error: " .. errmsg)
   print("(type 'lua " .. my_name .. " -h' for help)")
   os.exit()
end

short_opts = "b:f:a:d:n:tp:hv"
long_opts = {
   bpm ='b',
   frequency ='f',
   amplitude = 'a',
   duration = 'd',
   name = 'n',
   transport = 't',
   playback = 'p',
   help = 'h',
   version = 'v',
}

-- get arguments and options:
optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

local name = optarg.n or my_name

local bpm = optarg.b or 120
bpm = math.tointeger(bpm)
if not bpm or bpm < 0 then 
   Error("bpm must be a positive integer")
end

local freq = optarg.f or 880
freq = tonumber(freq) 
if not freq or freq < 0 then
   Error("frequency must be a positive number")
end

local max_amp = optarg.a or 0.5
max_amp = tonumber(max_amp)
if not max_amp or max_amp < 0 or max_amp > 1 then
   Error("amplitude must be a number between 0 and 1")
end

local dur = optarg.d or 0.1
dur = tonumber(dur)
if not dur or dur < 0 then
   Error("duration must be a positive number")
end

local transport_aware = optarg.t and true or false
dur = tonumber(dur)
if not dur or dur < 0 or dur >= 60/bpm then 
   Error("duration must be a positive number and smaller than 60/bpm")
end

local playback_port = optarg.p

local port_name = tostring(bpm) .. "_bpm"

print("client name = " .. name)
print("bpm = " .. bpm)
print("output port = " .. port_name)
print("playback port = " .. (playback_port or "none"))
print("tone frequency = " .. freq)
print("tone amplitude = " .. max_amp)
print("tone duration = " .. dur)
print("transport aware = " .. (transport_aware and "yes" or "no"))

-- open the client:
c = jack.client_open(name, { no_start_server=true })   

-- create the output port:
out = jack.output_audio_port(c, port_name)

-- load the process script:
jack.process_load(c, [[
local c = arg[1]
local out = arg[2]
local bpm = arg[3]
local freq = arg[4]
local max_amp = arg[5]
local dur = arg[6]
local transport_aware = arg[7]

local offset = 1
local wave_length
local wave_table = {}

local function round(x)
   local y = math.floor(x)
   return x%1 > 0.5 and y+1 or y
end

-- get the sample rate and build the wave table
local fs = jack.sample_rate(c)
wave_length = round(60 * fs / bpm)
tone_length = round(fs * dur)
scale = 2 * math.pi * freq / fs
for i = 1, tone_length do
	wave_table[i] = max_amp * math.sin(scale*(i-1))
end
for i = tone_length+1, wave_length do
   wave_table[i] = 0
end

local function process_silence(nframes)
   jack.get_buffer(out)
   jack.clear(out)
end

local function process_audio(nframes)
   jack.get_buffer(out)
   local frames_left = nframes
   while (wave_length - offset) < frames_left do 
      jack.write(out, table.unpack(wave_table, offset, wave_length))
      frames_left = frames_left - (wave_length - offset + 1)
      offset = 1
   end
   if frames_left > 0 then
      jack.write(out, table.unpack(wave_table, offset, offset + frames_left))
      offset = offset + frames_left
   end
end

local function process(nframes)
   if transport_aware then
      local state, pos = jack.transport_query(c)
      if state ~= "rolling" then
         process_silence(nframes) 
         return
      end
   end
   process_audio(nframes)
end

jack.process_callback(c, process)

]], c, out, bpm, freq, max_amp, dur, transport_aware)

-- register other callbacks:
jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

-- activate the client:
jack.activate(c)

-- optionally connect to playback port:
if playback_port then
   jack.port_connect(out, playback_port)
end

-- sleep while waiting for jack to call back:
jack.sleep()

