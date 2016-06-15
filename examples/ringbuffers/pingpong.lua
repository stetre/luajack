-- LuaJack example: pingpong.lua
-- 
-- Shows the use of ringbuffers for bi-directional communication
-- between the rt process thread and a non rt client thread.

jack = require("luajack")

jack.verbose("on")


opt = arg[1]
if not opt or opt=="wait" then WAIT=true
elseif opt=="poll" then WAIT=false 
else error("invalid argument")
end


THREAD = [[
rbuf_in, rbuf_out, wait = table.unpack(arg)

local function receive()
   local tag, data = jack.ringbuffer_read(rbuf_in) 
   if not tag then return end
   print("thread received: tag=".. tag .. " data=".. data)
   jack.ringbuffer_write(rbuf_out, tag, "pong" .. tag)
end

if wait then
	-- Alternative 1: wait until the process thread signals.
	print("Blocking")
	while true do
		jack.wait()
   	receive()
	end
else
	-- Alternative 2: don't wait, and repeatedly poll the ringbuffer instead.
	-- We need to add a cancellation point to avoid hanging up when the client exits.
	print("Non-blocking")
	while true do
		jack.testcancel() -- test if the client is exiting (cancellation point)
		receive()
	end
end
]]

PROCESS = [[
c, thread, rbuf_in, rbuf_out = table.unpack(arg)
 
local m = 0
local tag = 0
function process(nframes)
   -- check if the user thread wrote some data to rbuf_in
   -- (since this is the rt thread, it can not block on wait()):
   local rtag, rdata = jack.ringbuffer_read(rbuf_in)
   if rtag then -- some data is available
      -- we should not print in rt threads, but this is just an example:
      print("main received:   tag=".. rtag .. " data=".. rdata)
   end
   m = m + 1
   if m == 100 then
      -- write some data to rbuf_out and signal the user thread
      jack.ringbuffer_write(rbuf_out, tag, "ping" .. tag )
      jack.signal(c, thread)
      tag = tag + 1
      m = 0
   end
end

jack.process_callback(c, process)
]]

-- Create the client:
c = jack.client_open("pingpong")

-- Create the ringbuffers:
local rbuf1 = jack.ringbuffer(c, 8000, true) -- thread to client
local rbuf2 = jack.ringbuffer(c, 8000, true) -- client to thread

-- Create the thread and process contexts:
thread = jack.thread_load(c, THREAD, rbuf2, rbuf1, WAIT)
jack.process_load(c, PROCESS, c, thread, rbuf1, rbuf2)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.activate(c)

jack.sleep(10)
print("exiting")

