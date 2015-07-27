-- LuaJack example: JACK transport interactive monitoring.
--

jack = require("luajack")

print(jack._VERSION .. ", " .. jack._JACK_VERSION)
jack.verbose("on")

c = jack.client_open("luajack", { no_start_server=true })

jack.shutdown_callback(c, function(_, code, reason)
	print(reason .. " (" .. code .. ")")
	os.exit()
end)

-- define a few commands ...
quit = function() os.exit() end

start = function() jack.transport_start(c) end

stop = function() jack.transport_stop(c) end

state = function() print(jack.transport_state(c)) end

query = function()
   local state, position = jack.transport_query(c)
   print("state: " .. state)
   print("frame: " .. position.frame .. " (current: " .. jack.frame(c) .. ")")
   print("position fields: ")
   for k, v in pairs(position) do print(k, v) end
end

help = function()
	print([[
Available commands:
start - start JACK transport rolling
stop  - stop JACK transport rolling
state - query JACK transport state
query - query JACK transport state and position
help  - print this help
quit  - exit this program
]]) end

print([[Type 'help' for a list of available commands.]])


function extract_command(s)
	local s = string.match(s, [[%b""]])
	if s then return string.sub(s, 2, #s-1) end
end

function load_command(err)
	local s = extract_command(err)
	if type(s) == "string" then 
		return load(s .. "()") 
	else
		return nil, err
	end
end


while true do
	io.write("luajack> ")
	local f, err = load(io.read())
	if type(f) ~= "function" then 
		f, err = load_command(err)
	end
	if f then 
		pcall(f)
	else
		print(err)
	end
	jack.sleep(0)
end

