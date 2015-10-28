l = require "lbf"

function printf(fmt, ...)
	print(string.format(fmt, ...))
end

function a(a)
	return a * 10240 + a / 1024 + 103
end

m = l.new(1e7, 0.01)
printf("slots=%d,fn=%d", l.slots_fn(1e5, 0.01))

m:set_hfuncs({a, a, a, a, a, a})

local v = {324,234,432,4321,342423,432,1}

table.foreach(v, function(i, v)
	m:set(v)
	printf("set %d", v)
end)

printf("size=%d,cap=%d", m:size(), m:cap())

local check = {234,23,4,324,3,21,21}
table.foreach(v, function(ii, vv)
	table.insert(check, vv)
end)

table.foreach(check, function(i, v)
	printf("is %d set ? %s", v, tostring(m:is_set(v)))
end)

