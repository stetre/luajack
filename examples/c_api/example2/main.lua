
RBUFSIZE = 1000

jack = require("luajack")

-- Create a client and two audio ports:
c = jack.client_open("mymodule", { no_start_server=true })
port_in = jack.input_audio_port(c, "in")
port_out = jack.output_audio_port(c, "out")
rbuf1 = jack.ringbuffer(c, RBUFSIZE, true) -- process to thread
rbuf2 = jack.ringbuffer(c, RBUFSIZE, true) -- thread to process


-- The process chunk just loads the C module implementing the RT callbacks,
-- and initializes it, passing it the Luajack objects (and possibly other
-- parameters). 
-- The C module will register its RT callbacks using the luajack.h C API.
PROCESS_CHUNK = [[
require("mymodule").init( table.unpack(arg))
]]

t = jack.thread_loadfile(c, "thread", rbuf1, rbuf2) 
jack.process_load(c, PROCESS_CHUNK, c, port_in, port_out, t, rbuf2, rbuf1)

jack.shutdown_callback(c, function(_,code,reason) error(reason) end)

jack.activate(c)

jack.sleep()


