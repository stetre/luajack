-- LuaJack example: echo.lua
-- 
-- Implements a simple echo effect.
--
--    x --.--------------------->(+)--> y
--        |                       ^
--        |     ________     g    |
--        '----|__z^-M__|---|>----'
-- 
-- 
-- To try it out, launch jackd (e.g, using QjackCtl), then in different shells:
-- 
-- $ jack-keyboard              # the MIDI controller
-- $ lua sinesynth.lua          # the synthesizer
-- $ lua echo.lua auto 0.4 0.8  # echo with gain=0.4 and delay of 0.8 seconds
-- 
-- With the 'auto' option, the script automatically connects jack-keyboard to 
-- sinesynth, sinesynth with the echo, and the echo with the system playback.
-- By playing notes on the keyboard you should then hear tones with echo.
--
-- An alternative way of running the exaample is with the 'capture' option, e.g.
-- $ lua echo.lua capture 0.4 0.8
-- This connects system:capture_1 to the echo input, and the echo output to the
-- system playback, thus applying the echo to whatever sound source is plugged
-- into system:capture_1.
--
-- With the "manual" option, no automatic connection is performed.
--

jack = require("luajack")

MIDI_CONTROLLER_OUT = "jack-keyboard:midi_out"
SYNTH_MIDI_IN = "sinesynth.lua:midi_in"
SYNTH_OUT = "sinesynth.lua:audio_out"
CAPTURE_1 = "system:capture_1"
PLAYBACK_1 = "system:playback_1"
PLAYBACK_2 = "system:playback_2"

NAME = arg[0]

auto_connect = arg[1] or "manual"

gain = tonumber(arg[2]) or 0.3    -- 
delay = tonumber(arg[3]) or 0.5   -- seconds
if gain > 1 and gain < 0 then error("invalid gain="..gain) end
if delay < 0 then error("invalid delay="..delay) end

--jack.verbose("on")

---------------------------------------------------------------------
PROCESS = [[
c, in_port, out_port, gain, delay = table.unpack(arg)

fs = jack.sample_rate(c)
bufsz = jack.buffer_size(c)
M = math.floor(delay*fs) -- delay line length
print("echo parameters: delay="..delay.." s"..", gain="..gain.." (delay line length: "..M..")")

D = { } -- delay line
for i = 1, M do D[i] = 0 end
ptr = 1 -- delay line pointer

function delayline(x)
   local y = D[ptr]
   D[ptr] = x
   ptr = ptr + 1
   if ptr > M then ptr = 1 end
   return y
end

input = {} -- output samples
output = {} -- output samples

for i = 1, bufsz do output[i] = 0 input[i] = 0 end

function process(nframes)
   jack.get_buffer(in_port)
   jack.get_buffer(out_port)
   input = { jack.read(in_port, nframes) }
   --assert(#input == nframes)
   for i = 1, nframes do
      local x = input[i]
      output[i] = x + delayline(x)*gain
   end
   jack.write(out_port, table.unpack(output))
end

jack.process_callback(c, process)
]]
   
---------------------------------------------------------------------
c = jack.client_open(NAME)

fs = jack.sample_rate(c)
bs = jack.buffer_size(c)
period = bs/fs*1e6
print(string.format("fs=%d bufsz=%d (period=%.0f us)", fs, bs, period))

in_port = jack.input_audio_port(c, "audio_in")
out_port = jack.output_audio_port(c, "audio_out")

jack.process_load(c, PROCESS, c, in_port, out_port, gain, delay)

jack.shutdown_callback(c, function(code, reason) error("shutdown from server") end)

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
