-- The MIT License (MIT)
--
-- Copyright (c) 2015 Stefano Trettel
--
-- Software repository: LuaJack, https://github.com/stetre/luajack
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

-------------------------------------------------------------------------------
-- LuaJack MIDI Utilities
-------------------------------------------------------------------------------
-- rfr. http://www.midi.org/techspecs/midimessages.php

local midi = {}

midi._VERSION = "LuaJackMidi 0.1"

local function assertf(level, condition, ...)
   if condition then return condition end
   local errmsg = next({...}) and string.format(...) or "assertf failed"
   error(errmsg,level+1)
end


local log = math.log
local floor = math.floor
local tointeger = math.tointeger

local function round(x)
	local y = floor(x)
	return x%1 > 0.5 and y+1 or y
end

-- 14 bit number to/from 7bit LSB-MSB
local function numtolsbmsb(num) -- 00mmmmmmmlllllll -> 0lllllll, 0mmmmmmm
	return tointeger(num % 128), floor(num / 128) -- lsb, msb
end

local function lsbmsbtonum(lsb, msb) -- 0lllllll, 0mmmmmmm -> 00mmmmmmmlllllll
	return tointeger(msb*128 + lsb)
end


-- number to/from Variable-Length-Quantity -------------------
local function numtovlq(num) --@@ TODO
end

local function vlqtonum(num) --@@ TODO
end


function midi.note_key(f)
-- returns the midi key corresponding to frequency f (hz)
	return round(12*log(f/440)/log(2) + 69)
end

function midi.note_frequency(n)
-- returns the frequency (in hz) corresponding to the midi key n
	return 2^((n-69)/12)*440
end


-- Timed messages: tmsg = time + msg, 
-- where time is an integer (uint32_t) and msg is a binary string
function midi.tmsg(time, msg)
	return string.pack("I4", time) .. msg -- (time,msg) -> tmsg (binary)
end

function midi.time_msg(tmsg)
	local time, pos = string.unpack("I4", tmsg)
	local msg = tmsg:sub(pos)
	return time, msg -- tmsg -> (time,msg)
end


-------------------------------------------------------------------------------
-- Message maps
-------------------------------------------------------------------------------

local type_to_name = {}	-- e.g. t[0x80] = "note off"
local name_to_type = {} -- e.g. t["note off"] = 0x80
local decodefunc = {}   -- e.g. t[0x80] = decode_kv ("note off")
local tostringfunc = {}   -- e.g. t[0x80] = tostring_kv ("note off")
local controller_to_number = {} -- e.g. t["volume (coarse)"] = 7 
local number_to_controller = {} -- e.g. t[7] = "volume (coarse)"]

local function definemessage(val, name, dec, tostr)
	type_to_name[val] = name
	name_to_type[name] = val
	decodefunc[val] = dec
	tostringfunc[val] = tostr
end

local function definecontroller(val, name)
	assertf(2, controller_to_number[name]==nil, "duplicated definition of controller %s", name)
	assertf(2, number_to_controller[val]==nil, "duplicated definition of controller %d",val)
	controller_to_number[name] = val 
	number_to_controller[val] = name
end

function midi.list()
	for k = 0,255 do 
		local m = type_to_name[k]
		if m~=nil then print(string.format("message 0x%.2x: '%s'", k, m)) end
		end
	for k = 0,127 do 
		local c = number_to_controller[k]
		if c~=nil then print(string.format("controller 0x%.2x: '%s'", k, c)) end
	end
end

function midi.list1()
	for k = 0,127 do 
		local c = number_to_controller[k]
		if c~=nil then print(string.format("| %d (0x%.2x) | '%s'", k, k, c)) end
	end
end


-------------------------------------------------------------------------------
-- Parameters decoding functions
-------------------------------------------------------------------------------
-- These functions receive a binary message and return a table containing the
-- decoded parameters, or (nil, errmsg) on error.
--
-- msg = the message, a binary string of length >=1, the first byte being the status

local function decode_notdefined(msg)
	return nil, "parameters decoder not defined for this midi message type"
end

local function tostring_notdefined(msg)
	return nil, "parameters decoder not defined for this midi message type"
