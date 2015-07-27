-- LuaJack example: cpu_load.lua
-- 
-- LuaJack version of JACK's cpu_load.c example.

jack = require("luajack")

c = jack.client_open(arg[0], { no_start_server=true })

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

jack.activate(c)

while true do
   cpu = jack.cpu_load(c)
   print("jack cpu load: ".. (cpu - cpu%0.01) .. " %")
   jack.sleep(1)
end
