local ad = require "atomdict"

handles = {}

for i=1,30 do
	local init = {}
	init.A = math.random(50)
	init.B = math.random(50)
	init.C = math.random(50)
	init.D = 100-init.A-init.B-init.C
	handles[i] = ad.new_handle(init)
end
