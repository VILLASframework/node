-- Install with: luarocks install lunajson luasockets

json = require 'lunajson'
http = require 'socket.http'

------------------------ Constants -------------------------
Reason = {
	OK = 0,
	ERROR = 1,
	SKIP_SAMPLE = 2,
	STOP_PROCESSING = 3
}

SampleFlags = {
	HAS_TS_ORIGIN   = 1,      -- "(1 <<  1)"    Include origin timestamp in output.
	HAS_TS_RECEIVED = 2,      -- "(1 <<  2)"    Include receive timestamp in output.
	HAS_OFFSET      = 4,      -- "(1 <<  3)"    Include offset (received - origin timestamp) in output.
	HAS_SEQUENCE    = 8,      -- "(1 <<  4)"    Include sequence number in output.
	HAS_DATA        = 16,     -- "(1 <<  5)"    Include values in output.
	HAS_ALL         = 15      -- "(1 <<  6) -1" Enable all output options.
}


-------------------------- Config --------------------------
city = 'Aachen'

--------------------- Global Variables ---------------------
f = 1           -- in Hz
t = nil         -- in seconds
t_start = nil   -- in seconds
temp_aachen = 0 -- in deg Celsius

function prepare(cfg)
	info("Preparing test_hook")

	info("Some setting: ", cfg.some_setting)
	info("Some other setting: ", cfg.this.is.nested)

	my_global_var = 1337
end


function start()
	info("Starting test_hook")

	info("Global var: ", my_global_var)

	info("This is a message with severity info")
	warn("This is a message with severity warn")
	error("This is a message with severity error")
	debug("This is a message with severity debug")

	t_start = socket.gettime()
end


function stop()
	info("Stopping test_hook")
end


function restart()
	info("Restarted test_hook")
end


function process(smp)
	info("Process test_hook")	

	if smp.sequence == 1 then
		dump_sample(smp)
	end

	-- We can adjust signal values inline
	smp.data.square = smp.data.square * 10

	-- Example for value limiter
	if smp.data.random > 10 then
		smp.data.random = 10
	elseif smp.data.random < -10 then
		smp.data.random = -10
	end

	global_var = smp.sequence

	-- Updating time (references in signal expression list of VILLASnode config)
	t = socket.gettime() - t_start

	if smp.sequence % 2 == 0 then
		return Reason.SKIP_SAMPLE
	elseif smp.sequence >= 200 then
		return Reason.ERROR
	end

	return Reason.OK
end



function periodic()
	info("Periodic test_hook")

	local body, statusCode, headers, statusText = http.request('https://wttr.in/' .. city .. '?format=j1')

	weather = json.decode(body)

	temp_aachen = tonumber(weather.current_condition[1].temp_C)

	info(string.format('Temperature in %s is %d °C', city, temp_aachen))
end


function dump_sample(smp)
	info("  Sequence:    ", smp.sequence)
	info("  Flags:       ", smp.flags)
	info("  TS origin:   ", string.format("%d.%09d", smp.ts_origin[0], smp.ts_origin[1]))
	info("  TS received: ", string.format("%d.%09d", smp.ts_received[0], smp.ts_received[1]))
	info("  Data:")
	for key, value in pairs(smp.data) do
		info("   " .. key .. ": " .. value)
	end
end
