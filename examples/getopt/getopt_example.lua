
local getopt = require("luajack.getopt")

local short_opts = "hVvo:n:S:"
local long_opts = {
   verbose = "v",
   help = "h",
   output = "o",
   set_value = "S",
   ["set-output"] = "o"
}

local progname = arg[0]
local USAGE = 
"Usage: lua " .. progname .. " [options] ARG1 [ ARG2 ... ]" ..
"\n\nOptions:" ..
"\n  -h, --help" ..
"\n  -v, --help" ..
"\n  -o FILE, --output=FILE" ..
"\n  -S VALUE, --set_value=VALUE"..
"\n\n"

local function Usage() print(USAGE) os.exit(true) end

local optarg, optind, opterr = getopt(arg, short_opts, long_opts)

if opterr then print(opterr,"\n") Usage() end

if optarg.h then  Usage() end

if #arg < optind then print("Not enough arguments\n") Usage() end

-- options:
print("Opt:", "Value:")
for k,v in pairs(optarg) do print(k,v) end
print("")

-- arguments that follow:
print("Arg:", "value:")
for i=optind, #arg do print(i, arg[i]) end
print("")

local nargs = #arg - optind + 1
print("nargs = " .. nargs)

