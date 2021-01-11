import sys

class StdoutStream:
    def __init__(self):
        self.buffer = ''
    def write(self, text):
        self.buffer = self.buffer + text

class StderrStream:
    def __init__(self):
        self.buffer = ''
    def write(self, text):
        self.buffer = self.buffer + text

stdout_stream = StdoutStream()
sys.stdout = stdout_stream
stderr_stream = StderrStream()
sys.stderr = stderr_stream
