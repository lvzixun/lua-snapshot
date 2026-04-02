local ss = require "snapshot"

local function fmt(label, size)
    io.write(string.format("%-50s %8d bytes\n", label, size))
end

local function section(title)
    io.write(string.format("\n=== %s ===\n", title))
end

io.write(string.format("Lua Version: %s\n", _VERSION))

-- ============================================================
section("Empty Table")
-- ============================================================
fmt("empty table {}", ss.objsize({}))

-- ============================================================
section("Table with array part only")
-- ============================================================
local t1 = {1}
local t2 = {1, 2}
local t4 = {1, 2, 3, 4}
local t8 = {1, 2, 3, 4, 5, 6, 7, 8}
local t16 = {}; for i = 1, 16 do t16[i] = i end
local t32 = {}; for i = 1, 32 do t32[i] = i end
local t64 = {}; for i = 1, 64 do t64[i] = i end
local t128 = {}; for i = 1, 128 do t128[i] = i end

fmt("{1}                  (1 array slot)", ss.objsize(t1))
fmt("{1,2}                (2 array slots)", ss.objsize(t2))
fmt("{1,2,3,4}            (4 array slots)", ss.objsize(t4))
fmt("{1..8}               (8 array slots)", ss.objsize(t8))
fmt("{1..16}              (16 array slots)", ss.objsize(t16))
fmt("{1..32}              (32 array slots)", ss.objsize(t32))
fmt("{1..64}              (64 array slots)", ss.objsize(t64))
fmt("{1..128}             (128 array slots)", ss.objsize(t128))

-- ============================================================
section("Table with hash part only")
-- ============================================================
local h1 = {a=1}
local h2 = {a=1, b=2}
local h4 = {a=1, b=2, c=3, d=4}
local h8 = {a=1, b=2, c=3, d=4, e=5, f=6, g=7, h=8}
local h16 = {}; for i = 1, 16 do h16["k"..i] = i end
local h32 = {}; for i = 1, 32 do h32["k"..i] = i end

fmt("{a=1}                (1 hash entry)", ss.objsize(h1))
fmt("{a=1,b=2}            (2 hash entries)", ss.objsize(h2))
fmt("{a=1..d=4}           (4 hash entries)", ss.objsize(h4))
fmt("{a=1..h=8}           (8 hash entries)", ss.objsize(h8))
fmt("{k1=1..k16=16}       (16 hash entries)", ss.objsize(h16))
fmt("{k1=1..k32=32}       (32 hash entries)", ss.objsize(h32))

-- ============================================================
section("Table with mixed array + hash")
-- ============================================================
local m1 = {1, a=1}
local m2 = {1, 2, 3, 4, a=1, b=2, c=3, d=4}

fmt("{1, a=1}             (1 arr + 1 hash)", ss.objsize(m1))
fmt("{1..4, a..d=1..4}    (4 arr + 4 hash)", ss.objsize(m2))

-- ============================================================
section("Table recursive vs non-recursive")
-- ============================================================
local parent = {child = {grandchild = {1, 2, 3}}}
fmt("parent table         (non-recursive)", ss.objsize(parent))
fmt("parent table         (recursive)", ss.objsize(parent, true))

-- ============================================================
section("Short strings (interned, <= 40 chars)")
-- ============================================================
fmt("\"\"                   (0 chars)", ss.objsize(""))
fmt("\"a\"                  (1 char)", ss.objsize("a"))
fmt("\"hello\"              (5 chars)", ss.objsize("hello"))
fmt("\"helloworld\"         (10 chars)", ss.objsize("helloworld"))
fmt("\"12345678901234567890\" (20 chars)", ss.objsize("12345678901234567890"))
fmt("rep('x', 30)         (30 chars)", ss.objsize(string.rep("x", 30)))
fmt("rep('x', 40)         (40 chars)", ss.objsize(string.rep("x", 40)))

