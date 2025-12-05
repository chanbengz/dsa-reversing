import paramiko
import time
import random

# Configuration
hostname = 'ssh_server'
username = 'root'
password = '123456'
KEYSTROKES = 512

# Connect to SSH server
client = paramiko.SSHClient()
client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client.connect(hostname, port=22, username=username, password=password)

# Open a session with a pseudo-terminal
# channel = client.exec_command("/bin/bash -i", get_pty=True)[0]
channel = client.invoke_shell()
time.sleep(5)

# Send characters one at a time with high-accuracy timestamps
timestamps = []
for _ in range(KEYSTROKES):
    timestamp = time.clock_gettime(time.CLOCK_MONOTONIC)
    channel.send(b'a')
    timestamps.append(timestamp)
    time.sleep(random.randrange(300, 700) / 1000)  # simulate human typing

# Print recorded timestamps
with open('ground-ts.txt', 'w') as f:
    for ts in timestamps:
        f.write(f"{ts:.9f}\n")
