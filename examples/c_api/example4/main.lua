
jack = require("luajack")

RBUFSIZE = 1000

MIDI_CONTROLLER_OUT = "jack-keyboard:midi_out"
SYNTH_MIDI_IN = "sinesynth.lua:midi_in"
SYNTH_OUT = "sinesynth.lua:audio_out"
CAPTURE_1 = "system:capture_1"
PLAYBACK_1 = "system:playback_1"
PLAYBACK_2 = "system:playback_2"
NAME = "myecho"

auto_connect = arg[1] or "manual"

-- Create a client and two audio ports:
c = jack.client_open(NAME, { no_start_server=true })
port_in = jack.input_audio_port(c, "audio_in")
port_out = jack.output_audio_port(c, "audio_out")
rbuf1 = jack.ringbuffer(c, RBUFSIZE, true) -- process to gui
rbuf2 = jack.ringbuffer(c, RBUFSIZE, true) -- gui to process


-- The process chunk just loads the C module implementing the RT callbacks,
-- and initializes it, passing it the Luajack objects (and possibly other
-- parameters). 
-- The C module will register its RT callbacks using the luajack.h C API.
PROCESS_CHUNK = [[
require("myecho").init( table.unpack(arg))
]]

t = jack.thread_loadfile(c, "gui", rbuf1, rbuf2) 
jack.process_load(c, PROCESS_CHUNK, c, port_in, port_out, t, rbuf2, rbuf1)

jack.shutdown_callback(c, function(_,code,reason) error(reason) end)

jack.activate(c)

function connect(c, port1, port2)
   if not jack.nport_connected(c, port1, port2) then
      jack.connect(c, port1, port2)
   end
end

if auto_connect=="auto" then
   connect(c, MIDI_CONTROLLER_OUT, SYNTH_MIDI_IN)
   connect(c, SYNTH_OUT, NAME..":audio_in")
   connect(c, NAME..":audio_out", PLAYBACK_1)
   connect(c, NAME..":audio_out", PLAYBACK_2)
elseif auto_connect=="capture" then
   connect(c, CAPTURE_1, NAME..":audio_in")
   connect(c, NAME..":audio_out", PLAYBACK_1)
   connect(c, NAME..":audio_out", PLAYBACK_2)
--elseif auto_connect=="myoption" then
--  ...
--  add your preferred connections here
--  ...
elseif auto_connect=="manual" then
   -- do not connect
else error("invalid option '"..auto_connect.."'")
end

jack.sleep()


