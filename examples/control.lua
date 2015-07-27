-- LuaJack example: control.lua
-- 
-- LuaJack version of JACK's control.c example.

jack = require("luajack")

c = jack.client_open(arg[0], { no_start_server=true })

function printports(type_pattern, direction)
   local ports = jack.get_ports(c, { type_pattern=type_pattern, direction=direction } )
   for i, name in ipairs(ports) do
      print("port: " .. name .. " (" .. jack.nport_flags(c, name) .. ")")
   end
end

reorder = 0
function list_ports()
   reorder = reorder + 1
   print("Graph reorder count = " .. reorder)
	printports("audio", "input")
	printports("audio", "output")
	printports("midi", "input")
	printports("midi", "output")
end

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.graph_order_callback(c, list_ports)

jack.activate(c)

jack.sleep()
