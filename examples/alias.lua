-- LuaJack example: alias.lua
-- 
-- LuaJack version of JACK's alias.c example.

jack = require("luajack")

getopt = require("luajack.getopt")
fmt = string.format

my_name = arg[0]

USAGE =  
"Usage: lua " .. my_name .. " [options] portname alias" ..
"\nWith no options, set 'alias' as alias for 'portname'"..
"\nOptions:" ..
"\n        -u, --unalias        remove 'alias' as an alias for 'portname'" ..
"\n        -l, --list           list aliases for port and exit" ..
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

short_opts = "luhv"
long_opts = {
   list = 'l',
   unalias = 'u',
   help = 'h',
   version = 'v',
}

optarg, optind, opterr = getopt(arg, short_opts, long_opts)

if opterr then ShowUsage(opterr) end

if optarg.h then ShowUsage() os.exit(true) end

if optarg.v then ShowVersion() os.exit(true) end

portname = arg[optind]
alias = arg[optind+1]

nargs = #arg -optind +1

if (optarg.l and nargs < 1) or (not optarg.l and nargs < 2 ) then 
   ShowUsage("Not enough arguments")
end

c = jack.client_open(my_name, { no_start_server=true })

if not jack.nport_exists(c, portname) then
   error("No port named '".. portname .."'")
end

if optarg.l then
   alias1, alias2 = jack.nport_aliases(c, portname)
   print(fmt("Aliases for %s = %s %s", portname, alias1 or "", alias2 or ""))
   ok = true
elseif optarg.u then
   print("Unsetting "..alias.." as alias for "..portname.." ...")
   ok = pcall(jack.nport_unset_alias, c, portname, alias)
else
   print("Setting "..alias.." as alias for "..portname.." ...")
   ok = pcall(jack.nport_set_alias, c, portname, alias)
end

if not ok then print("Failed") end
os.exit(ok)
