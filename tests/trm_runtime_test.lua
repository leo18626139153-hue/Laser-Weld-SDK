-- 使用虚拟时间和远端回读执行由真实DLL生成的脚本。
local function check(value, message)
    if not value then error(message, 2) end
end

local function world()
    local w = {now=0, tx={}, rx={}, ints={}, bools={}, tasks={}, events={}, writes={}, motions={}, states={}}
    local env = setmetatable({}, {__index=_G})
    function w:add(func)
        table.insert(self.tasks, {co=coroutine.create(func), at=self.now})
    end
    function w:sample()
        if self.freeze then return end
        for i=0,7 do
            local value = self.tx[200+i] or 0
            if self.ignoreEnable and i == 1 and value == 3 then value = 2 end
            self.rx[220+i] = value
        end
        self.rx[240] = self.rx[221] >= 4 and not self.missingEmission and 1 or 0
        self.rx[241] = self.deviceFault or 0
        self.rx[242] = 0
        self.ints[250] = (self.ints[250] or 0) + 1
    end
    function w:run(path, max, scenario)
        local chunk, err = loadfile(path)
        check(chunk, err or "无法读取脚本")
        setfenv(chunk, env)
        self:add(chunk)
        for now=0,max do
            self.now = now
            if now % 2 == 0 then self:sample() end
            if scenario then scenario(self) end
            for _, task in ipairs(self.tasks) do
                if not self.halted and now >= task.at and coroutine.status(task.co) ~= "dead" then
                    local ok, delay = coroutine.resume(task.co)
                    check(ok, tostring(delay))
                    task.at = now + (delay or 1)
                end
            end
            if self.halted then return end
        end
    end
    env.get_systime = function()
        return {math.floor(w.now/1000), (w.now%1000)*1000}
    end
    env.read_integer_register = function(addr)
        local value = w.ints[addr] or 0
        if addr == 250 and w.advanceOnRead then
            w.advanceOnRead = false
            w.freeze = false
            w:sample()
            w.freeze = true
            w.advancedAt = w.now
        end
        return value
    end
    env.write_integer_register = function(addr, value)
        w.ints[addr] = value
        if addr == 251 then table.insert(w.states, value) end
    end
    env.read_int16_register = function(addr) return w.rx[addr] or 0 end
    env.write_int16_register = function(addr, value)
        w.tx[addr] = value
        table.insert(w.writes, {addr=addr, value=value, at=w.now})
    end
    env.read_boolean_register = function(addr) return w.bools[addr] or false end
    env.write_boolean_register = function(addr, value) w.bools[addr] = value end
    env.wait = coroutine.yield
    env.task_create = function(f) w:add(f) return #w.tasks end
    env.sys_event_register = function(event, f) w.events[event] = f return event end
    env.halt = function() w.halted = true w.haltedAt = w.now end
    env.movel = function()
        table.insert(w.motions, {at=w.now, emission=w.rx[240]})
        env.wait(80)
    end
    return w
end

local function writes(w, address)
    local values = {}
    for _, value in ipairs(w.writes) do
        if value.addr == address then table.insert(values, value.value) end
    end
    return table.concat(values, ",")
end

