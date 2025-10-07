#!/usr/bin/env python3
"""
Auto-detect network interface with EtherCAT slave connected
Scans all available interfaces and identifies which has the motor
"""

import sys
import subprocess
import pysoem


def get_network_interfaces():
    """Get list of available network interfaces"""
    interfaces = []

    try:
        # Linux
        result = subprocess.run(['ip', 'link', 'show'],
                              capture_output=True,
                              text=True,
                              check=False)

        if result.returncode == 0:
            # Parse ip link output
            for line in result.stdout.split('\n'):
                if ':' in line and 'state' in line.lower():
                    parts = line.split(':')
                    if len(parts) >= 2:
                        iface = parts[1].strip()
                        # Skip loopback and virtual interfaces
                        if not iface.startswith('lo') and not iface.startswith('veth'):
                            interfaces.append(iface)
        else:
            # Try macOS/BSD
            result = subprocess.run(['ifconfig'],
                                  capture_output=True,
                                  text=True,
                                  check=False)

            for line in result.stdout.split('\n'):
                if ':' in line and 'flags=' in line:
                    iface = line.split(':')[0]
                    # Common Ethernet interfaces
                    if iface.startswith('en') or iface.startswith('eth'):
                        interfaces.append(iface)

    except Exception as e:
        print(f"Error detecting interfaces: {e}")
        # Fallback to common names
        interfaces = ['eth0', 'eth1', 'enp0s3', 'en0', 'en9']

    return interfaces


def detect_ethercat_interface():
    """Detect which interface has EtherCAT slave"""

    print("EtherCAT Interface Detection")
    print("=" * 60)

    interfaces = get_network_interfaces()
    print(f"Found {len(interfaces)} network interface(s): {interfaces}\n")

    if not interfaces:
        print("No network interfaces found!")
        return None

    # Try each interface
    for interface in interfaces:
        print(f"Trying {interface}...", end=" ")

        master = None
        try:
            master = pysoem.Master()
            master.open(interface)

            # Try to find slaves
            slave_count = master.config_init()

            if slave_count > 0:
                print(f"✓ Found {slave_count} slave(s)!")

                # Print slave info
                for i, slave in enumerate(master.slaves):
                    print(f"  Slave {i}: {slave.name}")
                    print(f"    Vendor: 0x{slave.man:08X}")
                    print(f"    Product: 0x{slave.id:08X}")

                # Close and return interface name
                master.close()
                return interface
            else:
                print("no slaves")
                master.close()

        except Exception as e:
            print(f"error ({e})")
            if master:
                try:
                    master.close()
                except:
                    pass

    print("\nNo EtherCAT slaves found on any interface!")
    return None


def main():
    """Main function"""

    interface = detect_ethercat_interface()

    print("\n" + "=" * 60)

    if interface:
        print(f"✅ EtherCAT interface detected: {interface}")
        print("\nUse this in your code:")
        print(f"  master.open('{interface}')")
        return 0
    else:
        print("❌ No EtherCAT interface found")
        print("\nTroubleshooting:")
        print("  - Check EtherCAT cable is connected")
        print("  - Verify motor power is on")
        print("  - Try: sudo python3 detect_ethercat_interface.py")
        print("  - Check available interfaces: ip link show (Linux) or ifconfig (macOS)")
        return 1


if __name__ == "__main__":
    sys.exit(main())