end

local function decode_nodata(msg)
	return nil
end

local function tostring_nodata(msg)
	return nil
end

local function decode_kv(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local par = {}
	par.key , par.velocity = string.unpack("BB", msg, 2)
	return par
end

local function tostring_kv(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local key , velocity = string.unpack("BB", msg, 2)
	return string.format("key %u, velocity %u", key, velocity)
end

local function decode_kp(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local par = {}
	par.key , par.pressure = string.unpack("BB", msg, 2)
	return par
end

local function tostring_kp(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local key , pressure = string.unpack("BB", msg, 2)
	return string.format("key %u, pressure %u", key, pressure)
end

local function decode_cc(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local par = {}
	par.number, par.value = string.unpack("BB", msg, 2)
	par.controller = number_to_controller[par.number] 
	return par
end

local function tostring_cc(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local number, value = string.unpack("BB", msg, 2)
	local controller = number_to_controller[number] 
	return string.format("%s, %u", controller, value)
end

local function decode_n(msg)
	if msg:len() < 2 then return nil, "message too short" end
	local par = {}
	par.number = string.unpack("B", msg, 2) + 1 -- number = 1..128
	return par
end

local function tostring_n(msg)
	if msg:len() < 2 then return nil, "message too short" end
	local number = string.unpack("B", msg, 2) + 1 -- number = 1..128
	return string.format("%u", number)
end

local function decode_p(msg) 
	if msg:len() < 2 then return nil, "message too short" end
	local par = {}
	par.pressure = string.unpack("B", msg, 2)
	return par
end

local function tostring_p(msg) 
	if msg:len() < 2 then return nil, "message too short" end
	local pressure = string.unpack("B", msg, 2)
	return string.format("%u", pressure)
end

local function decode_v(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local par = {}
	par.lsb, par.msb = string.unpack("BB", msg, 2)
	par.value = lsbmsbtonum(par.lsb, par.msb)
	return par
end

local function tostring_v(msg) 
	if msg:len() < 3 then return nil, "message too short" end
	local lsb, msb = string.unpack("BB", msg, 2)
	local value = lsbmsbtonum(lsb, msb)
	return string.format("%u", value)
end

local function decode_mtcqf(msg) 
	if msg:len() < 2 then return nil, "message too short" end
	local par = {}
	local val = string.unpack("B", msg, 2) -- 0nnndddd
	par.nnn = floor(val / 16)
	par.dddd = tointeger(val % 16)
	par.value = val -- 0nnndddd
	return par
end

local mtcqf_nnn = {}
-- nnn   dddd
mtcqf_nnn[0] = "current frames (low nibble)"
mtcqf_nnn[1] = "current frames (high nibble)"
mtcqf_nnn[2] = "current seconds (low nibble)"
mtcqf_nnn[3] = "current seconds (high nibble)"
mtcqf_nnn[4] = "current minutes (low nibble)" 
mtcqf_nnn[5] = "current minutes (high nibble)"
mtcqf_nnn[6] = "current hours (low nibble)" 
mtcqf_nnn[7] =	"current Hours (high nibble) and SMPTE Type"

local function tostring_mtcqf(msg) 
	if msg:len() < 2 then return nil, "message too short" end
	local par = {}
	local val = string.unpack("B", msg, 2) -- 0nnndddd
	local nnn = floor(val / 16)
	local dddd = tointeger(val % 16)
	-- local value = val -- 0nnndddd
	return string.format("%s %u", mtcqf_nnn[nnn], dddd)
end


-- nnn   dddd
-- 0 		Current Frames Low Nibble
-- 1 		Current Frames High Nibble
-- 2 		Current Seconds Low Nibble
-- 3 		Current Seconds High Nibble
-- 4 		Current Minutes Low Nibble
-- 5 		Current Minutes High Nibble
-- 6 		Current Hours Low Nibble
-- 7 		Current Hours High Nibble and SMPTE Type



-------------------------------------------------------------------------------
-- Typedefs
-------------------------------------------------------------------------------

-- Channel Voice messages (0x80..0xef) status = 0xttttcccc, t=type, c=channel
definemessage(0x80, "note off", decode_kv, tostring_kv)
definemessage(0x90, "note on", decode_kv, tostring_kv)
definemessage(0xa0, "aftertouch", decode_kp, tostring_kp) -- "polyphonic key pressure"
definemessage(0xb0, "control change", decode_cc, tostring_cc) -- (120-127: channel mode messages)
definemessage(0xc0, "program change", decode_n, tostring_n)
definemessage(0xd0, "channel pressure", decode_p, tostring_p)
definemessage(0xe0, "pitch wheel", decode_v, tostring_v) -- "pich bend change"
-- System Common messages (0xf0..0xf7)
definemessage(0xf0, "system exclusive") -- ...
definemessage(0xf1, "mtc quarter frame", decode_mtcqf, tostring_mtcqf) -- "midi time code quarter frame"
definemessage(0xf2, "song position", decode_v, tostring_v) -- "song position pointer"
definemessage(0xf3, "song select", decode_n, tostring_n)
definemessage(0xf6, "tune request", decode_nodata, tostring_nodata)
definemessage(0xf7, "end of exclusive", decode_nodata, tostring_nodata)
-- System Real-Time messages (0xf8 ... 0xff)
definemessage(0xf8, "clock", decode_nodata, tostring_nodata) -- "timing clock"
definemessage(0xfa, "start", decode_nodata, tostring_nodata)
definemessage(0xfb, "continue", decode_nodata, tostring_nodata)
definemessage(0xfc, "stop", decode_nodata, tostring_nodata) 
definemessage(0xfe, "active sensing", decode_nodata, tostring_nodata)
definemessage(0xff, "reset", decode_nodata, tostring_nodata)


-- CONTROLLERS
definecontroller(0, "bank select (coarse)")
definecontroller(1, "modulation wheel (coarse)")
definecontroller(2, "breath controller (coarse)")
definecontroller(4, "foot pedal (coarse)")
definecontroller(5, "portamento time (coarse)")
definecontroller(6, "data entry (coarse)")
definecontroller(7, "volume (coarse)")
definecontroller(8, "balance (coarse)")
definecontroller(10, "pan position (coarse)")
definecontroller(11, "expression (coarse)")
definecontroller(12, "effect control 1 (coarse)")
definecontroller(13, "effect control 2 (coarse)")
definecontroller(16, "general purpose slider 1")
definecontroller(17, "general purpose slider 2")
definecontroller(18, "general purpose slider 3")
definecontroller(19, "general purpose slider 4")
definecontroller(32, "bank select (fine)")
definecontroller(33, "modulation wheel (fine)")
definecontroller(34, "breath controller (fine)")
definecontroller(36, "foot pedal (fine)")
definecontroller(37, "portamento time (fine)")
definecontroller(38, "data entry (fine)")
definecontroller(39, "volume (fine)")
definecontroller(40, "balance (fine)")
definecontroller(42, "pan position (fine)")
definecontroller(43, "expression (fine)")
definecontroller(44, "effect control 1 (fine)")
definecontroller(45, "effect control 2 (fine)")
definecontroller(64, "hold pedal") -- (on/off)
definecontroller(65, "portamento") -- (on/off)
definecontroller(66, "sustenuto pedal") -- (on/off)
definecontroller(67, "soft pedal") -- (on/off)
definecontroller(68, "legato pedal") -- (on/off)
definecontroller(69, "hold 2 pedal") -- (on/off)
definecontroller(70, "sound variation")
definecontroller(71, "sound timbre")
definecontroller(72, "sound release time")
definecontroller(73, "sound attack time")
definecontroller(74, "sound brightness")
definecontroller(75, "sound control 6")
definecontroller(76, "sound control 7")
definecontroller(77, "sound control 8")
definecontroller(78, "sound control 9")
definecontroller(79, "sound control 10")
definecontroller(80, "general purpose button 1") -- (on/off)
definecontroller(81, "general purpose button 2") -- (on/off)
definecontroller(82, "general purpose button 3") -- (on/off)
definecontroller(83, "general purpose button 4") -- (on/off)
definecontroller(91, "effects level")
definecontroller(92, "tremulo level")
definecontroller(93, "chorus level")
definecontroller(94, "celeste level")
definecontroller(95, "phaser level")
definecontroller(96, "data button increment")
definecontroller(97, "data button decrement")
definecontroller(98, "non-registered parameter (fine)")
definecontroller(99, "non-registered parameter (coarse)")
definecontroller(100, "registered parameter (fine)")
definecontroller(101, "registered parameter (coarse)")
definecontroller(120, "all sound off")
definecontroller(121, "all controllers off")
definecontroller(122, "local keyboard") -- (on/off)
definecontroller(123, "all notes off")
definecontroller(124, "omni mode off")
definecontroller(125, "omni mode on")
definecontroller(126, "mono operation")
definecontroller(127, "poly operation")


-------------------------------------------------------------------------------
-- Decoding 
-------------------------------------------------------------------------------

function midi.decode_status(status)
-- returns name, channel, decodefunc
	assertf(2, 0x80 <= status and status <= 0xff, "invalid midi status = %u", status)
	local t = status
	local c
	if t >= 0x80 and t <0xf0 then
		t = floor(t/16)*16
		c = status - t + 1
	end
	local name = type_to_name[t] or "undefined"
	local dec = decodefunc[t] or decode_notdefined
	local tostr = tostringfunc[t] or tostring_notdefined
	return name, c, dec, tostr
end

function midi.decode(msg, stringize)
-- given msg, a binary string of len >=1, whose first byte is the status, 
-- returns name, chan, par where:
-- name is the message name (a string)
-- chan = 1...16 or nil is the channel (nil for non-voice messages)
-- par is a table containing the decoded parameters
--
-- if stringize = true, returns parameters in a string instead of a table (for dumps)
	local status = string.unpack("B", msg)
	if status < 0x80 then 
		return nil, string.format("midi status %u is out of range", status) end
	local name, chan, dec, tostr = midi.decode_status(status)
	if stringize then return name, chan, tostr(msg) end
	return name, chan, dec(msg)
end

function midi.tohex(msg)
	local n = msg:len()
	local bytes = { string.unpack(string.rep("B", n), msg) }
	bytes[#bytes] = nil -- 'position' returned by unpack()
	for i, v in ipairs(bytes) do bytes[i] = string.format("%.2x", v) end
	return table.concat(bytes, " ")
end

-------------------------------------------------------------------------------
-- Tostring 
-------------------------------------------------------------------------------

function midi.tostring(time, msg)
-- decodes binary message and returns it in a printable string
	local m, c, par  = midi.decode(msg, true)
	if not m then return nil, c end -- nil, errmsg @@
	if c then
		return string.format("%5u: [%-8s] %s (channel %u), %s", time, midi.tohex(msg), m, c, par)
	elseif par then
		return string.format("%5u: [%-8s] %s, %s",time, midi.tohex(msg), m, par)
	else
		return string.format("%5u: [%-8s] %s",time, midi.tohex(msg), m)
	end
end
	

-------------------------------------------------------------------------------
-- Encoding 
-------------------------------------------------------------------------------

function midi.status(name, chan)
-- name = message name
-- chan = 1..16 (opt)
-- encodes name and (possibly) chan and returns the status byte
	local t = name_to_type[name]
	if t and t >= 0x80 and t < 0xf0 then
		assertf(chan > 0 and chan <= 16, "invalid midi channel = %u", chan)
		return t + chan - 1
	end
	return t
end

function midi.note_off(chan, key, velocity)
-- chan = 1..16
-- key = 0..127 
-- velocity = 0..127 or nil (default=64)
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	assertf(2, key>=0 and key<128, "invalid key %d", key)
	local velocity = velocity or 64
	assertf(2, velocity>=0 and velocity<128, "invalid velocity %d", velocity)
	return string.pack("BBB", 0x80 + chan -1, key, velocity)
end


function midi.note_on(chan, key, velocity)
-- chan = 1..16
-- key = 0..127 
-- velocity = 0..127 or nil (default=64)
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	assertf(2, key>=0 and key<128, "invalid key %d", key)
	local velocity = velocity or 64
	assertf(2, velocity>=0 and velocity<128, "invalid velocity %d", velocity)
	return string.pack("BBB", 0x90 + chan -1, key, velocity)
end


function midi.aftertouch(chan, key, pressure)
-- chan = 1..16
-- key = 0..127 
-- pressure = 0..127
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	assertf(2, key>=0 and key<128, "invalid key %d", key)
	assertf(2, pressure>=0 and pressure<128, "invalid pressure %d", pressure)
	return string.pack("BBB", 0xa0 + chan -1, key, pressure)
end


function midi.control_change(chan, control, value)
-- chan = 1..16
-- control = 0..127 or control name (a string)
-- value = 0..127 or "on"|"off" or nil (defaults to 0)
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	local ctl, val
	if type(control) == "string" then
		ctl = controller_to_number[control]
		assertf(2, ctl, "unknown controller '%s'", control)
	else
		ctl = control
		assertf(2, ctl>=0 and ctl<128, "invalid controller number %d", ctl)
	end
	if value == nil then
		val = 0
	elseif type(value) == "string" then
		if value == "on" then val = 127
		elseif value == "off" then val = 0
		else error(string.format("unkown value '%s'", value, 2))
		end
	else
		assertf(2, value >=0 and value < 128, "invalid value %d", value)
		val = value
	end
	return string.pack("BBB", 0xb0 + chan - 1, ctl, val)
end


function midi.program_change(chan, number)
-- chan = 1..16
-- number = 1..128
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	assertf(2, number > 0 and number <= 128, "invalid number %d", number)
	return string.pack("BB", 0xc0 + chan - 1, number - 1)
end


function midi.channel_pressure(chan, pressure)
-- chan = 1..16
-- pressure = 0..127
	assertf(2, chan>0 and chan<=16, "invalid channel %d", chan)
	assertf(2, pressure>=0 and pressure<128, "invalid pressure %d", pressure)
	return string.pack("BB", 0xd0 + chan - 1, pressure)
end


function midi.pitch_wheel(chan, value) -- "pitch bend change" 
-- chan = 1..16
-- value = 0..16383 (0x3fff) cents
-- Note: 'cents' are fractions of an half-step (cents = 0x2000 : wheel centered).
	assertf(2, chan > 0 and chan <= 16, "invalid channel %d", chan)
	assertf(2, value >= 0 and value <= 0x3fff, "invalid value %d", value)
	return string.pack("BBB", 0xe0 + chan -1, numtolsbmsb(value))
end


function midi.system_exclusive() --@@
-- "system exclusive" 0xf0 ...
end


function midi.mtc_quarter_frame(nnn, dddd) -- "time code quarter frame", 0xf1  0nnndddd
-- nnn = 0..7   mtc message type
-- dddd = 0..f  mtc message value
	assertf(2, nnn >= 0 and nnn <= 7, "invalid mtc quarter frame type %d", nnn)
	assertf(2, dddd >= 0 and dddd <= 0x0f, "invalid mtc quarter frame nibble value %d", dddd)
	return string.pack("BB", 0xf1, tointeger(nnn*16 + dddd))
end


function midi.song_position(value)
-- value = 0..16383 (0x3fff)
	assertf(2, value >= 0 and value <= 0x3fff, "invalid value %d", value)
	return string.pack("BBB", 0xf2, numtolsbmsb(value))
end


function midi.song_select(number)
-- number = 1..128
	assertf(2, number > 0 and number <= 128, "invalid number %d", number)
	return string.pack("BB", 0xf3, number - 1)
end


function midi.tune_request()
	return string.pack("B", 0xf6)
end

function midi.end_of_exclusive()
	return string.pack("B", 0xf7)
end

function midi.clock()
	return string.pack("B", 0xf8)
end

function midi.start()
	return string.pack("B", 0xfa)
end

function midi.continue()
	return string.pack("B", 0xfb)
end

function midi.stop()
	return string.pack("B", 0xfc)
end

function midi.active_sensing()
	return string.pack("B", 0xfe)
end

function midi.reset()
	return string.pack("B", 0xff)
end

return midi

