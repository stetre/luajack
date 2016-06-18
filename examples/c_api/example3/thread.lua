
-- This is the thread chunk, loaded by the main chunk with jack.thread_load().
-- It just sends pings and waits for pongs using the ringbuffers created by the 
-- main chunk for process<->thread communication.

fl = require("moonfltk")

rbuf_in  = arg[1] -- process to thread ringbuffer
rbuf_out = arg[2] -- thread to process ringbuffer

client, thread = jack.self()

next_send = 1    -- message counter (we will use it as tag)
next_receive = 1 -- next expected tag


function send_ping() 
-- Sends a 'ping' to the process context via the 'out' ringbuffer.
   jack.ringbuffer_write(rbuf_out, next_send, "ping"..next_send)
   next_send = next_send + 1
end


function check_ringbuffer()
-- Retrieves all the messages available in the 'in' ringbuffer,
-- checks them, and sends another ping.
   while true do
      local tag, data = jack.ringbuffer_read(rbuf_in)
      if not tag then return end -- no more messages available
      
      if (tag % 100) == 0 then -- print only every 100 ping-pong exchanges
         print("[thread] received tag="..tag.." data='"..data.."'")
      end

		o_ping:value(next_send-1)
		o_pong:value(next_receive-1)

      -- Check that the pong is correct
      if tag ~= next_receive then
         error("[thread] unexpected tag="..tag.." from process chunk")
      end
      if data ~= "pong"..tag then
         error("[thread] unexpected data='"..data.."' from process chunk")
      end

      next_receive = next_receive + 1

      send_ping()
   end
end


win = fl.window(340, 180, "LuaJack C-API - Example 3")
o_ping = fl.output(60, 40, 260, 30, "pings")
o_pong = fl.output(60, 100, 260, 30, "pongs")
win:done()
win:show()

function main_loop()
   while true do
      jack.testcancel() -- cancellation point
      check_ringbuffer()
		if not fl.check() then os.exit() end
   end
end


send_ping()
main_loop() 

