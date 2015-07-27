-- LuaJack example: bufsize.lua
-- 
-- LuaJack version of JACK's bufsize.c example.

local jack = require("luajack")

local USAGE =  
"Usage: lua " .. arg[0] .. " [bufsize]" ..
"\nPrints the buffer size and the sample rate.\n" ..
"Optionally sets the buffer size (bufsize range: 1-8192).\n\n"

local function Usage(errmsg) 
   if errmsg then print(errmsg, "\n") end
   print(USAGE) 
   os.exit() 
end

if #arg > 0 then 
   bufsize = math.tointeger(arg[1])
   if not bufsize then Usage() end
   if bufsize < 1 or bufsize > 8192 then Usage("Invalid bufsize") end
end

c = jack.client_open(arg[0])

print("Buffer size = " .. jack.buffer_size(c))
print("Sample rate = " .. jack.sample_rate(c))

if bufsize then
	print("Setting buffer size to " .. bufsize)
   jack.set_buffer_size(c, bufsize)
end


