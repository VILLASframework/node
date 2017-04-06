function fileStart(e)
{
	var file = e.target.files[0];
	var reader = new FileReader();
	
	var start;
	var msgs = [ ]
	var position = 0;

	reader.onload = function(e) {
		var lines = e.target.result.split(/[\r\n]+/g); // tolerate both Windows and Unix linebreaks

		for (var i = 0; i < lines.length; i++) {
			var msg = new Msg();
			msg.parse(lines[i]);
			msgs.push(msg);
		}

		console.log('Read ' + msgs.length + ' samples from file');

		if (msgs.length > 0) {
			var offset = Date.now() - msgs[0].ts;
			var data = [];
			
			for (var i = 0; i < msgs.length; i++)
				data.push(msgs[i].ts + offset, msgs[i].data[0]);
		
			plotData.push(data);
		}
		else {
			
		}
	};

	reader.readAsText(file);
}
