ad =require "atomdict"
handle = ad.new_handle { HP = 1, name = "Hello" }
obj = ad.new(handle)
ad.dump_handle(handle)

obj.HP = 2

for k,v in pairs(obj) do
	print(k,v)
end


ad.dump_handle(handle)

ad.barrier()

obj.name = "World"

ad.dump_handle(handle)

ad.barrier()

ad.dump_handle(handle)
