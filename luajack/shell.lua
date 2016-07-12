-- The MIT License (MIT)
--
-- Copyright (c) 2016 Stefano Trettel
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
-- LuaJack Bash Utilities
-------------------------------------------------------------------------------

local shell = {}

function shell.exec(command)
-- Executes a shell command in the background and returns its pid
   local fp = io.popen(command .. ' & echo $!')
   assert(fp)
   local pid = fp:read('n')
   assert(pid)
   fp:close()
   return math.floor(pid)
end

function shell.term(pid) 
-- Sends a TERM signal to process
   os.execute("kill -TERM " .. pid)
end

function shell.kill(pid) 
-- Sends a KILL signal to process
   os.execute("kill -KILL " .. pid)
end

function shell.exists(pid)
-- Checks if the process exists
   if os.execute("ps -p ".. pid .." -o pid= > /dev/null") then
      return true
   end
   return false
end

return shell