local apply = world()
apply:run("trm_apply.lua", 100)
check(apply.ints[251] == 8 and #apply.motions == 0, "工艺应用没有返回确认")
check(writes(apply,201) == "" and writes(apply,200) == "1", "工艺应用夹带控制字或流程号错误")
check(writes(apply,202) == "800" and writes(apply,205) == "345", "工艺量纲错误")

local normal = world()
normal:run("trm_program.lua", 400)
check(not normal.halted and normal.ints[251] == 1 and normal.ints[800] == 1, "自动焊正常收尾")
check(writes(normal,201) == "0,2,3,7,2,0", "预送气、使能、出光及后送气顺序")
check(#normal.motions == 3 and normal.motions[1].emission == 0 and normal.motions[2].emission == 1 and normal.motions[3].emission == 1,
    "轨迹只在出光确认后进入焊接，缓降在末段运动中执行")
check(string.find(writes(normal,202), "800,400", 1, true) == 1, "SDK开光功率")
check(string.find(writes(normal,205), "345,123", 1, true) == 1, "SDK三段丝速")
local enabledAt, laseredAt, postGasAt, offAt
for _, value in ipairs(normal.writes) do
    if value.addr == 201 and value.value == 3 then enabledAt = value.at end
    if value.addr == 201 and value.value == 7 then laseredAt = value.at end
    if value.addr == 201 and value.value == 2 and laseredAt then postGasAt = value.at end
    if value.addr == 201 and value.value == 0 and postGasAt then offAt = value.at end
end
check(enabledAt < laseredAt and offAt - postGasAt >= 15, "远端使能确认及后送气延时")

local blocked = world()
blocked.ignoreEnable = true
blocked:run("trm_program.lua", 400)
check(blocked.halted and blocked.ints[251] == 6 and blocked.ints[800] == 0 and #blocked.motions == 1, "使能未确认仍开始焊接轨迹")
check(writes(blocked,201) == "0,2,3,0", "使能失败不得提交开光")

local noLight = world()
noLight.missingEmission = true
noLight:run("trm_program.lua", 400)
check(noLight.halted and noLight.ints[251] == 6 and #noLight.motions == 1 and noLight.tx[201] == 0,
    "控制字回读不等于出光，失败必须发停止")

local stale = world()
stale:run("trm_program.lua", 400, function(w)
    if #w.motions == 2 then w.freeze = true end
end)
check(stale.halted and stale.ints[251] == 6 and stale.ints[675] == 1 and stale.tx[201] == 0,
    "旧状态不能确认渐变或关光")

local paused = world()
local pauseAt, resumeAt
paused:run("trm_program.lua", 500, function(w)
    if #w.motions == 2 and not pauseAt then
        pauseAt = w.now
        w:add(w.events[2])
    end
    if pauseAt and not resumeAt and w.ints[251] == 4 and w.now >= pauseAt + 12 then
        resumeAt = w.now
        w:add(w.events[3])
    end
end)
check(pauseAt and resumeAt and not paused.halted and paused.ints[251] == 1, "暂停续焊异常")
check(writes(paused,201) == "0,2,3,7,0,2,3,7,2,0", "暂停必须先关光，明确恢复重新使能")

local held = world()
held:run("trm_manual.lua", 450, function(w)
    if w.now == 30 then w.bools[653] = true end
end)
check(held.halted and held.ints[251] == 6 and held.tx[201] == 0 and #held.motions == 0, "持续按压点焊必须在上限内关闭")
check(writes(held,201) == "0,2,3,7,0", "超时点焊禁止续焊或延时收尾")
local manualOnAt, manualOffAt
for _, value in ipairs(held.writes) do
    if value.addr == 201 and value.value == 7 then manualOnAt = value.at end
    if value.addr == 201 and value.value == 0 and manualOnAt then manualOffAt = value.at end
end
check(manualOffAt - manualOnAt == 120 and manualOffAt == held.haltedAt, "点焊上限先提交停止且同时halt")
local lastStaleWrite = stale.writes[#stale.writes]
check(lastStaleWrite.addr == 201 and lastStaleWrite.value == 0 and lastStaleWrite.at == stale.haltedAt,
    "通信失效应立即提交安全字并停止运动，不等待回执")

local gas = world()
gas:run("trm_manual.lua", 150, function(w)
    if w.now == 30 then w.bools[650] = true end
    if w.now == 60 then w.bools[650] = false end
end)
check(writes(gas,201) == "0,2,0" and gas.ints[251] == 1, "检气松手必须关闭，不夹带开光")

local betweenReads = world()
betweenReads:run("trm_manual.lua", 170, function(w)
    if w.now == 30 then w.bools[650] = true end
    if w.now == 51 then w.freeze = true end
    if w.now == 55 then w.advanceOnRead = true end
end)
check(betweenReads.advancedAt ~= nil and betweenReads.haltedAt == betweenReads.advancedAt + 51,
    "同一远端序号不能在连续轮询中延长两次检气反馈有效期")
check(writes(betweenReads,201) == "0,2,0", "检气反馈超时立即关气")

print("TRM generated Lua regressions passed")
