import paramiko
import time

# Configuration
hostname = '172.17.0.1'
username = 'user'
password = 'password'
command = 'ssh root'

# Connect to SSH server
client = paramiko.SSHClient()
client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client.connect(hostname, port=22, username=username, password=password)

# Open a session with a pseudo-terminal
channel = client.exec_command("bash", get_pty=True)[0]

# Give shell time to initialize
time.sleep(10)

# Send characters one at a time with high-accuracy timestamps
timestamps = []

for char in command:
    timestamp = time.clock_gettime(time.CLOCK_MONOTONIC)
    channel.write(char)
    # channel.send(char)
    timestamps.append((char, timestamp))
    time.sleep(2)  # simulate human typing

# Wait for spy to exit
time.sleep(10)

# Print recorded timestamps
for char, ts in timestamps:
    print(f"{repr(char)} at {ts:.9f}")

# Cleanup
channel.close()
client.close()
