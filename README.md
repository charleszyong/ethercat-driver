# MyActuator EtherCAT Motor Control

Minimal SOEM C program to control MyActuator motor at 10 RPM using EtherCAT CSV mode (Cyclic Synchronous Velocity).

## For Linux (Recommended for Humanoid/Production Use)

### Prerequisites

```bash
sudo apt update
sudo apt install build-essential cmake git
```

### Installation

1. **Install SOEM library:**

```bash
# Clone SOEM
git clone https://github.com/OpenEtherCATsociety/SOEM.git /usr/local/SOEM

# Build SOEM
cd /usr/local/SOEM
cmake --preset default
cmake --build --preset default
```

Or use the Makefile:
```bash
make install-soem
```

2. **Compile motor control program:**

Edit `Makefile` if your SOEM is installed in a different location, then:

```bash
make
```

This creates the `motor_control` executable.

### Usage

1. **Find your network interface:**
```bash
ip link show
```

Look for your EtherCAT network interface (e.g., `eth0`, `enp0s3`, etc.)

2. **Run the motor control:**
```bash
sudo ./motor_control <interface>
```

Example:
```bash
sudo ./motor_control eth0
```

3. **Stop the motor:**
Press `Ctrl+C` to gracefully stop the motor.

### What It Does

1. **Initializes EtherCAT** on specified network interface
2. **Scans for motor** (MyActuator MT_Device)
3. **Configures DC sync** with 2ms cycle time
4. **Sets CSV mode** (mode 9 for velocity control)
5. **Uses reactive state machine**:
   - Reads status word (0x6041) every cycle
   - Sends appropriate control word (0x6040) based on status
   - When enabled (status 0x1237), sends target velocity
6. **Runs at 10 RPM** (21,845 pulses/second)
7. **Prints status** every second showing position, velocity, mode

### Key Features

- **Reactive State Machine**: Adapts to actual motor state every cycle
- **DC Synchronization**: 2ms cycle time (500 Hz)
- **CSV Mode**: Direct velocity control (mode 9)
- **CiA 402 Compliant**: Standard CANopen drive profile
- **10 RPM Target**: Configurable via `TARGET_RPM` define

### PDO Structure

Based on ESI file (`esi_files/mt-device.xml`):

**Output (RxPDO - 16 bytes):**
- Control Word (0x6040): 16-bit
- Target Position (0x607A): 32-bit
- Target Velocity (0x60FF): 32-bit
- Target Torque (0x6071): 16-bit
- Max Torque (0x6072): 16-bit
- Mode (0x6060): 8-bit
- Dummy: 8-bit

**Input (TxPDO - 16 bytes):**
- Status Word (0x6041): 16-bit
- Actual Position (0x6064): 32-bit
- Actual Velocity (0x606C): 32-bit
- Actual Torque (0x6077): 16-bit
- Error Code (0x603F): 16-bit
- Mode Display (0x6061): 8-bit
- Dummy: 8-bit

### Troubleshooting

**No slaves found:**
```bash
# Check network interface exists
ip link show

# Check if EtherCAT cable is connected
# Verify motor power is on
```

**Permission denied:**
```bash
# Must run with sudo for raw socket access
sudo ./motor_control eth0
```

**Motor doesn't move:**
- Check motor power supply is connected (separate from logic power)
- Verify encoder is connected
- Check for mechanical brake
- Monitor actual velocity in output - should increase towards target

**Compilation errors:**
- Verify SOEM path in Makefile matches your installation
- Check SOEM was built successfully: `ls /usr/local/SOEM/build/lib/libsoem.a`

### Adjusting Speed

To change target RPM, edit `motor_control.c`:

```c
#define TARGET_RPM 10  // Change this value
```

Then recompile:
```bash
make clean
make
```

### Documentation

- **Manual**: `manuals/ethercat_cn.md` - Chinese manual with protocol details
- **ESI File**: `esi_files/mt-device.xml` - Device configuration
- **SOEM Docs**: https://openethercatsociety.github.io/doc/soem/

### Hardware Setup

1. Connect motor to computer via EtherCAT cable (RJ45)
2. Ensure motor power supply is connected and on
3. Identify network interface connected to motor
4. Run program with that interface

### Output Example

```
MyActuator Motor Control - SOEM
================================
Network interface: eth0
Target: 10 RPM (21845 pulses/s)
================================

✓ SOEM initialized on eth0
✓ Found 1 slave(s)
✓ Motor: MT_Device
✓ DC configured
✓ PDO mapped
✓ DC sync activated (2ms cycle)
✓ SAFE-OP state
✓ Interpolation period set to 2 ms
✓ OP state

Expected WKC: 3

🎉 Motor ENABLED! (Status: 0x1237)
   Starting position: 12345

[   500] Status: 0x1237 | Control: 0x0F | Pos:      23456 (Δ    +11111) | Vel:   9.87 RPM ( 21500 p/s) | Mode: 9 | WKC: 3/3
         🎉 MOTOR IS MOVING! Moved 11111 counts!
```

### License

This code is provided as-is for controlling MyActuator motors. Based on SOEM (Simple Open EtherCAT Master) library.

### Notes

- Designed for Linux (Ubuntu/Debian tested)
- Real-time kernel recommended for humanoid applications
- For production, consider adding error handling and safety checks
- Adjust `max_torque` in code if needed (currently 1000 = 100% of rated current)
