echo "Checking DSA support..."
if ! sudo dmesg | grep -q "idxd "; then
    echo "DSA support not found in dmesg. Please ensure your kernel has DSA support enabled."
    exit 1
fi

echo "Installing dependencies..."

sudo apt install accel-config libaccel-config-dev libaccel-config1 uuid-dev
curl -fsSL https://get.docker.com | sh

export ROOTDIR=$(pwd)

cd $ROOTDIR/setup && chmod +x *.sh
sudo ./setup_dsa.sh config/common.conf
docker build -t dto_dsa -f Dockerfile.dto .
docker build -t dto-llm -f Dockerfile.llm .

git clone https://github.com/karpathy/llama2.c && cd llama2.c && make run && cd ..
./get_models.sh

cd vpp && docker build --build-arg REPO="2506" -t vpp . && ./startvpp1.sh
cd $ROOTDIR/evaluate && pip3 install -r requirements.txt

cd $ROOTDIR
echo "Setup complete."