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
-- LuaJack selectable ringbuffer
-------------------------------------------------------------------------------
-- Turns the read end of a LuaJack ringbuffer into an object which is
-- compatible with LuaSocket's socket.select() and eith LunaSDL's reactor.

local function create_object(jack, ref)
   local self = {}
   local ref = ref
   local jack = jack
   local fd = jack.ringbuffer_getfd(ref)
   return setmetatable(self,
      {
      __index = {

      reference = function(self) return ref end,

      getfd = function(self) return fd end,

      dirty = function(self) return jack.ringbuffer_peek(ref) end,

      settimeout = function(self, seconds) return true end,

      read = function(self) return jack.ringbuffer_read(ref) end,

      write = function(self, tag, data) return jack.ringbuffer_write(ref, tag, data) end,

      reset = function(self) return jack.ringbuffer_reset(ref) end,

      }, -- __index

      __tostring = function(self) return string.format("ringbuffer{%d}",ref) end, 

--    __gc = function(self) print("collected")  end,

      })
end

return create_object

