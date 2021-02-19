function prepare()
	print("Preparing test_hook")

	my_global_var = 1337
end


function start()
	print("Starting test_hook")

	print("Global var: ", my_global_var)
end


function stop()
	print("Stopping test_hook")
end


function process(seq, ts_origin, ts_recv, data, flags)
	print("Process test_hook")

	out = ""
	for key, value in pairs(smp) do
		out = out .. " " .. value
	end

	-- smp[1] = smp[1] * 10

	print(out)

	return smp
end


function periodic()
	print("Periodic test_hook")
end


function restart()
	print("Restarted test_hook")
end
