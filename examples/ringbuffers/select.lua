-- LuaJack example: select.lua
-- 
-- Shows the use of ringbuffers with select() in client threads.

jack = require("luajack")

jack.verbose("on")

THREAD = [[
selectable = require("luajack.selectable")
socket = require("socket")

rbuf = arg[1]
-- create the 'selectable' ringbuffer:
r = selectable(jack, rbuf)

-- select() loop:
while true do
   local rlist, wlist, errmsg = socket.select({ r }, nil, .5)
   if errmsg then print("select:", errmsg) end
   if rlist then 
      for _, s in ipairs(rlist) do
         if s == r then
   			local tag, data = jack.ringbuffer_read(rbuf) 
			   print("thread received: tag=".. tag .. " data=".. data)
			end
      end
   end
end
]]

PROCESS = [[
c, rbuf = table.unpack(arg)
 
local m = 0
local tag = 1
function process(nframes)
   m = m + 1
   if m == 100 then
		-- Write some data to the ringbuffer. This will automatically write 
		-- a byte to the associated pipe, so select() will detect it and will
		-- wake up the user thread:
      jack.ringbuffer_write(rbuf, tag, "message #" .. tag )
      tag = tag + 1
      m = 0
   end
end

jack.process_callback(c, process)
]]

-- Create the client:
c = jack.client_open("select_example")

-- Create the ringbuffers:
local rbuf = jack.ringbuffer(c, 100, true, true) -- main to thread
-- Notice that the ringbuffer is created with usepipe=true so that the thread
-- can use it with socket.select().

-- Create the thread and process contexts:
thread = jack.thread_load(c, THREAD, rbuf)
jack.process_load(c, PROCESS, c, rbuf)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.activate(c)

jack.sleep(10)
print("exiting")

