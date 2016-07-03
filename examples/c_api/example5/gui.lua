-- This is the GUI script. It is loaded by the main chunk with 
-- jack.thread_loadfile() and runs in a separate thread.

fl = require("moonfltk")

rbuf_in  = arg[1] -- process to gui ringbuffer
rbuf_out = arg[2] -- gui to process ringbuffer

client, gui = jack.self()

------------------------------------------------------------------
-- GUI <-> process protocol
------------------------------------------------------------------

TAG_ON = 1
TAG_OFF = 2

function send_onoff() 
-- Sends a control message to the process, via the 'out' ringbuffer.
   local on, g, d = onoff:value(), gain:value(), delay:value()
   if not on or g==0 or d==0 then 
      jack.ringbuffer_write(rbuf_out, TAG_OFF)
   else
      jack.ringbuffer_write(rbuf_out, TAG_ON, string.pack("ff",g,d))
   end
end

function check_ringbuffer()
-- Retrieves all the messages available in the 'in' ringbuffer.
   while true do
      local tag, data = jack.ringbuffer_read(rbuf_in)
      if not tag then return end -- no more messages available
      -- ... STATUS_RSP ...
   end
end

------------------------------------------------------------------
-- GUI implementation
------------------------------------------------------------------

W, H = 300, 400            -- window width and height
M = math.floor(W/6)        -- margin (dials)
M1 = math.floor(W/20)      -- margin (button)
D = math.floor((W - 3*M)/2) -- dial side

win = fl.double_window(100, 100, W, H, "Echo")

o = fl.dial(M, M, D, D, "Delay")
o:type('line')
o:align('top')
o:color(0xff007700)
o:range(0, 4.0)
o:step(0.01)
o:value(0)
o:callback(function(o) 
   local val = o:value()
   o:label(string.format("Delay\n%.2f",val)) 
   send_onoff()
end)
delay = o


o = fl.dial(M+D+M, M, D, D, "Gain")
o:type('line')
o:align('top')
o:color(0x7700ff00)
o:range(0, 1.0)
o:step(.1)
o:value(0)
o:callback(function(o) 
   local val = o:value()
   o:label(string.format("Gain\n%.1f",val))  
   send_onoff()
end)
gain = o

o = fl.button(M1, M+D+M1, W-2*M1, H-D-M-2*M1)
o:value(true)
o:down_box('thin down box')
o:color(fl.BLACK)
o:selection_color(0x77007700)
o:labelsize(40)
o:callback(function(o) 
   local val = not o:value()
   o:value(val)
   o:label(val and "On" or "Off")
   send_onoff()
end)
onoff = o

gain:do_callback()
delay:do_callback()
onoff:do_callback()

win:done()
win:resizable(win)
win:show()

------------------------------------------------------------------
--  Main loop
------------------------------------------------------------------

while true do
   jack.testcancel() -- cancellation point
   check_ringbuffer()
   rv = fl.wait(0.1)
   if rv==nil or not win:shown() then os.exit() end
   if rv then -- an event occurred
      -- local ev = fl.event()
      -- ... if we want to handle other events ...
   end
end

