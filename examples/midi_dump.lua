-- LuaJack example: midi_dump.lua
-- 
-- LuaJack version of JACK's midi_dump.c example.

jack = require("luajack")
midi = require("luajack.midi")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =
"Usage: lua " .. my_name .. " [options]" ..
"\nListens for MIDI events on a jack MIDI port and prints them to stdout."..
"\nOptions:" ..
"\n        -a, --abstime        use absolute timestamps relative to application start" ..
"\n        -p, --play           play notes on an output audio port" ..
"\n        -v, --version        display version information and exit" ..
"\n\n"

function ShowVersion()
   print(fmt("%s: %s, %s", my_name, jack._VERSION, jack._JACK_VERSION))
end

function ShowUsage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   if errmsg then os.exit() end
end

short_opts = "aphv"
long_opts = {
	abstime = 'a',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

-- create the client:
c = jack.client_open(my_name)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code..")")
end)

-- create the input port:
midi_in = jack.input_midi_port(c, "midi_in")

-- create a ringbuffer for process to thread communication:
rbuf = jack.ringbuffer(c, 1000, true)

-- create the thread and load the thread script in it:
t = jack.thread_load(c, [[
jack = assert(jack)
midi = require("luajack.midi")

rbuf = arg[1]

while true do
	jack.wait()
	while true do 
		local tag, msg = jack.ringbuffer_read(rbuf)
		if not tag then break end
		print(midi.tostring(tag, msg))
	end
end
]], rbuf)

-- load the process script:
jack.process_load(c, [[
c, t, midi_in, rbuf, abstime  = table.unpack(arg)

local n = 0 -- absolute frame number

function process(nframes)
	local ec, lc = jack.get_buffer(midi_in)
	if ec > 0 then 
		local time, msg = jack.read(midi_in)
		while time do
			-- write to ringbuffer
			local tag = abstime and time+n or time
			jack.ringbuffer_write(rbuf, tag, msg)
			jack.signal(c, t)
			time, msg = jack.read(midi_in)
		end
	end
	n = n + nframes
end

jack.process_callback(c, process)

]], c, t, midi_in, rbuf, optarg.a)

-- activate client and enter the sleep loop:
jack.activate(c)
jack.sleep()

