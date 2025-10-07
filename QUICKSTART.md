# Quick Start Guide

## On Linux (for your humanoid)

### 1. Install SOEM
```bash
# Install build tools
sudo apt install build-essential cmake git

# Clone and build SOEM
git clone https://github.com/OpenEtherCATsociety/SOEM.git /usr/local/SOEM
cd /usr/local/SOEM
cmake --preset default
cmake --build --preset default
```

### 2. Compile Motor Control
```bash
cd /path/to/myactuator_ethercat_sdk
make
```

### 3. Find Network Interface
```bash
ip link show
# Look for interface connected to motor (e.g., eth0, enp0s3)
```

### 4. Run
```bash
sudo ./motor_control eth0
```

## Expected Behavior

Motor should:
1. Transition through states automatically
2. Enable when status reaches 0x1237
3. Spin at 10 RPM (21,845 pulses/second)
4. Show position changes in output

Press Ctrl+C to stop.

## One-Liner Setup

```bash
# Complete setup and build
sudo apt install -y build-essential cmake git && \
git clone https://github.com/OpenEtherCATsociety/SOEM.git /usr/local/SOEM && \
cd /usr/local/SOEM && cmake --preset default && cmake --build --preset default && \
cd - && make && \
echo "Done! Run: sudo ./motor_control <interface>"
```

## Change Speed

Edit `motor_control.c`, line 57:
```c
#define TARGET_RPM 10  // Change to desired RPM
```

Then rebuild:
```bash
make clean && make
```

## Troubleshooting

**"No slaves found":**
- Check EtherCAT cable connection
- Verify motor power is on
- Try different network interface

**"Permission denied":**
- Must use `sudo` for raw socket access

**Motor doesn't move:**
- Check motor power supply (separate from logic power)
- Verify actual_velocity increases in output
- Check for mechanical brake

## Why This Works on Linux (vs macOS/Python)

- ✅ SOEM uses **raw sockets** on Linux (more reliable)
- ✅ **Direct memory access** to PDO buffers
- ✅ **Better real-time** performance
- ✅ **Well-tested** platform for SOEM
- ✅ **No buffer freezing** issues like pysoem on macOS

For humanoid control, Linux + SOEM is the industry-standard approach.
