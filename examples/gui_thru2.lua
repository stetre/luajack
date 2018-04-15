-- LuaJack example: gui_thru.lua
-- 
-- This example implements an audio 'thru' with a simple GUI that displays
-- the number of processed frames in its title, and has a button to 
-- mute/unmute the output.
--
-- The MoonFLTK based GUI is running in the main thread where also the jack client
-- is created. ( https://github.com/stetre/moonfltk/ ).
--
-- The main thread and the process thread are connected with a couple of 
-- ringbuffers. This bidirectional channel carries a trivial 'control protocol'
-- between the GUI and the audio processing thread. The protocol is summarized
-- in the table below.
--
-- tag   data     direction      message meaning
-- ---------------------------------------------------
-- 1     nil      gui->process   mute
-- 2     nil      gui->process   unmute
-- 3     nil      gui->process   frames count request
-- 4     nframes  process->gui   frames count response
--
--
-- To run the example, launch qjackctl, start the JACK server and execute:
-- $ lua gui_thread.lua
--
-- Then execute the metronome example:
-- $ lua metro.lua
--
-- and via the qjackctl GUI connect the metro.lua 'out' to the gui_thru 'in'
-- and the gui_thru 'out' to a system 'in'. 
--


-----------------------------------------------------------------------
-- This is the inline script implementing the audio processing. 
-- It runs in the client's process thread.

PROCESS = [[
client, port_in, port_out, to_proc_rbuf, from_proc_rbuf = table.unpack(arg)

local fl = require("moonfltk.background")

tot_frames = 0
mute = false

function process(nframes)

   tot_frames = tot_frames + nframes

   -- GUI <-> process protocol
   local tag, data = jack.ringbuffer_read(to_proc_rbuf)
   if tag then 
      if tag==1 then mute=true        -- mute
      elseif tag==2 then mute=false   -- unmute
      elseif tag==3 then              -- frames count request
         jack.ringbuffer_write(from_proc_rbuf, 4, tostring(tot_frames))
         fl.awake()
      else -- ignore
      end
   end
      
   -- Audio processing 
   jack.get_buffer(port_in)
   jack.get_buffer(port_out)

   if mute then -- write 0's 
      jack.clear(port_out)
   else
      jack.copy(port_out, port_in)
   end

end

jack.process_callback(client, process)
]]

-----------------------------------------------------------------------
-- This is the main script.

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

jack.verbose("on")

port_in = jack.input_audio_port(c, "in")
port_out = jack.output_audio_port(c, "out")

from_proc_rbuf = jack.ringbuffer(c, 1000, true) -- process -> GUI
to_proc_rbuf   = jack.ringbuffer(c, 1000, true) -- GUI -> process

jack.process_load(c, PROCESS, c, port_in, port_out, to_proc_rbuf, 
                                                    from_proc_rbuf)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code..")")
end)

jack.activate(c)

-----------------------------------------------------------------------
-- GUI

fl = require("moonfltk")

timer = require("moonfltk.timer")
timer.init()

-- The T1 timer is used to send frame count requests at regular intervals
REQ_INTERVAL = 1.0 -- seconds
T1 = timer.create(REQ_INTERVAL, function()
   jack.ringbuffer_write(to_proc_rbuf, 3)
   timer.start(T1)
end)

function mute_cb(b) -- callback for the 'mute' button
   local tag = b:value() and 1 or 2 -- mute/unmute
   jack.ringbuffer_write(to_proc_rbuf, tag)
end

win = fl.window(340, 180, "thru")
b = fl.toggle_button(20, 40, 300, 100, "MUTE")
b:callback(mute_cb)
b:labelfont(fl.BOLD + fl.ITALIC)
b:labelsize(36)
b:labeltype('shadow')
win:done()
win:show()

timer.start(T1)

-- GUI thread main loop:
while fl.wait() do

   repeat
      local tag, data = jack.ringbuffer_read(from_proc_rbuf)
      if tag == 4 then -- frames count response
         win:label("frames: " .. data)
      end
   until not tag

   jack.sleep(0) -- just check for errors

end


