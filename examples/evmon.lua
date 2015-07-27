-- LuaJack example: evmon.lua
-- 
-- LuaJack version of JACK's evmon.c example.

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =
"Usage: lua " .. my_name .. " [options]" ..
"\nWith no options, monitor JACK server events"..
"\nOptions:" ..
"\n        -h, --help           display this help message" ..
"\n        -v, --version        display version information and exit" ..
"\n\n"

function ShowVersion()
   print(fmt("%s: %s, %s", my_name, jack._VERSION, jack._JACK_VERSION))
end

function ShowUsage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   if errmsg then os.exit() end
end

short_opts = "hv"
long_opts = {
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)
if opterr then ShowUsage(opterr) end
if optarg.h then ShowUsage() os.exit(true) end
if optarg.v then ShowVersion() os.exit(true) end

-- Create the client:
c = jack.client_open(my_name, { no_start_server=true })

-- Register the callbacks:
jack.port_registration_callback(c, function (_, portname, reg)
   print(fmt("Port %s %s",portname, reg))
end)

jack.port_connect_callback(c, function (_, portname1, portname2, con)
   print(fmt("Ports %s and %s %s", portname1, portname2, con))
end)
jack.client_registration_callback(c, function (_, clientname, reg)
   print(fmt("Client %s %s", clientname, reg))
end)

jack.graph_order_callback(c, function() print("Graph reordered") end)

-- Activate the client:
jack.activate(c)

-- Endless loop:
jack.sleep()

