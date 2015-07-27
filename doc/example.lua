
jack = require("luajack")

-- Open a JACK client:
c = jack.client_open("myclient")

-- Create two ports:
p_in = jack.input_audio_port("port_in")
p_out = jack.output_audio_port("port_out")

-- Load the process chunk:
jack.process_load(c, [[
c, p_in, p_out = table.unpack(arg)

function process(nframes)
   -- Audio processing happens here (in this example, we just
   -- copy the samples from the input port to the output port).
   jack.get_buffer(p_in)
   jack.get_buffer(p_out)
   jack.copy(p_out, p_in) 
end

-- Register the (rt) process callback:
jack.process_callback(c, process)
]], c, p_in, p_out)

-- Register a non-rt callback:
jack.shutdown_callback(c, function() error("shutdown from server") end)

-- Activate the client:
jack.activate(c)

-- Sleep, waiting for JACK to call back: 
jack.sleep()
