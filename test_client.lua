local ad = require "atomdict"

local d = {}

for k,v in ipairs(handles) do
	d[k] = ad.new(v)
end

local c = d[thread+1]

local function dump_object(obj)
	local t = {}
	local s = 0
	for k,v in pairs(obj) do
		table.insert(t, k .. "=" .. v)
		s = s+v
	end
	return "thread["..thread.."]"..table.concat(t," ")
end

io.write(dump_object(c) .. "\n")

local function sum_object(obj)
	local s = 0
	for _,v in pairs(obj) do
		s = s+v
	end
	if s~=100 then
		ad.dump(obj)
	end
	assert(s==100,s)
	return s
end


for i = 1, 1000 do
	c.A = math.random(80)
	c.B = math.random(80)
	c.C = math.random(80)

	c.D = 100 - c.A - c.B - c.C

	for _,v in pairs(d) do
		sum_object(v)
	end

	ad.barrier()
end

