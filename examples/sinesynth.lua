-- LuaJack example: sinesynth.lua
-- 
-- Naive synthesizer, receives MIDI events from a MIDI input port
-- and generates boring sinusoidal tones.
--
-- To try it, with  the JACK virtual keyboard, launch jackd (eg. using 
-- QjackCtl), then from two different shells execute:
-- $ jack-keyboard
-- $ lua sinesynth.lua auto
--
-- This should connect the MIDI out of jack-keyboard with the MIDI in of the synth,
-- and the audio out of the synth with the first 2 system playback ports.
-- If everything is alright, playing the jack-keyboard should result in sinusoidal
-- tones coming out from the system speakers, corresponding to the notes played on
-- the keyboard. The synth reacts only to note on/off events.
-- 
-- By executing without the 'auto' argument (i.e.: $ lua sinesynth.lua )
-- the ports are not automatically connected and must be connected manually
-- using QjackCtl or by other means.
--
jack = require("luajack")

NAME = arg[0]
auto_connect = arg[1] == "auto" and true or false
MIDI_CONTROLLER_OUT = "jack-keyboard:midi_out"

jack.verbose("on")

---------------------------------------------------------------------
PROCESS = [[
midi = require("luajack.midi")
c, in_port, out_port = table.unpack(arg)

pi = math.pi
sin = math.sin
active = {} -- active[m] = velocity (if key m is on) or nil (if off)

fs = jack.sample_rate(c)

local wT = {} -- wT[m] = radian frequency for MIDI key = m
for m = 1, 120 do -- piano keyboard: 21 to 108
   wT[m] = midi.note_frequency(m)*2*pi/fs
end

function set_velocity(m, v)
   if v == 0 then
      active[m] = nil
   else
      active[m] = v/127/2
   end
end

local y = {} -- output samples
n = 0 -- frame number
function compute_output(nframes)
   for i = 1, nframes do
      y[i] = 0
      for m,v in pairs(active) do -- add active notes
         y[i] = y[i] + v * sin(wT[m]*n)
      end
      n = n+1
   end
end

function get_midi_events()
   while true do
      time, msg = jack.read(in_port)
      if not time then return end
      ev, chan, par = midi.decode(msg)
      if ev == "note on" then
         --print(time, ev, par.key, par.velocity) -- should not print in an RT callback ...
         set_velocity(par.key, par.velocity)
      elseif ev == "note off" then
         --print(time, ev, par.key, par.velocity)
         set_velocity(par.key, 0)
      else
         --print(time, ev)
      end
   end
end   

function process(nframes)
   ec, lc = jack.get_buffer(in_port)
   if ec > 0 then 
      get_midi_events()
   end
   jack.get_buffer(out_port)
   compute_output(nframes)
   jack.write(out_port, table.unpack(y))
end

jack.process_callback(c, process)
]]
   
---------------------------------------------------------------------
c = jack.client_open(NAME)

in_port = jack.input_midi_port(c, "midi_in")
out_port = jack.output_audio_port(c, "audio_out")

jack.process_load(c, PROCESS, c, in_port, out_port)

jack.shutdown_callback(c, function(code, reason) error("shutdown from server") end)

jack.activate(c)

if auto_connect then
   jack.connect(c, MIDI_CONTROLLER_OUT, NAME..":midi_in")
   jack.connect(c, NAME..":audio_out", "system:playback_1")
   jack.connect(c, NAME..":audio_out", "system:playback_2")
end

fs = jack.sample_rate(c)
bs = jack.buffer_size(c)
period = bs/fs*1e6

jack.sleep(1)
jack.profile(c, "start")
print(string.format("fs=%d bufsz=%d (period=%.0f us)", fs, bs, period))

while true do
   jack.sleep(3)
   local n, min, max, mean, var = jack.profile(c)
   print(string.format("n=%d min=%.1f max=%.1f mean=%.1f dev=%.1f (us) period/max=%.0f",
      n, min*1e6, max*1e6, mean*1e6, math.sqrt(var)*1e6, period/(max*1e6)))
end

