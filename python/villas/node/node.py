import json
import tempfile
import subprocess
import logging
import signal
import requests
import uuid
import datetime

LOGGER = logging.getLogger('villas.node')

class Node(object):

    def __init__(self, log_filename = None, config_filename = None, config = { }, api = 'http://localhost:8080', executable = 'villas-node', **kwargs):
        self.config = config
        self.config_filename = config_filename
        self.log_filename = log_filename
        self.api_endpoint = api
        self.executable = executable

        self.child = None

    def start(self):
        self.config_file = tempfile.NamedTemporaryFile(mode='w+', suffix='.json')

        if self.config_filename:
            with open(self.config_filename) as f:
                self.config_file.write(f.read())
        else:
            json.dump(self.config, self.config_file)

        self.config_file.flush()

        if self.log_filename is None:
            now = datetime.datetime.now()
            self.log_filename = now.strftime('villas-node_%Y-%m-%d_%H-%M-%S.log')

        self.log = open(self.log_filename, 'w+')

        LOGGER.info("Starting VILLASnode with config: %s", self.config_file.name)

        self.child = subprocess.Popen([self.executable, self.config_file.name], stdout=self.log, stderr=self.log)

    def pause(self):
        self.child.send_signal(signal.SIGSTOP)

    def resume(self):
        self.child.send_signal(signal.SIGCONT)

    def stop(self):
        self.child.send_signal(signal.SIGTERM)
        self.child.wait()

        self.log.close()

    def restart(self):
        self.request('restart')

    @property
    def active_config(self):
        resp = self.request('config')

        return resp['response']

    @property
    def nodes(self):
        return self.request('nodes')['response']

    @property
    def paths(self):
        return self.request('paths')['response']

    def request(self, action, req = None):
        body = {
            'action' : action,
            'id' : str(uuid.uuid4())
        }

        if req is not None:
            body['request'] = req

        r = requests.post(self.api_endpoint + '/api/v1', json=body, timeout=1)

        r.raise_for_status()

        return r.json()

    def get_version(self):
            ver = subprocess.check_output([ self.executable, '-V'], )

            return ver.decode('ascii').rstrip()

    def is_running(self):
        if self.child is None:
            return False
        else:
            return self.child.poll() is None
