
jack = require("luajack")

-- Create a client and two audio ports:
c = jack.client_open("mymodule", { no_start_server=true })
port_in = jack.input_audio_port(c, "in")
port_out = jack.output_audio_port(c, "out")


-- The process chunk just loads the C module implementing the RT callbacks,
-- and initializes it, passing it the Luajack objects (and possibly other
-- parameters). 
-- The C module will register its RT callbacks using the luajack.h C API.
PROCESS_CHUNK = [[
c, port_in, port_out = table.unpack(arg)
require("mymodule").init(c, port_in, port_out)
]]

jack.process_load(c, PROCESS_CHUNK, c, port_in, port_out)

jack.shutdown_callback(c, function(_,code,reason) error(reason) end)

jack.activate(c)

jack.sleep()


