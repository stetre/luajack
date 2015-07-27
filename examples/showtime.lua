-- LuaJack example: showtime.lua
-- 
-- LuaJack version of JACK's showtime.c example.

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options]" ..
"\nWith no options, displays jack time information at intervals of 1 second."..
"\nOptions:" ..
"\n   -i, --interval <sec>   display time at intervals of <sec> seconds" ..
"\n   -h, --help             display this help message" ..
"\n   -v, --version          display version information and exit" ..
"\n\n"

function ShowVersion()
   print(fmt("%s: %s, %s", my_name, jack._VERSION, jack._JACK_VERSION))
end

function ShowUsage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   if errmsg then os.exit() end
end


short_opts = "i:hv"
long_opts = {
   interval = 'i',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end
interval = tonumber(optarg.i) or 1

function showtime(c)
   local state, position = jack.transport_query(c)
   print("------------------------")
   print("state: " .. state)
   print("frame: " .. position.frame .. " (current: " .. jack.frame_time(c) .. ")")
   print("position fields: ")
   for k, v in pairs(position) do print(k, v) end
-- print("state:", jack.transport_state(c))
end

c = jack.client_open(my_name)

jack.shutdown_callback(c, function(_, code, reason)
   error(reason .." (".. code ..")")
end)

while true do
   jack.sleep(interval)
   showtime(c)
end
