local function test_read_write()
	free_game()
	local r = region.create(0, 0, "plain")
	local f = faction.create("enno@eressea.de", "human", "de")
	local u = unit.create(f, r)
	u.number = 2
	local fno = f.id
	local uno = u.id
	local result = 0
	assert(r.terrain=="plain")
	result = write_game("test_read_write.dat", "binary")
	assert(result==0)
	assert(get_region(0, 0)~=nil)
	assert(get_faction(fno)~=nil)
	assert(get_unit(uno)~=nil)
	r = nil
	f = nil
	u = nil
	free_game()
	assert(get_region(0, 0)==nil)
	assert(get_faction(fno)==nil)
	assert(get_unit(uno)==nil)
	result = read_game("test_read_write.dat", "binary")
	assert(result==0)
	assert(get_region(0, 0)~=nil)
	assert(get_faction(fno)~=nil)
	assert(get_unit(uno)~=nil)
	free_game()
end

local function test_gmtool()
	free_game()
	local r1 = region.create(1, 0, "plain")
	local r2 = region.create(1, 1, "plain")
	local r3 = region.create(1, 2, "plain")
	gmtool.open()
	gmtool.select(r1, true)
	gmtool.select_at(0, 1, true)
	gmtool.select(r2, true)
	gmtool.select_at(0, 2, true)
	gmtool.select(r3, false)
	gmtool.select(r3, true)
	gmtool.select_at(0, 3, false)
	gmtool.select(r3, false)
	
	local selections = 0
	for r in gmtool.get_selection() do
		selections=selections+1
	end
	assert(selections==2)
	print(gmtool.get_cursor())

	gmtool.close()
end

local function test_faction()
	free_game()
	local r = region.create(0, 0, "plain")
	local f = faction.create("enno@eressea.de", "human", "de")
	assert(f)
	f.info = "Spazz"
	assert(f.info=="Spazz")
	f:add_item("donotwant", 42)
	f:add_item("stone", 42)
	f:add_item("sword", 42)
	local items = 0
	for u in f.items do
		items = items + 1
	end
	assert(items==2)
	unit.create(f, r)
	unit.create(f, r)
	local units = 0
	for u in f.units do
		units = units + 1
	end
	assert(units==2)
end

local function test_unit()
	free_game()
	local r = region.create(0, 0, "plain")
	local f = faction.create("enno@eressea.de", "human", "de")
	local u = unit.create(f, r)
	u.number = 20
	u.name = "Enno"
	assert(u.name=="Enno")
	u.info = "Spazz"
	assert(u.info=="Spazz")
	u:add_item("sword", 4)
	assert(u:get_item("sword")==4)
	assert(u:get_pooled("sword")==4)
	u:use_pooled("sword", 2)
	assert(u:get_item("sword")==2)
end

local function test_region()
	free_game()
	local r = region.create(0, 0, "plain")
	r:set_resource("horse", 42)
	r:set_resource("money", 45)
	r:set_resource("peasant", 200)
	assert(r:get_resource("horse") == 42)
	assert(r:get_resource("money") == 45)
	assert(r:get_resource("peasant") == 200)
end

local function test_building()
	free_game()
	local u
	local f = faction.create("enno@eressea.de", "human", "de")
	local r = region.create(0, 0, "plain")
	local b = building.create(r, "castle")
	u = unit.create(f, r)
	u.number = 1
	u.building = b
	u = unit.create(f, r)
	u.number = 2
	-- u.building = b
	u = unit.create(f, r)
	u.number = 3
	u.building = b
	local units = 0
	for u in b.units do
		units = units + 1
	end
	assert(units==2)
end

local function loadscript(name)
  local script = scriptpath .. "/" .. name
  print("- loading " .. script)
  if pcall(dofile, script)==0 then
    print("Could not load " .. script)
  end
end

local function test_message()
	free_game()
	local r = region.create(0, 0, "plain")
	local f = faction.create("enno@eressea.de", "human", "de")
	local u = unit.create(f, r)
	local msg = message.create("item_create_spell")
	msg:set_unit("mage", u)
	msg:set_int("number", 1)
	msg:set_resource("item", "sword")
	msg:send_region(r)
	msg:send_faction(f)
	
	return msg
end

local function test_hashtable()
	free_game()
	local f = faction.create("enno@eressea.de", "human", "de")
	f.objects:set("enno", "smart guy")
	f.objects:set("age", 10)
	assert(f.objects:get("jesus") == nil)
	assert(f.objects:get("enno") == "smart guy")
	assert(f.objects:get("age") == 10)
	f.objects:set("age", nil)
	assert(f.objects:get("age") == nil)
end

loadscript("extensions.lua")
test_read_write()
test_region()
test_faction()
test_building()
test_unit()
test_message()
test_hashtable()
test_gmtool()
