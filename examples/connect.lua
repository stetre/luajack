-- LuaJack example: connect.lua
-- 
-- LuaJack version of JACK's connect.c example.

jack = require("luajack")
getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options] portname1 portname2" ..
"\nConnects two ports together."..
"\nOptions:" ..
"\n        -s, --server <name>  connect to the jack server named <name>" ..
"\n        -d, --disconnect     disconnect ports" ..
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


short_opts = "ds:hv"
long_opts = {
   disconnect = 'd',
   server = 's',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)

if opterr then ShowUsage(opterr) end

if optarg.h then ShowUsage() os.exit(true) end

if optarg.v then ShowVersion() os.exit(true) end

server_name = optarg.s

nargs = #arg -optind +1
if nargs < 2 then 
   ShowUsage("Not enough arguments")
end

portname1 = arg[optind]
portname2 = arg[optind+1]

c = jack.client_open(my_name, { no_start_server=true, server_name=server_name} )

-- Set the port connection callback:
jack.port_connect_callback(c, function(_, srcport, dstport, con) 
   if ((srcport == portname1) and (dstport == portname2)) 
      or ((srcport == portname2) and (dstport == portname1))
   then 
      done = true
   end
end)
   
-- Activate the client:
jack.activate(c)

-- Connect (or disconnects) the ports:
if optarg.d then
   jack.disconnect(c, portname1, portname2)
else
   jack.connect(c, portname1, portname2)
end

while not done do
   jack.sleep(0)
end

print("Done")
os.exit(true)
