-- LuaJack example: selectpingpong.lua
-- 
-- Shows the use of ringbuffers with select() in client threads.

jack = require("luajack")

jack.verbose("on")

THREAD = [[
selectable = require("luajack.selectable")
socket = require("socket")

rbuf_in, rbuf_out = table.unpack(arg)

-- create the 'selectable' object:
r = selectable(jack, rbuf_in)

local function receive()
   local tag, data = jack.ringbuffer_read(rbuf_in) 
   print("thread received: tag=".. tag .. " data=".. data)
   jack.ringbuffer_write(rbuf_out, tag, "pong" .. tag)
end

-- select() loop:
while true do
   local rlist, wlist, errmsg = socket.select({ r }, nil, .5)
   if errmsg then print("select:", errmsg) end
   if rlist then 
      for _, s in ipairs(rlist) do
         if s == r then receive() end
      end
   end
end
]]

PROCESS = [[
c, thread, rbuf_in, rbuf_out = table.unpack(arg)
 
local m = 0
local tag = 0
function process(nframes)
   -- check if the user thread wrote some data to rbuf_in:
   local rtag, rdata = jack.ringbuffer_read(rbuf_in)
   if rtag then
      -- we should not print in rt threads, but this is just an example:
      print("main received:   tag=".. rtag .. " data=".. rdata)
   end
   m = m + 1
   if m == 100 then
      -- write some data to rbuf_out (this will automatically write a byte
      -- to the associated pipe, so select() will detect it and will wake up
      -- the user thread):
      jack.ringbuffer_write(rbuf_out, tag, "ping" .. tag )
      tag = tag + 1
      m = 0
   end
end

jack.process_callback(c, process)
]]

-- Create the client:
c = jack.client_open("select_example")

-- Create the ringbuffers:
local rbuf1 = jack.ringbuffer(c, 8000, true) -- thread to client
local rbuf2 = jack.ringbuffer(c, 8000, true, true) -- client to thread
-- Note: rbuf2 is created with usepipe=true so that the thread can use it with 
-- socket.select().
-- rbuf1, on the other hand, is created with usepipe=nil (i.e. false) because
-- its reader is the process() callback's thread, which cannot use socket.select(),
-- being a real-time thread. Iinstead, it checks for data in the ringbuffer at the
-- beginning of the callback.

-- Create the thread and process contexts:
thread = jack.thread_load(c, THREAD, rbuf2, rbuf1)
jack.process_load(c, PROCESS, c, thread, rbuf1, rbuf2)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.activate(c)

jack.sleep(10)
print("exiting")

