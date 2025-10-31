# detect if stress-ng installed
if ! command -v stress-ng &> /dev/null
then
    echo "stress-ng could not be found, installing..."
    sudo apt-get update
    sudo apt-get install -y stress-ng
fi

# generate memory, ssd and network
stress-ng --cpu 0 --vm 0 --vm-bytes 80% --stream 0 --hdd 4 --hdd-path /tmp --fork 8 --timeout 2m --metrics