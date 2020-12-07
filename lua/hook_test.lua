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


function process(data)
	print("Process test_hook")
	printf("Test data: ", data)
end


function periodic()
	print("Periodic test_hook")
end
