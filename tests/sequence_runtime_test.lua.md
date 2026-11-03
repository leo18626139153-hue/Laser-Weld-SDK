-- 用已安装的 Lua 原样执行 SDK 生成代码；所有设备、时钟与任务均为虚拟实现。
io.stdout:setvbuf("no")
local fixture_root = assert(arg[1], "缺少生成目录")
local fixtures = assert(dofile(fixture_root .. "/generated_sequence_fixtures.lua.md"))
local checks, groups = 0, 0

local function check(condition, message)
    checks = checks + 1
    if not condition then error(message or "断言失败", 2) end
end

local function equal(actual, expected, message)
    check(actual == expected, (message or "值不相等") .. ": actual=" .. tostring(actual)
        .. " expected=" .. tostring(expected))
end

local function pass_group(fixture, name)
    groups = groups + 1
    print("PASS Lua " .. fixture.name .. ": " .. name)
end

local function create_world(fixture, quantum, red_light_state)
    local world = { now = 0, events = {}, tasks = {}, next_id = 0, quantum = quantum or 1,
                    registers = {}, chars = {}, outputs = {[0] = red_light_state == nil and true or red_light_state},
                    handlers = {}, position = {0,0,0,0,0,0} }
    local env = {}
    for key, value in pairs(_G) do env[key] = value end
    env._G = env
    world.env = env
    local function event(kind, address, value)
        local manual_gas_address = fixture.legacy and 49 or (fixture.powerAddress == 63 and 94 or 103)
        check(not (kind == "write" and address == manual_gas_address),
              fixture.name .. " 新外控阶段不应独立写手动气阀开关")
        if fixture.powerAddress == 90 and kind == "write" then
            check(address ~= 69 and not (address >= 98 and address <= 101),
                  fixture.name .. " 工艺/起收弧不得重复写入上位机固定设置")
        end
        world.events[#world.events + 1] = { time = world.now, kind = kind,
                                         address = address, value = value,
                                         active_wire_stop = not (fixture.powerAddress == 90
                                             and kind == "write" and address == 95 and value == 0)
                                             or world.wire_on == true }
        if (kind == "do" and address == 2) or
           (kind == "write" and fixture.legacy and address == 50) or
           (kind == "write" and fixture.powerAddress == 90 and address == 95) then
            world.wire_on = value == true or value == 1
        end
    end
    local function wait_ms(milliseconds)
        local duration = math.max(1, tonumber(milliseconds) or 1)
        duration = math.ceil(duration / world.quantum) * world.quantum
        if coroutine.running() then return coroutine.yield(duration) end
        return world:advance(duration)
    end
    function world:add_task(fn, name)
        self.next_id = self.next_id + 1
        local item = { id = self.next_id, co = coroutine.create(fn), wake = self.now,
                       alive = true, name = name or "后台任务" }
        self.tasks[item.id] = item
        return item.id
    end
    function world:step(target)
        local chosen
        for _, item in pairs(self.tasks) do
            if item.alive and item.wake <= target and
                    (not chosen or item.wake < chosen.wake or
                     (item.wake == chosen.wake and item.id < chosen.id)) then chosen = item end
        end
        if not chosen then return false end
        self.now = math.max(self.now, chosen.wake)
        local ok, duration = coroutine.resume(chosen.co)
        check(ok, fixture.name .. " " .. chosen.name .. ": " .. tostring(duration))
        if coroutine.status(chosen.co) == "dead" then chosen.alive = false
        elseif chosen.alive then chosen.wake = self.now + (tonumber(duration) or 1) end
        return true
    end
    function world:advance(duration)
        local target = self.now + duration
        local count = 0
        while self:step(target) do
            count = count + 1
            check(count < 100000, fixture.name .. " 调度器未收敛")
        end
        self.now = math.max(self.now, target)
    end
    function world:invoke(fn, ...)
        local args = {...}
        local returns
        local id = self:add_task(function() returns = {fn(unpack(args))} end, "主调用")
        local limit = self.now + 120000
        local count = 0
        while self.tasks[id].alive do
            check(self:step(limit), fixture.name .. " 主调用被无限阻塞")
            count = count + 1
            check(count < 100000, fixture.name .. " 主调用未收敛")
        end
        return unpack(returns or {})
    end
    function world:load(source, name)
        local chunk, message = loadstring(source, name or fixture.name)
        check(chunk ~= nil, tostring(message))
        setfenv(chunk, env)
        return self:invoke(chunk)
    end
    function world:writes(address, since)
        local result = {}
        for _, item in ipairs(self.events) do
            if item.kind == "write" and item.address == address and
                    (not since or item.time >= since) then result[#result + 1] = item end
        end
        return result
    end
    function world:offs()
        local result = {}
        for _, item in ipairs(self.events) do
            if (item.kind == "do" and item.address == 2 and item.value == false) or
               (item.kind == "write" and fixture.legacy and item.address == 50 and item.value == 0) or
               (item.kind == "write" and not fixture.legacy and fixture.powerAddress == 90
                    and item.address == 95 and item.value == 0 and item.active_wire_stop) then
                result[#result + 1] = item
            end
        end
        return result
    end
    env.print = function() end
    env.wait = wait_ms
    env.get_systime = function()
        return {math.floor(world.now / 1000), math.floor((world.now % 1000) * 1000)}
    end
    env.task_create = function(fn)
        world.background_created = (world.background_created or 0) + 1
        return world:add_task(fn)
    end
    env.task_delete = function(id)
        if world.tasks[id] and not world.defer_delete then world.tasks[id].alive = false end
        event("delete", id, true)
    end
    env.task_kill = env.task_delete
    env.sys_event_register = function(id, handler)
        world.handlers[id] = handler
        return id
    end
    env.write_integer_register = function(address, value)
        world.registers[address] = value
        event("integer", address, value)
    end
    env.read_integer_register = function(address)
        event("read_integer", address, world.registers[address] or 0)
        return world.registers[address] or 0
    end
    env.write_int16_register = function(address, value)
        world.registers[address] = value
        event("write", address, value)
    end
    env.read_int16_register = function(address)
        event("read_int16", address, world.registers[address] or 0)
        return world.registers[address] or 0
    end
    env.write_boolean_register = function(address, value)
        world.registers[address] = value
        event("boolean", address, value)
    end
    env.write_bool_register = env.write_boolean_register
    env.read_boolean_register = function(address) return world.registers[address] or false end
    env.read_bool_register = env.read_boolean_register
    env.write_char_register = function(address, value)
        check(world.now >= (world.serial_busy_until or 0), fixture.name .. " 串口未完成就覆盖共享发送缓冲区")
        world.chars[address] = value
    end
    env.read_char_register = function(address) return world.chars[address] or 0 end
    env.set_configurable_digital_out = function(address, value)
        check(address ~= 0, fixture.name .. " CO0是红光开关，正式焊接阶段不得操作")
        world.outputs[address] = value
        event("do", address, value)
    end
    env.get_configurable_digital_out = function(address) return world.outputs[address] or false end
    env.serial_send_receive = function(port, size, send_type, send_address, recv_type, recv_address, receipt)
        check(world.now >= (world.serial_busy_until or 0), fixture.name .. " 连续发送串口帧未等待稳定间隔")
        world.serial_busy_until = world.now + fixture.settle
        local bytes = {}
        for i = 1, size do bytes[i] = world.chars[send_address + i - 1] or 0 end
        check(port == 2 or port == 3, fixture.name .. " 串口端口错误")
        equal(bytes[1], port == 2 and 10 or 9, fixture.name .. " 串口站号错误")
        local address = (bytes[3] or 0) * 256 + (bytes[4] or 0)
        if bytes[2] == 6 then
            event("write", address, (bytes[5] or 0) * 256 + (bytes[6] or 0))
        elseif bytes[2] == 16 or bytes[2] == 9 then
            local count = (bytes[5] or 0) * 256 + (bytes[6] or 0)
            for i = 1, count do
                event("write", address + i - 1, (bytes[6+i*2] or 0) * 256 + (bytes[7+i*2] or 0))
            end
        end
        if receipt ~= 0 and not world.drop_ack then
            local function acknowledge() world.registers[recv_address] = 1 end
            if world.ack_delay and world.ack_delay > 0 then
                world:add_task(function() wait_ms(world.ack_delay) acknowledge() end, "虚拟设备回执")
            else acknowledge() end
        end
        return 1
    end
    env.movel = function(pose, speed)
        event("move_start", pose[1], speed)
        local dx, dy, dz = pose[1]-world.position[1], pose[2]-world.position[2], pose[3]-world.position[3]
        local duration = math.sqrt(dx*dx+dy*dy+dz*dz) / speed * 1000
        if duration > 0 then wait_ms(duration) end
        world.position = pose
        event("move_end", pose[1], speed)
    end
    env.movej = env.movel
    env.pause = function() event("pause", 0, 1) end
    env.stop = function() event("stop", 0, 1) end
    return world
end

local function last_write(world, address)
    local writes = world:writes(address)
    check(#writes > 0, "没有输出寄存器 " .. tostring(address))
    return writes[#writes].value
end

local function is_wire_on(fixture, item)
    return (item.kind == "do" and item.address == 2 and item.value == true)
        or (fixture.legacy and item.kind == "write" and item.address == 50 and item.value == 1)
        or (fixture.powerAddress == 90 and item.kind == "write" and item.address == 95 and item.value == 1)
end

local function fresh(fixture, quantum)
    local world = create_world(fixture, quantum)
    world:load(fixture.runtime, fixture.name .. " runtime")
    check(type(world.env.cl_ramp_start) == "function", "缺少起弧入口")
    check(type(world.env.cl_ramp_down) == "function", "缺少缓降入口")
    check(type(world.env.cl_ramp_finish) == "function", "缺少正常终点入口")
    check(type(world.env.cl_ramp_abort) == "function", "缺少异常取消入口")
    return world
end

local function check_no_wire(fixture, world)
    if fixture.wire then return end
    for _, item in ipairs(world.events) do
        check(not (item.kind == "do" and item.address == 2), fixture.name .. " 自熔焊操作 CO2")
        check(not (item.kind == "write" and
              (item.address == fixture.wireAddress
               or (fixture.legacy and (item.address == 50 or item.address == 260 or item.address == 261))
               or (fixture.powerAddress == 90 and (item.address == 95 or item.address == 96 or item.address == 97))
               or (fixture.powerAddress == 63 and (item.address == 75 or item.address == 76 or item.address == 77)))),
              fixture.name .. " 自熔焊操作送丝/回抽")
        check(not ((item.kind == "read_int16" or item.kind == "read_integer") and
               (item.address == fixture.wireAddress or item.address == 260
                or item.address == 261 or item.address == 277)), fixture.name .. " 自熔焊读取送丝参数")
    end
end

-- 预期直接由独立墙钟公式计算，不能拿运行时自身状态作为正确性的依据。
local function check_linear(fixture, world, started, duration, p0, p1, w0, w1)
    local powers = world:writes(fixture.powerAddress, started)
    local wires = fixture.wire and world:writes(fixture.wireAddress, started) or {}
    check(#powers >= 2, fixture.name .. " 没有实际执行过渡采样")
    if fixture.wire then equal(#wires, #powers, fixture.name .. " 功率送丝目标未成对输出") end
    for index, item in ipairs(powers) do
        local ratio = math.min(1, math.max(0, (item.time - started) / duration))
        equal(item.value, math.floor(p0 + (p1-p0)*ratio + 0.5), fixture.name .. " 功率不符合墙钟线性公式")
        if fixture.wire then
            equal(wires[index].value, math.floor(w0 + (w1-w0)*ratio + 0.5), fixture.name .. " 送丝不符合相同阶段线性公式")
            check(wires[index].time >= item.time, fixture.name .. " 送丝目标早于同组功率目标")
            check(wires[index].time-item.time <= math.max(fixture.settle, world.quantum),
                  fixture.name .. " 同组功率与送丝发送时间差超出帧间隔及模拟调度抖动")
        end
    end
end

local function run_runtime_cases(fixture)
    local start_wire = fixture.wire and 30 * fixture.wireMultiplier or nil
    local weld_wire = fixture.wire and 60 * fixture.wireMultiplier or nil
    local end_wire = fixture.wire and 20 * fixture.wireMultiplier or nil
    -- 送气由设备自动执行，参数非零不能延后 SDK 阶段起点；触发线按品牌契约单独核对。
    local automatic_gas, no_gas_delay = fresh(fixture), fresh(fixture)
    automatic_gas:invoke(automatic_gas.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 1000, 1000)
    no_gas_delay:invoke(no_gas_delay.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 1000, 0)
    equal(automatic_gas.now, no_gas_delay.now, fixture.name .. " SDK 等待了设备预气参数")
    automatic_gas:advance(1500)
    no_gas_delay:advance(1500)
    local before_gas_finish, before_plain_finish = automatic_gas.now, no_gas_delay.now
    automatic_gas:invoke(automatic_gas.env.cl_ramp_finish, 500, end_wire, 1000)
    no_gas_delay:invoke(no_gas_delay.env.cl_ramp_finish, 500, end_wire, 0)
    equal(automatic_gas.now-before_gas_finish, no_gas_delay.now-before_plain_finish,
          fixture.name .. " SDK 等待了设备后气参数")
    check_no_wire(fixture, automatic_gas)
    check_no_wire(fixture, no_gas_delay)
    pass_group(fixture, "device gas parameters add no SDK timing delay or manual gas-valve writes")

    local world = fresh(fixture)
    world:invoke(world.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 1000, 100)
    local ramp_started = world.now
    world:advance(1500)
    check_linear(fixture, world, ramp_started, 1000, 1000, 2000, start_wire, weld_wire)
    equal(last_write(world, fixture.powerAddress), 2000, fixture.name .. " 起弧精确终值")
    if fixture.wire then equal(last_write(world, fixture.wireAddress), weld_wire, fixture.name .. " 焊接速度终值") end
    world:invoke(world.env.cl_ramp_down, 2000, 500, weld_wire, end_wire, 1000)
    local before_down = world.now
    world:advance(1400)
    check_linear(fixture, world, before_down, 1000, 2000, 500, weld_wire, end_wire)
    equal(last_write(world, fixture.powerAddress), 500, fixture.name .. " 缓降精确终值")
    if fixture.wire then
        equal(last_write(world, fixture.wireAddress), end_wire, fixture.name .. " 收弧速度终值")
        equal(#world:offs(), 0, fixture.name .. " 终点前不能停丝触发回抽")
    end
    world:invoke(world.env.cl_ramp_finish, 500, end_wire, 200)
    equal(world.outputs[1], false, fixture.name .. " 正常终点关光")
    if fixture.wire then equal(#world:offs(), 1, fixture.name .. " 正常终点只停丝一次") end
    local first_finish_power, first_finish_wire, first_finish_offs =
        #world:writes(fixture.powerAddress), #world:writes(fixture.wireAddress), #world:offs()
    world:invoke(world.env.cl_ramp_finish, 500, end_wire, 200)
    equal(#world:writes(fixture.powerAddress), first_finish_power, fixture.name .. " 重复终点重复发送功率")
    equal(#world:writes(fixture.wireAddress), first_finish_wire, fixture.name .. " 重复终点重复发送速度")
    equal(#world:offs(), first_finish_offs, fixture.name .. " 重复终点重复停丝")
    local finished_count = #world.events
    world:advance(3000)
    equal(#world.events, finished_count, fixture.name .. " 终点之后不得有残留任务写入")
    check_no_wire(fixture, world)
    pass_group(fixture, "wall-clock linear ramp, exact targets, one stop, repeated finish")

    -- 零时间、短于周期、相等及负差均执行真实生成函数，不另写一份业务算法。
    for _, duration in ipairs({0, 1, 50, 100}) do
        local short = fresh(fixture)
        short:invoke(short.env.cl_ramp_start, 1600, 800, weld_wire, start_wire, duration, 0)
        local short_started = short.now
        short:advance(1000)
        equal(last_write(short, fixture.powerAddress), 800, fixture.name .. " 短时间下降终值")
        if not fixture.legacy then
            local writes = short:writes(fixture.powerAddress)
            check(writes[#writes].time <= short_started + duration,
                  fixture.name .. " 短坡度被初值稳定等待拉长")
        end
        if fixture.wire then equal(last_write(short, fixture.wireAddress), start_wire, fixture.name .. " 负差送丝终值") end
        short:invoke(short.env.cl_ramp_down, 800, 800, start_wire, start_wire, duration)
        short:advance(1000)
        equal(last_write(short, fixture.powerAddress), 800, fixture.name .. " 相同目标终值")
        short:invoke(short.env.cl_ramp_finish, 800, start_wire, 0)
        check_no_wire(fixture, short)
        pass_group(fixture, "duration " .. tostring(duration) .. ", negative and equal deltas")
    end

    -- 故意把任务唤醒拖慢到周期的两倍以上，确认写当前时刻而非补发历史队列。
    local slow = fresh(fixture, 250)
    slow:invoke(slow.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 1000, 0)
    local slow_started = slow.now
    slow:advance(2500)
    check_linear(fixture, slow, slow_started, 1000, 1000, 2000, start_wire, weld_wire)
    equal(last_write(slow, fixture.powerAddress), 2000, fixture.name .. " 慢周期精确终值")
    check(#slow:writes(fixture.powerAddress) <= 8, fixture.name .. " 慢周期积压补发历史功率")
    if fixture.wire then check(#slow:writes(fixture.wireAddress) <= 8, fixture.name .. " 慢周期积压补发历史送丝") end
    slow:invoke(slow.env.cl_ramp_finish, 2000, weld_wire, 0)
    check_no_wire(fixture, slow)
    pass_group(fixture, "slow scheduling skips expired samples")

    -- 收弧抢占仍在进行的起弧，终点再取消未完成缓降。
    local interrupted = fresh(fixture)
    interrupted:invoke(interrupted.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 5000, 0)
    interrupted:advance(200)
    interrupted:invoke(interrupted.env.cl_ramp_down, 1300, 500, start_wire, end_wire, 5000)
    interrupted:advance(200)
    interrupted.defer_delete = true
    interrupted:invoke(interrupted.env.cl_ramp_finish, 500, end_wire, 0)
    local ended = #interrupted.events
    interrupted:advance(10000)
    equal(#interrupted.events, ended, fixture.name .. " 抢占后旧任务残留写入")
    if not fixture.legacy then
        equal(last_write(interrupted, fixture.powerAddress), 500, fixture.name .. " 映射抢占终值")
    end
    if fixture.wire then equal(#interrupted:offs(), 1, fixture.name .. " 抢占正常停丝次数") end
    check_no_wire(fixture, interrupted)
    pass_group(fixture, "down preempts up; endpoint cancels outstanding task")

    -- 异常取消不写正常收弧终值，也不允许后续旧任务恢复输出。
    local aborted = fresh(fixture)
    aborted:invoke(aborted.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 5000, 0)
    aborted:advance(200)
    aborted.defer_delete = true
    aborted:invoke(aborted.env.cl_ramp_abort)
    equal(aborted.outputs[1], false, fixture.name .. " 异常取消立即关光")
    local abort_count = #aborted.events
    aborted:advance(10000)
    equal(#aborted.events, abort_count, fixture.name .. " 异常取消后残留写入")
    check_no_wire(fixture, aborted)
    pass_group(fixture, "abort cancels task and stops outputs")

    -- 起弧主调用仍在等待时被取消，旧调用恢复后不能重新开光或恢复送料。
    local pending = fresh(fixture)
    pending:add_task(function()
        pending.env.cl_ramp_start(1000, 2000, start_wire, weld_wire, 5000, 1000)
    end, "待抢占起弧")
    pending:advance(25)
    pending:invoke(pending.env.cl_ramp_abort)
    equal(#pending:offs(), 0, fixture.name .. " 初值准备取消触发了未开启送丝的回抽")
    local cancelled = #pending.events
    pending:advance(10000)
    equal(#pending.events, cancelled, fixture.name .. " 起弧等待被取消后恢复输出")
    equal(pending.outputs[1], false, fixture.name .. " 起弧抢占后仍处于开光状态")
    check_no_wire(fixture, pending)
    pass_group(fixture, "abort while start is still waiting")

    local restarted = fresh(fixture)
    restarted:invoke(restarted.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 5000, 0)
    restarted:advance(25)
    restarted.defer_delete = true
    restarted:invoke(restarted.env.cl_ramp_start, 1600, 1200, weld_wire, start_wire, 500, 200)
    restarted:advance(1500)
    equal(last_write(restarted, fixture.powerAddress), 1200, fixture.name .. " 新起弧被旧周期覆盖")
    if fixture.wire then equal(last_write(restarted, fixture.wireAddress), start_wire, fixture.name .. " 新起弧速度被旧周期覆盖") end
    restarted:invoke(restarted.env.cl_ramp_finish, 1200, start_wire, 0)
    if fixture.wire then equal(#restarted:offs(), 2, fixture.name .. " 起弧抢占必须分别关闭旧周期与新周期") end
    local restart_finished = #restarted.events
    restarted:advance(10000)
    equal(#restarted.events, restart_finished, fixture.name .. " 新起弧结束后旧周期继续输出")
    check_no_wire(fixture, restarted)
    pass_group(fixture, "new start preempts active cycle with delayed task deletion")

    -- 模拟控制器任务长期未获调度，超出阶段时间及容差后必须异常退出。
    local timed_out = fresh(fixture)
    timed_out:invoke(timed_out.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 1000, 0)
    timed_out:advance(0)
    timed_out.quantum = 10000
    timed_out:advance(15000)
    equal(timed_out.outputs[1], false, fixture.name .. " 超时没有关闭激光")
    equal(timed_out.registers[675], 1, fixture.name .. " 超时没有记录收弧异常")
    local timeout_events = #timed_out.events
    timed_out:advance(20000)
    equal(#timed_out.events, timeout_events, fixture.name .. " 超时退出后旧任务继续写入")
    check_no_wire(fixture, timed_out)
    pass_group(fixture, "late worker timeout abort")

    if fixture.legacy then
        -- 真实准备函数等待虚拟设备回执；丢回执必须超时关光，且取消后不能复活。
        local no_ack = fresh(fixture)
        no_ack.drop_ack = true
        no_ack:load(fixture.publicStart, fixture.name .. " public preparation without acknowledgement")
        check(no_ack.outputs[1] ~= true, fixture.name .. " 准备回执超时后仍开光")
        equal(no_ack.registers[673], 1, fixture.name .. " 准备回执超时未记录起弧异常")
        local failed = #no_ack.events
        no_ack:advance(10000)
        equal(#no_ack.events, failed, fixture.name .. " 回执超时后继续准备或开光")
        check_no_wire(fixture, no_ack)
        pass_group(fixture, "serial preparation acknowledgement timeout aborts")

        local cancelled_prep = fresh(fixture)
        cancelled_prep.drop_ack = true
        cancelled_prep.defer_delete = true
        local pending_source, pending_error = loadstring(fixture.publicStart, fixture.name .. " cancelled public preparation")
        check(pending_source ~= nil, tostring(pending_error))
        setfenv(pending_source, cancelled_prep.env)
        cancelled_prep:add_task(pending_source, "等待回执的起弧准备")
        cancelled_prep:advance(100)
        cancelled_prep:invoke(cancelled_prep.env.cl_ramp_abort)
        local cancelled_count = #cancelled_prep.events
        cancelled_prep:advance(10000)
        equal(#cancelled_prep.events, cancelled_count, fixture.name .. " 已取消的回执准备重新写入")
        check(cancelled_prep.outputs[1] ~= true, fixture.name .. " 已取消的回执准备重新开光")
        check_no_wire(fixture, cancelled_prep)
        pass_group(fixture, "cancelled serial preparation cannot resume")

        if fixture.wire then
            local reentrant = fresh(fixture)
            reentrant:invoke(reentrant.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 5000, 0)
            reentrant:advance(25)
            reentrant:add_task(function() reentrant.env.cl_ramp_finish(500, end_wire, 0) end,
                               "在途串口帧后排队停丝")
            reentrant:advance(1)
            reentrant:add_task(function()
                reentrant.env.cl_ramp_start(1000, 2000, start_wire, weld_wire, 1000, 1000)
            end, "抢占尚未提交的旧停丝")
            reentrant:advance(2750)
            local on_events = {}
            for _, item in ipairs(reentrant.events) do
                if is_wire_on(fixture, item) then on_events[#on_events+1] = item end
            end
            local off_events = reentrant:offs()
            equal(#on_events, 2, fixture.name .. " 新周期没有正常重新开丝")
            check(#off_events >= 1 and off_events[1].time <= on_events[2].time,
                  fixture.name .. " 旧周期停丝未成功提交就开始新周期")
            reentrant:invoke(reentrant.env.cl_ramp_finish, 500, end_wire, 0)
            equal(#reentrant:offs(), 2, fixture.name .. " 新周期未接管尚未完成的旧停丝")
            pass_group(fixture, "new cycle takes over pending serial wire-off before restarting wire")

            local duplicate_stop = fresh(fixture)
            duplicate_stop:invoke(duplicate_stop.env.cl_ramp_start, 1000, 2000, start_wire, weld_wire, 5000, 0)
            duplicate_stop:advance(25)
            duplicate_stop:add_task(function() duplicate_stop.env.cl_ramp_finish(500, end_wire, 0) end,
                                    "等待总线的旧终点")
            duplicate_stop:advance(1)
            local stop_time = duplicate_stop.now
            duplicate_stop:load(fixture.publicFinish, fixture.name .. " reload pending finish")
            duplicate_stop:advance(10000)
            equal(duplicate_stop.outputs[1], false, fixture.name .. " 重复终点重新开光")
            equal(#duplicate_stop:offs(), 1, fixture.name .. " 重复终点未接管或重复提交旧停丝")
            for _, item in ipairs(duplicate_stop:writes(fixture.powerAddress)) do
                check(item.time <= stop_time, fixture.name .. " 重复终点恢复功率输出")
            end
            pass_group(fixture, "reloaded finish takes over pending wire-off without residual power")
        end
    end
end

local function run_program_case(fixture)
    local world = create_world(fixture)
    world:load(fixture.program, fixture.name .. " full program")
    local endpoint
    for _, item in ipairs(world.events) do
        if item.kind == "move_end" and item.address == 10 then endpoint = item.time end
    end
    check(endpoint ~= nil, fixture.name .. " 完整程序没有执行焊接终点")
    if not fixture.legacy then
        equal(last_write(world, fixture.powerAddress), 500, fixture.name .. " 完整程序最终功率")
    else
        local final_power = last_write(world, fixture.powerAddress)
        check(final_power >= 500 and final_power <= 2000, fixture.name .. " 串口终点功率越界")
        for _, item in ipairs(world:writes(fixture.powerAddress)) do
            check(item.time <= endpoint, fixture.name .. " 串口终点关光后补发了功率")
        end
    end
    equal(world.outputs[1], false, fixture.name .. " 完整程序终点关光")
    if fixture.wire then
        if not fixture.legacy then
            equal(last_write(world, fixture.wireAddress), 20 * fixture.wireMultiplier, fixture.name .. " 完整程序收弧速度单位")
        else
            for _, item in ipairs(world:writes(fixture.wireAddress)) do
                check(item.time <= endpoint, fixture.name .. " 串口终点关光后补发了速度")
            end
        end
        local saw_start, saw_weld = false, false
        for _, item in ipairs(world:writes(fixture.wireAddress)) do
            if item.value == 30 * fixture.wireMultiplier then saw_start = true end
            if item.value == 60 * fixture.wireMultiplier then saw_weld = true end
        end
        check(saw_start and saw_weld, fixture.name .. " 完整程序三档送丝未按同一单位编码")
        local offs = world:offs()
        equal(#offs, 1, fixture.name .. " 完整程序正常停丝次数")
        check(offs[1].time >= endpoint, fixture.name .. " 轨迹终点之前已停丝或触发回抽")
        if fixture.powerAddress == 90 then
            local prepared, started = false, false
            for _, item in ipairs(world.events) do
                if item.kind == "write" and item.address == 95 then
                    if item.value == 0 and not started then prepared = true end
                    if item.value == 1 then
                        check(prepared, fixture.name .. " 首站准备未停丝就开启送丝")
                        started = true
                    end
                end
            end
            check(prepared and started, fixture.name .. " 缺少明确停丝准备或起弧送丝")
        end
    end
    local rated = fixture.legacy and 560 or (fixture.powerAddress == 63 and 60 or 131)
    equal(#world:writes(rated), 0, fixture.name .. " 外控程序写了设备额定功率")
    local finished = #world.events
    world:advance(10000)
    equal(#world.events, finished, fixture.name .. " 完整程序结束后仍有任务输出")
    check_no_wire(fixture, world)
    local trace = assert(io.open(fixture_root .. "/sequence-" .. fixture.name .. "-events.tsv.md", "w"))
    trace:write("time_ms\tkind\taddress\tvalue\n")
    for _, item in ipairs(world.events) do
        trace:write(tostring(item.time), "\t", item.kind, "\t", tostring(item.address),
                    "\t", tostring(item.value), "\n")
    end
    trace:close()
    pass_group(fixture, "public full program keeps wire until motion endpoint")

    local parameters = create_world(fixture)
    parameters:load(fixture.runtime, fixture.name .. " parameter helpers")
    parameters:load(fixture.parameters, fixture.name .. " actual parameters")
    if fixture.wire then
        equal(last_write(parameters, fixture.wireAddress), 60 * fixture.wireMultiplier,
              fixture.name .. " 静态工艺与运行时送丝单位不一致")
    end
    if fixture.powerAddress == 63 then
        equal(last_write(parameters, 66), 1000, fixture.name .. " 宝辰鑫开光端点物理值")
        equal(last_write(parameters, 67), 500, fixture.name .. " 宝辰鑫关光端点物理值")
        equal(last_write(parameters, 68), 0, fixture.name .. " 宝辰鑫板卡缓升未关闭")
        equal(last_write(parameters, 69), 0, fixture.name .. " 宝辰鑫板卡缓降未关闭")
    elseif fixture.powerAddress == 90 then
        for _, address in ipairs({69, 98, 99, 100, 101}) do
            equal(#parameters:writes(address), 0, fixture.name .. " 不得下发固定设置 " .. tostring(address))
        end
    else
        equal(last_write(parameters, 561), 100, fixture.name .. " 旧超强开光比例旁路")
        equal(last_write(parameters, 563), 100, fixture.name .. " 旧超强关光比例旁路")
        equal(last_write(parameters, 562), 0, fixture.name .. " 旧超强板卡缓升未关闭")
        equal(last_write(parameters, 564), 0, fixture.name .. " 旧超强板卡缓降未关闭")
    end
    equal(#parameters:writes(rated), 0, fixture.name .. " 外控参数脚本写了设备额定功率")
    check_no_wire(fixture, parameters)
    pass_group(fixture, "actual craft parameter units and rated power isolation")

    local simulation = create_world(fixture)
    simulation:load(fixture.simulation, fixture.name .. " simulation")
    equal(#simulation:writes(fixture.powerAddress), 0, fixture.name .. " 轨迹模拟写了功率")
    equal(#simulation:writes(fixture.wireAddress), 0, fixture.name .. " 轨迹模拟写了送丝速度")
    for _, item in ipairs(simulation.events) do
        check(item.kind ~= "do", fixture.name .. " 轨迹模拟输出了设备使能")
    end
    pass_group(fixture, "public simulation emits no device IO")

    local actions = create_world(fixture)
    actions:load(fixture.publicStart, fixture.name .. " public start")
    actions:advance(1000)
    equal(last_write(actions, fixture.powerAddress), 2000, fixture.name .. " 独立起弧入口终值")
    actions:load(fixture.publicDown, fixture.name .. " public ramp down")
    actions:advance(1000)
    equal(last_write(actions, fixture.powerAddress), 500, fixture.name .. " 独立缓降入口终值")
    if fixture.wire then equal(#actions:offs(), 0, fixture.name .. " 独立缓降入口提前停丝") end
    actions:load(fixture.publicFinish, fixture.name .. " public finish")
    equal(actions.outputs[1], false, fixture.name .. " 独立收弧入口没有关光")
    if fixture.wire then equal(#actions:offs(), 1, fixture.name .. " 独立收弧入口停丝次数") end
    check_no_wire(fixture, actions)
    pass_group(fixture, "public standalone start, ramp-down and finish APIs")

    for _, variant in ipairs({{"short", fixture.programShort}, {"zero", fixture.programZero}}) do
        local terminal = create_world(fixture)
        terminal:load(variant[2], fixture.name .. " " .. variant[1] .. " terminal segment")
        local motion_end, laser_off, saw_laser
        for _, item in ipairs(terminal.events) do
            if item.kind == "move_end" and item.address == 10 then motion_end = item.time end
            if item.kind == "do" and item.address == 1 and item.value == true then saw_laser = true end
            if saw_laser and item.kind == "do" and item.address == 1 and item.value == false then
                laser_off = laser_off or item.time
            end
        end
        check(motion_end ~= nil and laser_off ~= nil, fixture.name .. " 缺少终点或关光事件")
        equal(laser_off, motion_end, fixture.name .. " " .. variant[1] .. " 末段在终点保持出光等待")
        if fixture.wire then
            local offs = terminal:offs()
            equal(#offs, 1, fixture.name .. " 短末段停丝次数")
            check(offs[1].time >= motion_end, fixture.name .. " 短末段提前触发回抽")
        end
        if not fixture.legacy then
            equal(last_write(terminal, fixture.powerAddress), 500, fixture.name .. " 短末段映射功率终值")
        end
        check_no_wire(fixture, terminal)
        pass_group(fixture, variant[1] .. " terminal segment closes laser at motion endpoint")
    end

    local pure = create_world(fixture)
    pure:load(fixture.pureStart, fixture.name .. " pure craft start")
    pure:advance(1000)
    equal(last_write(pure, fixture.powerAddress), 2000, fixture.name .. " 工艺快照起弧目标")
    if fixture.wire then
        equal(last_write(pure, fixture.wireAddress), 60 * fixture.wireMultiplier, fixture.name .. " 工艺快照送丝单位")
    end
    pure:load(fixture.pureDown, fixture.name .. " pure craft down")
    pure:advance(1000)
    equal(last_write(pure, fixture.powerAddress), 500, fixture.name .. " 工艺快照收弧目标")
    pure:load(fixture.pureFinish, fixture.name .. " pure craft finish")
    equal(pure.outputs[1], false, fixture.name .. " 工艺快照终点关光")
    if fixture.wire then equal(#pure:offs(), 1, fixture.name .. " 工艺快照停丝次数") end
    check_no_wire(fixture, pure)
    pass_group(fixture, "pure craft snapshot API start/down/finish")

    for _, on_api in ipairs({{"public", fixture.publicStart}, {"pure craft", fixture.pureStart}}) do
        local restart = fresh(fixture)
        restart:invoke(restart.env.cl_ramp_start, 1000, 1500,
                       fixture.wire and 30 * fixture.wireMultiplier or nil,
                       fixture.wire and 40 * fixture.wireMultiplier or nil, 5000, 0)
        restart:advance(25)
        restart.defer_delete = true
        local before_restart = #restart.events
        restart:load(on_api[2], fixture.name .. " " .. on_api[1] .. " ON preemption")
        local laser_off_index, wire_off_index
        for index = before_restart+1, #restart.events do
            local item = restart.events[index]
            if not laser_off_index and item.kind == "do" and item.address == 1 and item.value == false then
                laser_off_index = index
            end
            local wire_off = (item.kind == "do" and item.address == 2 and item.value == false) or
                (fixture.legacy and item.kind == "write" and item.address == 50 and item.value == 0) or
                (fixture.powerAddress == 90 and item.kind == "write" and item.address == 95 and item.value == 0)
            if wire_off and not wire_off_index then wire_off_index = index end
            if item.kind == "write" and not wire_off then
                check(laser_off_index ~= nil and laser_off_index < index,
                      fixture.name .. " 公有ON未停旧激光就写准备参数")
                if fixture.wire then
                    check(wire_off_index ~= nil and wire_off_index < index,
                          fixture.name .. " 公有ON未停旧送丝就写准备参数")
                end
            end
        end
        check(laser_off_index ~= nil, fixture.name .. " 公有ON未关闭旧周期激光")
        restart:advance(1500)
        restart:invoke(restart.env.cl_ramp_finish, 500,
                       fixture.wire and 20 * fixture.wireMultiplier or nil, 0)
        if fixture.wire then equal(#restart:offs(), 2, fixture.name .. " 公有ON抢占停丝次数") end
        check_no_wire(fixture, restart)
        pass_group(fixture, on_api[1] .. " ON stops old cycle before parameter preparation")
    end

    world:load(fixture.protection, fixture.name .. " natural completion protection")
    check(type(world.handlers[1]) == "function", fixture.name .. " 缺少停止事件保护")
    local completed_power = #world:writes(fixture.powerAddress)
    local completed_wire = #world:writes(fixture.wireAddress)
    local completed_events = #world.events
    world:invoke(world.handlers[3])
    world:advance(10000)
    equal(#world:writes(fixture.powerAddress), completed_power, fixture.name .. " 正常结束后继续恢复功率")
    equal(#world:writes(fixture.wireAddress), completed_wire, fixture.name .. " 正常结束后继续恢复送料")
    for index = completed_events+1, #world.events do
        local item = world.events[index]
        check(not (item.kind == "do" and item.address == 1 and item.value == true),
              fixture.name .. " 正常结束后继续重新出光")
    end
    world:invoke(world.handlers[1])
    check(world.registers[674] == nil or world.registers[674] == 0,
          fixture.name .. " 自然结束误报焊接中断")
    if fixture.wire then equal(#world:offs(), 1, fixture.name .. " 自然结束事件重复停丝") end
    pass_group(fixture, "natural stop event does not report interruption")

    for _, event_id in ipairs({1, 2}) do
        local protected = fresh(fixture)
        protected:load(fixture.protection, fixture.name .. " interruption protection")
        protected:invoke(protected.env.cl_ramp_start, 1000, 2000,
                         fixture.wire and 30 * fixture.wireMultiplier or nil,
                         fixture.wire and 60 * fixture.wireMultiplier or nil, 5000, 0)
        protected.registers[611] = true
        protected:advance(200)
        check(type(protected.handlers[event_id]) == "function", fixture.name .. " 缺少事件入口")
        protected:invoke(protected.handlers[event_id])
        equal(protected.outputs[1], false, fixture.name .. " 事件没有关闭激光")
        if event_id == 1 then equal(protected.registers[674], 1, fixture.name .. " 停止事件没有标记焊接中断") end
        local event_count = #protected.events
        protected:advance(10000)
        equal(#protected.events, event_count, fixture.name .. " 事件后残留任务写入")
        check_no_wire(fixture, protected)
        pass_group(fixture, "stop/pause/resume protection event " .. tostring(event_id))
    end
end

local function count_laser_on(world)
    local count = 0
    for _, item in ipairs(world.events) do
        if item.kind == "do" and item.address == 1 and item.value == true then count = count + 1 end
    end
    return count
end

local function run_pause_resume_cases(fixture)
    local w0 = fixture.wire and 30 * fixture.wireMultiplier or nil
    local w1 = fixture.wire and 60 * fixture.wireMultiplier or nil
    local w2 = fixture.wire and 20 * fixture.wireMultiplier or nil
    for _, phase in ipairs({"up", "steady", "down"}) do
        local world = fresh(fixture)
        world:load(fixture.protection, fixture.name .. " resumable protection")
        world:invoke(world.env.cl_ramp_start, 1000, 2000, w0, w1, 1000, 0)
        if phase == "up" then world:advance(200)
        else
            world:advance(1500)
            if phase == "down" then
                world:invoke(world.env.cl_ramp_down, 2000, 500, w1, w2, 1000)
                world:advance(200)
            end
        end
        world.defer_delete = true
        world.registers[611] = true
        world:invoke(world.handlers[2])
        equal(world.outputs[1], false, fixture.name .. " 暂停未关闭激光")
        local paused_events = #world.events
        world:advance(60000)
        equal(#world.events, paused_events, fixture.name .. " 长暂停期间仍有阶段输出")
        local before_resume = #world.events
        world:invoke(world.handlers[3])
        equal(world.outputs[1], true, fixture.name .. " 普通继续没有恢复出光")
        local on_count = count_laser_on(world)
        local created = world.background_created or 0
        world:invoke(world.handlers[3])
        equal(count_laser_on(world), on_count, fixture.name .. " 重复继续重复开启激光")
        equal(world.background_created or 0, created, fixture.name .. " 重复继续重复创建阶段任务")
        world:advance(1500)
        local target = phase == "down" and 500 or 2000
        equal(last_write(world, fixture.powerAddress), target, fixture.name .. " 暂停后阶段目标未恢复")
        if fixture.wire then equal(last_write(world, fixture.wireAddress), phase == "down" and w2 or w1,
                                   fixture.name .. " 暂停后送丝目标未恢复") end
        if phase ~= "steady" then
            local resumed_at, target_at
            for index = before_resume+1, #world.events do
                local item = world.events[index]
                if not resumed_at and item.kind == "do" and item.address == 1 and item.value == true then
                    resumed_at = item.time
                end
                if resumed_at and not target_at and item.kind == "write"
                        and item.address == fixture.powerAddress and item.value == target then
                    target_at = item.time
                end
            end
            check(resumed_at ~= nil and target_at ~= nil and target_at-resumed_at <= 950,
                  fixture.name .. " 继续重放了整段坡度，未沿剩余时间恢复")
        end
        world:invoke(world.env.cl_ramp_finish, target, phase == "down" and w2 or w1, 1000)
        check_no_wire(fixture, world)
        pass_group(fixture, phase .. " pause, long wait, resume remaining phase, duplicate resume")
    end

    -- 准备中的起弧被暂停，继续应接管准备权；旧主调用不能随后复活。
    for _, preparation in ipairs({"runtime", "pure public"}) do
        local world = fresh(fixture)
        world:load(fixture.protection, fixture.name .. " preparation pause protection")
        world.registers[611] = true
        if preparation == "runtime" then
            world:add_task(function() world.env.cl_ramp_start(1000, 2000, w0, w1, 1000, 0) end, "待暂停的初值准备")
        else
            local chunk, message = loadstring(fixture.pureStart, fixture.name .. " pending pure start")
            check(chunk ~= nil, tostring(message))
            setfenv(chunk, world.env)
            world:add_task(chunk, "待暂停的公有参数准备")
        end
        world:advance(25)
        world.defer_delete = true
        world:invoke(world.handlers[2])
        local paused = #world.events
        world:advance(10000)
        equal(#world.events, paused, fixture.name .. " 准备暂停后旧调用继续写入")
        world:invoke(world.handlers[3])
        world:advance(1500)
        equal(world.outputs[1], true, fixture.name .. " 准备暂停后普通继续未重新起弧")
        equal(last_write(world, fixture.powerAddress), 2000, fixture.name .. " 准备暂停后目标功率")
        world:invoke(world.env.cl_ramp_finish, 500, w2, 0)
        check_no_wire(fixture, world)
        pass_group(fixture, preparation .. " preparation pause/resume without old caller revival")
    end

    for _, terminal in ipairs({"stop", "abort", "finish"}) do
        local world = fresh(fixture)
        world:load(fixture.protection, fixture.name .. " cancelled resume permission")
        world.registers[611] = true
        world:invoke(world.env.cl_ramp_start, 1000, 2000, w0, w1, 1000, 0)
        world:advance(200)
        world:invoke(world.handlers[2])
        if terminal == "stop" then world:invoke(world.handlers[1])
        elseif terminal == "abort" then world:invoke(world.env.cl_ramp_abort)
        else world:invoke(world.env.cl_ramp_finish, 500, w2, 0) end
        local on_count, power_count = count_laser_on(world), #world:writes(fixture.powerAddress)
        local wire_count = #world:writes(fixture.wireAddress)
        world:invoke(world.handlers[3])
        world:advance(10000)
        equal(count_laser_on(world), on_count, fixture.name .. " " .. terminal .. " 后继续重新出光")
        equal(#world:writes(fixture.powerAddress), power_count, fixture.name .. " " .. terminal .. " 后恢复功率写入")
        equal(#world:writes(fixture.wireAddress), wire_count, fixture.name .. " " .. terminal .. " 后恢复速度写入")
        check_no_wire(fixture, world)
        pass_group(fixture, "pause then " .. terminal .. " revokes resume permission")
    end

    if fixture.legacy and fixture.wire then
        local world = fresh(fixture)
        world:load(fixture.protection, fixture.name .. " pending handoff pause protection")
        world:invoke(world.env.cl_ramp_start, 1000, 2000, w0, w1, 5000, 0)
        world:advance(25)
        world:add_task(function() world.env.cl_ramp_finish(500, w2, 0) end, "旧周期停丝排队")
        world:advance(1)
        local start, message = loadstring(fixture.pureStart, fixture.name .. " pending handoff ON")
        check(start ~= nil, tostring(message))
        setfenv(start, world.env)
        world:add_task(start, "新起弧接管旧停丝队列")
        world:advance(1)
        world.defer_delete = true
        world:invoke(world.handlers[2])
        local paused = #world.events
        world:advance(10000)
        equal(#world.events, paused, fixture.name .. " 停丝接管期暂停后仍执行旧准备")
        world:invoke(world.handlers[3])
        world:advance(1500)
        equal(world.outputs[1], true, fixture.name .. " 停丝接管期暂停丢失新起弧恢复权")
        equal(last_write(world, fixture.powerAddress), 2000, fixture.name .. " 停丝接管期恢复后的目标功率")
        world:invoke(world.env.cl_ramp_finish, 500, w2, 0)
        equal(#world:offs(), 2, fixture.name .. " 停丝接管期暂停产生重复或遗漏停丝")
        pass_group(fixture, "pause while new ON takes ownership of pending old serial wire-off")
    end
end

local function run_phase_wait_cases(fixture)
    local w0 = fixture.wire and 30 * fixture.wireMultiplier or nil
    local w1 = fixture.wire and 60 * fixture.wireMultiplier or nil
    local w2 = fixture.wire and 20 * fixture.wireMultiplier or nil
    local world = fresh(fixture)
    check(type(world.env.cl_ramp_wait_phase) == "function", fixture.name .. " 缺少阶段等待接口")
    world:invoke(world.env.cl_ramp_start, 2000, 2000, w1, w1, 0)
    world:invoke(world.env.cl_ramp_down, 2000, 500, w1, w2, 200)
    local phase_started = world.now
    local completed = world:invoke(world.env.cl_ramp_wait_phase)
    equal(completed, true, fixture.name .. " 正常缓降等待未返回完成")
    equal(last_write(world, fixture.powerAddress), 500, fixture.name .. " 等待完成前功率未到终值")
    if fixture.wire then
        equal(last_write(world, fixture.wireAddress), w2, fixture.name .. " 等待完成前送丝未到终值")
        equal(#world:offs(), 0, fixture.name .. " 阶段等待提前停丝")
    end
    equal(world.outputs[1], true, fixture.name .. " 阶段等待提前关光")
    local last_power = world:writes(fixture.powerAddress)
    local last_frame_at = last_power[#last_power].time
    if fixture.wire then
        local last_wire = world:writes(fixture.wireAddress)
        last_frame_at = math.max(last_frame_at, last_wire[#last_wire].time)
    end
    if fixture.legacy then
        check(world.now >= last_frame_at + fixture.settle,
              fixture.name .. " 阶段等待在最后串口帧稳定前返回")
        check(world.now <= last_frame_at + fixture.settle + 1,
              fixture.name .. " 阶段等待在实际最后帧稳定后额外驻留: returned=" .. tostring(world.now)
              .. " lastFrame=" .. tostring(last_frame_at) .. " settle=" .. tostring(fixture.settle))
    else
        check(world.now <= phase_started + 201, fixture.name .. " 映射阶段完成后增加固定等待")
    end
    local repeat_at = world.now
    equal(world:invoke(world.env.cl_ramp_wait_phase), true, fixture.name .. " 重复等待已完成阶段")
    equal(world.now, repeat_at, fixture.name .. " 已完成阶段重复等待产生延迟")
    world:invoke(world.env.cl_ramp_finish, 500, w2)
    check_no_wire(fixture, world)
    pass_group(fixture, "phase wait returns only after exact final values and serial stability")

    local zero = fresh(fixture)
    zero:invoke(zero.env.cl_ramp_start, 2000, 2000, w1, w1, 0)
    local zero_at = zero.now
    zero:invoke(zero.env.cl_ramp_down, 2000, 500, w1, w2, 0)
    equal(zero:invoke(zero.env.cl_ramp_wait_phase), true, fixture.name .. " 零缓降等待未完成")
    equal(zero.now, zero_at, fixture.name .. " 零缓降等待在终点驻留")
    zero:invoke(zero.env.cl_ramp_finish, 500, w2)
    check_no_wire(fixture, zero)
    pass_group(fixture, "zero duration phase wait returns immediately without endpoint dwell")

    local paused = fresh(fixture, 10)
    paused:load(fixture.protection, fixture.name .. " phase wait pause protection")
    paused:invoke(paused.env.cl_ramp_start, 2000, 2000, w1, w1, 0)
    paused:invoke(paused.env.cl_ramp_down, 2000, 500, w1, w2, 1000)
    local pause_result, pause_returned_at
    local pause_waiter = paused:add_task(function()
        pause_result = paused.env.cl_ramp_wait_phase()
        pause_returned_at = paused.now
        if pause_result then paused.env.cl_ramp_finish(500, w2) end
    end, "跨暂停的点焊阶段等待")
    paused:advance(200)
    paused.defer_delete = true
    paused:invoke(paused.handlers[2])
    check(paused.tasks[pause_waiter].alive, fixture.name .. " 暂停使点焊等待错误结束")
    equal(pause_result, nil, fixture.name .. " 暂停被误判为阶段完成或取消")
    local paused_events = #paused.events
    paused:advance(60000)
    equal(#paused.events, paused_events, fixture.name .. " 点焊长暂停期间仍有设备输出")
    check(paused.tasks[pause_waiter].alive, fixture.name .. " 长暂停被阶段超时取消")
    local resume_at = paused.now
    paused:invoke(paused.handlers[3])
    paused:advance(1500)
    equal(pause_result, true, fixture.name .. " 暂停恢复后等待未完成原逻辑阶段")
    check(pause_returned_at ~= nil and pause_returned_at > resume_at,
          fixture.name .. " 继续未等待剩余坡度就结束点焊")
    equal(paused.outputs[1], false, fixture.name .. " 点焊等待完成后未执行终点关光")
    if fixture.wire then equal(#paused:offs(), 2, fixture.name .. " 暂停和完成应各停丝一次") end
    check_no_wire(fixture, paused)
    pass_group(fixture, "point weld phase wait survives long pause and resumes remaining logical phase")

    for _, terminal in ipairs({"abort", "finish", "stop", "new start", "pure new start", "new down", "retarget"}) do
        local cancelled = fresh(fixture)
        cancelled:load(fixture.protection, fixture.name .. " phase wait cancellation protection")
        cancelled:invoke(cancelled.env.cl_ramp_start, 2000, 2000, w1, w1, 0)
        cancelled:invoke(cancelled.env.cl_ramp_down, 2000, 500, w1, w2, 1000)
        local wait_result, stale_finish = nil, false
        cancelled:add_task(function()
            wait_result = cancelled.env.cl_ramp_wait_phase()
            if wait_result then
                stale_finish = true
                cancelled.env.cl_ramp_finish(500, w2)
            end
        end, "必须被抢占的旧点焊等待")
        cancelled:advance(25)
        cancelled.defer_delete = true
        if terminal == "abort" then cancelled:invoke(cancelled.env.cl_ramp_abort)
        elseif terminal == "finish" then cancelled:invoke(cancelled.env.cl_ramp_finish, 500, w2)
        elseif terminal == "stop" then cancelled:invoke(cancelled.handlers[1])
        elseif terminal == "new start" then
            cancelled:invoke(cancelled.env.cl_ramp_start, 1000, 1500, w0, w1, 1000)
        elseif terminal == "pure new start" then
            cancelled:load(fixture.pureStart, fixture.name .. " pure new ON cancels point wait")
        elseif terminal == "new down" then
            cancelled:invoke(cancelled.env.cl_ramp_down, 1700, 800, w1, w2, 1000)
        else cancelled:invoke(cancelled.env.cl_ramp_retarget, 1400, w1) end
        cancelled:advance(2000)
        equal(wait_result, false, fixture.name .. " " .. terminal .. " 未取消旧逻辑阶段等待")
        equal(stale_finish, false, fixture.name .. " 旧点焊等待错误执行终点关闭新周期")
        if terminal == "new start" or terminal == "pure new start"
                or terminal == "new down" or terminal == "retarget" then
            equal(cancelled.outputs[1], true, fixture.name .. " 旧等待关闭了抢占后的有效激光")
            cancelled:invoke(cancelled.env.cl_ramp_finish, 500, w2)
        else equal(cancelled.outputs[1], false, fixture.name .. " 终止后残留激光输出") end
        check_no_wire(fixture, cancelled)
        pass_group(fixture, "phase wait cancellation by " .. terminal .. " cannot finish a newer cycle")
    end

    if fixture.legacy and fixture.wire then
        local pending = fresh(fixture)
        pending:invoke(pending.env.cl_ramp_start, 2000, 2000, w1, w1, 0)
        pending:advance(fixture.settle)
        pending:invoke(pending.env.cl_ramp_down, 2000, 500, w1, w2, 200)
        local pending_result
        pending:add_task(function() pending_result = pending.env.cl_ramp_wait_phase() end,
                         "等待最后送丝帧排队及稳定")
        pending:advance(200)
        equal(last_write(pending, fixture.powerAddress), 500, fixture.name .. " 最后功率帧未实际发送")
        equal(pending_result, nil, fixture.name .. " 最后送丝帧尚排队就提前结束")
        pending:advance(49)
        equal(pending_result, nil, fixture.name .. " 共享缓冲忙时阶段等待提前返回")
        pending:advance(100)
        equal(pending_result, true, fixture.name .. " 最后送丝帧稳定后仍未结束")
        equal(last_write(pending, fixture.wireAddress), w2, fixture.name .. " 最后送丝帧没有到设备")
        pending:invoke(pending.env.cl_ramp_finish, 500, w2)
        pass_group(fixture, "phase wait includes last wire frame queued behind shared serial buffer")
    end
end

local function run_red_light_preservation(fixture)
    for _, initial in ipairs({true, false}) do
        local world = create_world(fixture, 1, initial)
        world:load(fixture.program, fixture.name .. " preserve red light in full program")
        equal(world.outputs[0], initial, fixture.name .. " 完整焊接改变了红光初值")
        world:load(fixture.protection, fixture.name .. " preserve red light in events")
        world.registers[611] = true
        world:invoke(world.env.cl_ramp_start, 1000, 2000,
                     fixture.wire and 30 * fixture.wireMultiplier or nil,
                     fixture.wire and 60 * fixture.wireMultiplier or nil, 1000, 0)
        world:advance(200)
        world:invoke(world.handlers[2])
        equal(world.outputs[0], initial, fixture.name .. " 暂停改变了红光初值")
        world:advance(10000)
        world:invoke(world.handlers[3])
        equal(world.outputs[0], initial, fixture.name .. " 继续改变了红光初值")
        world:invoke(world.env.cl_ramp_abort)
        equal(world.outputs[0], initial, fixture.name .. " 异常退出改变了红光初值")
        check_no_wire(fixture, world)
        pass_group(fixture, "CO0 red light initial " .. tostring(initial) .. " preserved through welding/pause/resume/abort")
    end
end

for _, fixture in ipairs(fixtures) do
    run_runtime_cases(fixture)
    run_program_case(fixture)
    run_pause_resume_cases(fixture)
    run_phase_wait_cases(fixture)
    run_red_light_preservation(fixture)
    print("PASS runtime: " .. fixture.name)
end
print("PASS executable sequence runtime: " .. tostring(checks) .. " checks, "
      .. tostring(groups) .. " scenario groups, virtual clock/task/IO only")
