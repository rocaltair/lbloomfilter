l = require "lbf"

local function printf(fmt, ...)
	print(string.format(fmt, ...))
end

local function hashfunc(i)
	return function(h)
		local v = math.pow(2, i)
		return h * v + math.floor(h / v)
	end
end

local cap = 1e7
local fakerate = 0.01
local bf, hfuncsize = l.new(cap, fakerate)

local funclist = {}
for i=1, hfuncsize do
	table.insert(funclist, hashfunc(i))
end


printf("slots=%d,fn=%d", l.slots_fn(cap, fakerate))
bf:set_hfuncs(funclist)

local v = {324,234,432,4321,342423,432,1}

table.foreach(v, function(i, v)
	bf:set(v)
	printf("set %d", v)
end)

printf("size=%d,cap=%d", bf:size(), bf:cap())

local check = {234,23,4,324,3,21,21}
table.foreach(v, function(ii, vv)
	table.insert(check, vv)
end)

table.foreach(check, function(i, v)
	printf("is %d set ? %s", v, tostring(bf:is_set(v)))
end)

bf:clear()

table.foreach(check, function(i, v)
	printf("after clear, is %d set ? %s", v, tostring(bf:is_set(v)))
end)
printf("after clear, size=%d,cap=%d", bf:size(), bf:cap())
printf("memory=%dbytes", collectgarbage("count"))


