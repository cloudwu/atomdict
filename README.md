## Atom Dict

Atomdict implement a data structure for multi lua state.

You can use it in multi-threads , create one lua state per thread. Atomdict will helps you to exchange a group data atomic between lua states.

## Quick Example

In data server :

```Lua
  local ad = require "atomdict"
  local handle = ad.new_handle { HP = 100 , Name = "Alice" }

  -- Send handle (A integer) to other lua state in other thread
```

In data client :

```Lua
  local ad = require "atomdict"

  -- ...

  local obj = ad.new(handle) -- get data handle from other lua state before.

  -- You can use pairs in this object
  for k,v in pairs(obj) do
    print(k,v)
  end

  -- Set obj.Name in local state, others can't see it before call ad.barrier()
  obj.Name = "Bob"

  -- put barrier in your main loop of the program
  ad.barrier()
```

## API

### atomdict.new_handle( { key = value(number or string) , ... } , [typename] )

  Use a table to init atomdict . You can name the type.
  If you have many objects with the same structure , give a typename will improve the performance.

  It returns an integer handle.

### atomdict.delete_handle( handle )

  Delete a atomdict handle. You must delete it before none of the object binding to the handle.

### atomdict.new(handle)

  Create an object binding the atomdict handle.
  It returns an userdata like a table.

### atomdict.barrier()

  Call barrier for commit atomdict objects that their changes could be been by others.

### atomdict.dump(object)

  Print the object to the stdout. (for debug)

### atomdict.dump_handle(handle) 
  Print the handle to the stdout. (for debug)

## Question ?

* Send me an email : http://www.codingnow.com/2000/gmail.gif
* My Blog : http://blog.codingnow.com
* Design : http://blog.codingnow.com/2012/07/dev_note_23.html (in Chinese)