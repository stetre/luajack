
-- This is the thread chunk, loaded by the main chunk with jack.thread_load().
-- It just sends pings and waits for pongs using the ringbuffers created by the 
-- main chunk for process<->thread communication.

rbuf_in  = arg[1] -- process to thread ringbuffer
rbuf_out = arg[2] -- thread to process ringbuffer

client, thread = jack.self()

count = 1        -- message counter (we will use it as tag)
next_receive = 1 -- next expected tag


function send_ping() 
-- Sends a 'ping' to the process context via the 'out' ringbuffer.
   jack.ringbuffer_write(rbuf_out, count, "ping"..count)
   count = count + 1
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


function main_loop1()
   while true do
      jack.wait() -- block until someone invokes jack.signal() on this thread.
      check_ringbuffer()
   end
end


function main_loop2()
   while true do
      jack.testcancel() -- cancellation point
      check_ringbuffer()
      -- ...
      -- check here if a GUI event occurred, and if so, process it
      -- ...
   end
end


-- Send the first ping and enter the thread's main loop:
main_loop = main_loop1 -- See NOTE below.
print("using main_loop" .. (main_loop==main_loop1 and 1 or 2))
send_ping()
main_loop() 


-- NOTE: This example shows two alternative main loops, implemented in the
-- main_loop1() and main_loop2() functions, respectively (to select the main
-- loop, just change the assignment of the main_loop variable above).
--
-- In the main_loop1() alternative, the thread blocks on jack.wait() when it
-- has read all the available messages. When the process context sends a ping,
-- it also calls jack.signal() on the thread, which causes jack.wait() to 
-- return.
--
-- The use of jack.wait(), however, may be unacceptable if, for example, the 
-- thread needs to continuosly listen for GUI events (or for messages on a 
-- socket, data on a pipe, or whatever).
--
-- In such cases an implementation such as the main_loop2() alternative is 
-- more appropriate. It avoids blocking on jack.wait(), and continuosly
-- checks for incoming messages in the ringbuffer and then for other kind
-- of events (GUI, sockets, etc).
--
-- Notice the use of jack.testcancel() to insert a cancellation point (it is
-- just a binding to pthread_testcancel()). This is needed to ensure that LuaJack
-- can cancel the thread if the main exits for some reason.
-- It is not needed if jack.wait() or any other function that is itself a
-- cancellation point (e.g. socket.select()) is already used in the main loop.
-- (If you are not sure that this is the case, use it).
--

