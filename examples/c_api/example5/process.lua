-- This is the script implementing the audio processing. 
-- It runs in the client's process thread.
-- 
-- In this example, the process callback is a Lua function.
-- The C module implements only the signal processing, while 
-- anything else is handled by the callback in Lua.
--
-- With respect to Example 4 (only-C RT callbacks) this is less
-- efficient and involves execution of Lua code in the RT thread.
-- It is, however, way simpler to code. If the heavy part of the
-- callback's work is the signal processing (as usually is), this
-- may be the best approach.

myecho = require("myecho")

client, port_in, port_out, gui, rbuf_in, rbuf_out = table.unpack(arg)

fs = jack.sample_rate(client)

myecho.init(fs)

TAG_ON = 1
TAG_OFF = 2
TAG_STATUS_REQ = 3

function process(nframes)

   -- GUI <-> process protocol -------------------------------
   local tag, data = jack.ringbuffer_read(rbuf_in)
   if tag then 
      if tag==TAG_ON then 
            gain, delay = string.unpack("ff", data)
            myecho.on(gain, delay)
      elseif tag==TAG_OFF then
            myecho.off()
--[[
      elseif tag==TAG_STATUS_REQ then
         gain, delay = myecho.status() 
         jack.ringbuffer_write(rbuf_out, TAG_STATUS_RSP, ......)
         jack.signal(client, gui)
--]]
      else -- ignore
      end
   end
      
   -- Audio processing ----------------------------------------
   jack.get_buffer(port_in)
   jack.get_buffer(port_out)
   -- Retrieve the raw buffer pointers, so that we can pass them
   -- to our signal processing function implemented in C:
   buf_in = jack.raw_buffer(port_in)
   buf_out = jack.raw_buffer(port_out)
   -- Do the signal processing:
   myecho.process(buf_in, buf_out, nframes) 

end

jack.process_callback(client, process)

