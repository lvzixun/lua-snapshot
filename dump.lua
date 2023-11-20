local lss = require "lsnapshot"

local sss = nil
lss.start_snapshot()

local tmp = {}
sss = {}
local function foo()
    return sss
end
local k = {}
k[1] = 1
k[2] = 2
local function test()
    sss[1] = 1
    return foo()
end

local function ff()
    k[3] = 3
end

local function dd()
    ff()
end

local function ee()
    return k[1]
end

local function bb()
    dd()
end

local function cc()
    ee()
end

local function aa(p)
    bb()
    cc()
    return p
end

local p = test()

aa(p)

lss.dstop_snapshot(40)

--[[
$ lua dump.lua
------------------ diff snapshot ------------------
[1] type:(T) addr:0x5648cc8274f0 size:0.1171875KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(T)k{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:19)ff{#dump.lua:0}->(T)k
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:27)ee{#dump.lua:0}->(T)k
[2] type:(T) addr:0x5648cc827450 size:0.0703125KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(T)p{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:14)test{#dump.lua:0}->(T)sss
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:8)foo{#dump.lua:0}->(T)sss
[3] type:(T) addr:0x5648cc8273c0 size:0.0546875KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(T)tmp{#dump.lua:0}
[4] type:(L@dump.lua:39) addr:0x5648cc827600 size:0.046875KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:39)aa{#dump.lua:0}
[5] type:(L@dump.lua:14) addr:0x5648cc827530 size:0.046875KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:14)test{#dump.lua:0}
[6] type:(L@dump.lua:19) addr:0x5648cc814f30 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:19)ff{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:23)dd{#dump.lua:0}->(L@dump.lua:19)ff
[7] type:(L@dump.lua:31) addr:0x5648cc827760 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:31)bb{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:39)aa{#dump.lua:0}->(L@dump.lua:31)bb
[8] type:(L@dump.lua:35) addr:0x5648cc8150e0 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:35)cc{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:39)aa{#dump.lua:0}->(L@dump.lua:35)cc
[9] type:(L@dump.lua:8) addr:0x5648cc827280 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:8)foo{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:14)test{#dump.lua:0}->(L@dump.lua:8)foo
[10] type:(L@dump.lua:27) addr:0x5648cc8276b0 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:27)ee{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:35)cc{#dump.lua:0}->(L@dump.lua:27)ee
[11] type:(L@dump.lua:23) addr:0x5648cc827590 size:0.0390625KB
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:23)dd{#dump.lua:0}
        Root->(T)[registry]->(L@thread ./lsnapshot.lua:266 dump.lua:49 [C])[1]->(L@dump.lua:31)bb{#dump.lua:0}->(L@dump.lua:23)dd
--------------- all size:0.1171875Kb ---------------
]]