# -*- coding: utf-8 -*-
import villas_pb2
import time, socket, errno, sys, os, signal

layer = sys.argv[1] if len(sys.argv) == 2 else 'udp'

if layer not in ['udp', 'unix']:
	raise Exception('Unsupported socket type')

if layer == 'unix':
	local  = '/var/run/villas-node.client.sock'
	remote = '/var/run/villas-node.server.sock'

	skt = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

	# Delete stale sockets
	if os.path.exists(local):
		os.unlink(local)
elif layer == 'udp':
	local  = ('0.0.0.0', 12001)
	remote = ('127.0.0.1', 12000)

	skt = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print('Start client...')

skt.bind(local)

# Try to connect in case Unix domain socket does not exist yet..
connected = False
while not connected:
	try:
		skt.connect(remote)
	except socket.error as serr:
		if serr.errno not in [ errno.ECONNREFUSED, errno.ENOENT ]:
			raise serr
			
		print('Not connected. Retrying in 1 sec')
		time.sleep(1)
	else:
		connected = True

print('Ready. Ctrl-C to quit.')

msg = villas_pb2.Message()


# Gracefully shutdown
def sighandler(signum, frame):
	running = False

signal.signal(signal.SIGINT, sighandler)
signal.signal(signal.SIGTERM, sighandler)

running = True

while running:
	dgram = skt.recv(1024)
	if not dgram:
		break
	else:
		msg.ParseFromString(dgram)
		print(msg)
		
		skt.send(msg.SerializeToString())

skt.close()

print('Bye.')