-- ============================================================
section("Long strings (> 40 chars)")
-- ============================================================
fmt("rep('x', 41)         (41 chars)", ss.objsize(string.rep("x", 41)))
fmt("rep('x', 50)         (50 chars)", ss.objsize(string.rep("x", 50)))
fmt("rep('x', 100)        (100 chars)", ss.objsize(string.rep("x", 100)))
fmt("rep('x', 256)        (256 chars)", ss.objsize(string.rep("x", 256)))
fmt("rep('x', 1000)       (1000 chars)", ss.objsize(string.rep("x", 1000)))
fmt("rep('x', 10000)      (10000 chars)", ss.objsize(string.rep("x", 10000)))

-- ============================================================
section("Functions")
-- ============================================================
local f0 = load("return 1")
local up1 = 1
local f1 = function() return up1 end
local up2 = 2
local f2 = function() return up1 + up2 end
local up3 = 3
local f3 = function() return up1 + up2 + up3 end

fmt("load('return 1')     (0 upvalues)", ss.objsize(f0))
fmt("closure              (1 upvalue)", ss.objsize(f1))
fmt("closure              (2 upvalues)", ss.objsize(f2))
fmt("closure              (3 upvalues)", ss.objsize(f3))
local ok, sz = pcall(ss.objsize, print)
if ok then fmt("print                (C function)", sz)
else fmt("print                (C function) [unsupported]", 0) end
ok, sz = pcall(ss.objsize, string.format)
if ok then fmt("string.format        (C function)", sz)
else fmt("string.format        (C function) [unsupported]", 0) end

-- ============================================================
section("Threads (coroutines)")
-- ============================================================
local co_empty = coroutine.create(function() end)
local co_stack = coroutine.create(function()
    local a, b, c, d, e = 1, 2, 3, 4, 5
    local t = {a, b, c, d, e}
    coroutine.yield(t)
end)
coroutine.resume(co_stack)

fmt("coroutine (fresh)", ss.objsize(co_empty))
fmt("coroutine (yielded, with locals)", ss.objsize(co_stack))

-- ============================================================
section("Snapshot entry sizes (sample)")
-- ============================================================
-- Create identifiable objects and check their sizes in snapshot
local marker_table = {}
for i = 1, 10 do marker_table[i] = i end
_G.__size_cmp_table = marker_table

local marker_str = "size_compare_test_string_marker_unique_12345678"
_G.__size_cmp_str = marker_str

local marker_func = function()
    local x = marker_table
    return x
end
_G.__size_cmp_func = marker_func

local snap = ss.snapshot()

local marker_table_addr = ss.obj2addr(marker_table)
local marker_str_addr = ss.obj2addr(marker_str)
local marker_func_addr = ss.obj2addr(marker_func)

if snap[marker_table_addr] then
    local _, sz = string.match(snap[marker_table_addr], "^([^{}]+) {(%d+)}")
    fmt("snapshot: marker table", tonumber(sz))
    fmt("objsize:  marker table", ss.objsize(marker_table))
end

if snap[marker_str_addr] then
    local _, sz = string.match(snap[marker_str_addr], "^([^{}]+) {(%d+)}")
    fmt("snapshot: marker string (48 chars)", tonumber(sz))
    fmt("objsize:  marker string (48 chars)", ss.objsize(marker_str))
end

if snap[marker_func_addr] then
    local _, sz = string.match(snap[marker_func_addr], "^([^{}]+) {(%d+)}")
    fmt("snapshot: marker function", tonumber(sz))
    fmt("objsize:  marker function", ss.objsize(marker_func))
end

-- Count total snapshot memory
local total_objects = 0
local total_size = 0
for k, v in pairs(snap) do
    total_objects = total_objects + 1
    local _, sz = string.match(v, "^([^{}]+) {(%d+)}")
    if sz then
        total_size = total_size + tonumber(sz)
    end
end
fmt("snapshot: total objects", total_objects)
fmt("snapshot: total size", total_size)

_G.__size_cmp_table = nil
_G.__size_cmp_str = nil
_G.__size_cmp_func = nil

io.write("\nDone.\n")
