# Attacking the DSA

For real-world attacks, we opt for these attack scenarios:
- [Spying on DPDK networking stack](#dpdk)
- [Spying on DTO-hooked SSH](#ssh-keystrokes)
- [Spying on DTO-hooked LLM inference](#llm-inference)
- [Covert channel](#covert-channel)

## DPDK

0. Setup DSA
```bash
cd $(ROOTDIR)/setup
sudo ./setup_dsa.sh config/dpdk.conf
```

1. Build the VPP image with DPDK support. Since DPDK is not standalone, we select one of the most active DPDK-based projects, VPP (Vector Packet Processing), to build the image.
```bash
cd $(ROOTDIR)/setup/vpp
docker build --build-arg REPO="2506" -t vpp .
```

2. Setup host network for memif master node
```bash
./startvpp1.sh
```

3. Start the VPP container with DPDK support
```bash
docker run -it --rm --name=vpp2 --privileged --network none --dns 114.114.114.114 \
        -v /sys/bus/dsa/devices:/sys/bus/dsa/devices \
        -v /dev/dsa:/dev/dsa \
        -v /run/vpp/memif.sock:/run/vpp/memif.sock \
        -v ./config:/vpp/config \
        vpp /bin/bash
```
Explanation:
- `--privileged`: Grant to access DSA devices.
- `--network none`: Disable the default network stack to avoid conflicts.
- `--dns *` Explicitly set DNS because we disable the default network stack.
- `-v *` Map DSA devtree and memif socket to the container.

4. Verify that the DSA devices are available in the container.

Ping any websites from the container to ensure that the network is working:
```bash
ping -c 3 google.com
```

In host terminal, run:
```bash
sudo vppctl -s /run/vpp/cli-vpp1.sock show dma
```

If you see any non-zero values in `hw`, for example

```
Config 0 thread 0 dma 0/0 request 6913997          hw 12          cpu 2 
```

it means that the DSA is working and you can proceed to the next step.

5. Collect data from the DSA devices.

```bash
cd $(ROOTDIR)/attack
make build TARGET=monitor
./run_wf.sh
```

It takes approximately 5 hours to get traces of 15 websites. Be patient.

6. Classify the data. After executing the previous step, the data will be stored in `evaluate/wfdata`.

```bash
cd $(ROOTDIR)/evaluate
python3 classify.py
```

View the results in terminal for accuracy and the confusion matrix will be saved in `evaluate/data/confusion_matrix.pdf`.

### Tuning

These parameters in `monitor.c` can be tuned to improve the spy performance:
```c
#define ATTACK_INTERVAL_US 10
#define TRACE_BUFFER_SIZE 100000
```
- `ATTACK_INTERVAL_US`: The interval of Prime+Probe in microseconds. The smaller the value, the more accurate the trace, but it may cause more noise.
- `TRACE_BUFFER_SIZE`: The size of the trace buffer. The larger the value, the more data can be collected. But dont exceed too much from the victim's activity, as idle data will include noise and useless data.

Also, for classification NN, you can tune the parameters in `evaluate/classify.py`:
```python
sequence_len = 250
group_size = 400
samples_per_class = 200

# ...

def build_model(input_shape, num_classes):
    inputs = Input(shape=input_shape)
    x = Bidirectional(LSTM(128, return_sequences=True))(inputs)
    x = Dropout(0.35)(x)
    x = Bidirectional(LSTM(128, return_sequences=True))(x)
    x = Dropout(0.3)(x)
    x = AttentionLayer()(x)
    x = Dropout(0.26)(x)
    outputs = Dense(num_classes, activation='softmax')(x)
    model = Model(inputs, outputs)
    return model

# ...

model.compile(optimizer=tf.keras.optimizers.Adam(learning_rate=1e-3),
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

model.fit(X_train, y_train,
          validation_data=(X_test, y_test),
          epochs=50,
          batch_size=128,
          callbacks=[early_stop])
```
- `sequence_len`: Number of traces in a time slot. The smaller the value, the more accurate the trace, but it increases the vector size and may cause overfitting or insufficient model capacity.
- `group_size`: Number of traces in a sample. `group_size * sequence_len` equals to the `TRACE_BUFFER_SIZE`
- `samples_per_class`: Number of samples per websites. No need to change this value, as it is used in the paper.
- `build_model`: The model architecture. For more websites to classify, you can increase the number of LSTM layers or the number of units in each layer. The current model is sufficient for 15 websites. For tuning, adjust the dropout rates.
- `learning_rate`: The learning rate for the optimizer. May increase the value to speed up the training, but it may cause oscillation.
- `epochs`: The number of epochs to train the model. You can increase this value to improve the accuracy, but it may cause overfitting.
- `batch_size`: The batch size for training.

## DTO

### SSH Keystrokes

Setup DSA

```bash
cd $(ROOTDIR)/setup
sudo ./setup_dsa.sh config/common.conf
```

Fetch DTO library and DSA reversing tools

```bash
git submodule update --init --recursive

cd setup/dto
make libdto && cd ../
```

Build the image

```bash
cd ..
docker build -t dto_dsa -f Dockerfile.dto .
```

Start Docker container with

```bash
docker run -it --privileged --name=dto1 \
    -v /dev/dsa:/dev/dsa \
    -v /sys/bus/dsa:/sys/bus/dsa \
    -v ./:/root/ \
    --env-file dto.env \ 
    dto_dsa /bin/bash
```

Now you can run any application with DSA acceleration, and most importantly,
you can spy on the `memcpy/memset/memcmp/memmove` made by the application.

Run another container as remote SSH server

```bash
docker run -it --rm --name=ssh_server ubuntu:latest /bin/bash

# inside
apt update && apt install -y ssh
service ssh start
echo "root:123456" | sudo chpasswd
```

Run a the SSH script and open another terminal to monitor the DSA traffic.

```bash
# Container
python sshev.py

# Host
cd $(ROOTDIR)/attack
make spy
sudo taskset -c 8 ./build/atc_spy ssh
# or
sudo taskset -c 8 ./build/swq_spy ssh
```

If you grant `CAP_SYS_RAWIO` to the `atc_spy` binary, you can run it without `sudo`.

Evaluate the results

```bash
cd $(ROOTDIR)/evaluate
mv ground-ts.txt data/atc-ground-ts.txt # or swq-ground-ts.txt
mv ../atc-ssh-ts.txt data/ # or swq-ssh-ts.txt
python3 keystroke.py
```

### LLM Inference

Build docker image for LLM inference

```bash
cd $(ROOTDIR)/setup
docker build -t dto-llm -f Dockerfile.llm .
```

Build llama2.c

```bash
git clone https://github.com/karpathy/llama2.c
make run
```

Start the container with

```bash
# llama2.c
docker run -it --rm --name=dto1 --privileged \
            -v /dev/dsa:/dev/dsa \
            -v /sys/bus/dsa:/sys/bus/dsa \
            -v ./llama2.c:/root/llama \
            --env-file ~/dsa-reversing/setup/dto.env \
            dto /bin/bash

# ollama
docker run -d --rm --gpus '"device=0"' --name=dto2 --privileged \
           -v /dev/dsa:/dev/dsa \
           -v /sys/bus/dsa:/sys/bus/dsa \
           -v ./models:/root/models \
           --env-file ~/dsa-reversing/setup/dto.env dto-llm
```

Download the model and run the inference

```bash
./get_models.sh
```

To get llama2, follow the [instructions](https://github.com/karpathy/llama2.c?tab=readme-ov-file#metas-llama-2-models).

Collect the data

```bash
cd $(ROOTDIR)/attack
make build TARGET=monitor
./llm.sh
```

### Tuning

For `atc_spy` and `swq_spy`, tune these parameters
```c
#define ATTACK_INTERVAL_US 3000
#define UPDATE_THRESHOLD(hit, miss) ((hit * 34 + miss * 66) / 100)
```
- `ATTACK_INTERVAL_US`: The interval of Prime+Probe/Congest+Probe in microseconds. The smaller the value, the more accurate the trace, but it may cause more noise.
- `UPDATE_THRESHOLD`: The threshold of `atc_spy`. Adjust this to optimize.

For LLM inference attack, see tuning guide in DPDK section.

## Covert channel

Setup DSA

```bash
cd $(ROOTDIR)/setup
sudo ./setup_dsa.sh config/common.conf
```

Build the binaries and run the covert channel

```bash
cd $(ROOTDIR)/attack
make covert
./run_cc.sh atc # atc or swq
```

Evaluate the results

```bash
cd $(ROOTDIR)/evaluate
mv ../attack/msg ./ccdata
python3 covert.py
```

This script also plots the results of multiple configurations, but for now we manually record the
data and fill in the script with the results. Uncomment the `draw_graph()` line in `covert.py` to plot the results.

To enable multithreading transmission, setup DSA first

```bash
cd $(ROOTDIR)/setup
sudo ./setup_dsa.sh config/full.conf
```

### Tuning

For both covert channel, the general parameters are
```c
#define BITS_REPEAT 1
#define START_BITS 2
```
- `BITS_REPEAT`: The number of times to repeat each bit. The larger the value, the more reliable the transmission, but it may cause more time.
- `START_BITS`: The number of bits to synchronize the sender and receiver. Large value may cause receiver to miss the synchronization, but small value may cause the receiver to misinterpret the data.

The interval of DevTLB attacker. Smaller value may cause more noise, but larger value may take longer to transmit the data.

```c
#define BIT_INTERVAL_NS 43000
```

The interval of SWQ attacker. This value should be close to the maximum congestion time, benchmarked by the `playground/wq/congest.c` program.

```c
#define WQ_INTERVAL_NS 130000
```