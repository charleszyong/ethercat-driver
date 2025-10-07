/**
 * Minimal SOEM program to control MyActuator motor at 10 RPM
 * For Linux with SOEM library
 *
 * Compile:
 *   gcc motor_control.c -I/path/to/SOEM/soem -L/path/to/SOEM/build/lib -lsoem -pthread -lrt -o motor_control
 *
 * Run:
 *   sudo ./motor_control [network_interface]
 *
 *   Without interface argument: Auto-detects interface with EtherCAT slave
 *   With interface: Uses specified interface
 *
 *   Examples:
 *     sudo ./motor_control          # Auto-detect
 *     sudo ./motor_control eth0     # Use eth0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>
#include <net/if.h>
#include "ethercat.h"

// Motor vendor/product from ESI
#define MOTOR_VENDOR_ID  0x00202008
#define MOTOR_PRODUCT_ID 0x00000000

// PDO structures from ESI file (mt-device.xml)
typedef struct __attribute__((__packed__))
{
    uint16 control_word;       // 0x6040: Control Word (16-bit)
    int32  target_position;    // 0x607A: Target Position (32-bit)
    int32  target_velocity;    // 0x60FF: Target Velocity (32-bit)
    int16  target_torque;      // 0x6071: Target Torque (16-bit)
    uint16 max_torque;         // 0x6072: Max Torque (16-bit)
    int8   mode;               // 0x6060: Mode of Operation (8-bit)
    uint8  dummy;              // 0x5FFE: Dummy (8-bit)
} OutputPDO;

typedef struct __attribute__((__packed__))
{
    uint16 status_word;        // 0x6041: Status Word (16-bit)
    int32  actual_position;    // 0x6064: Actual Position (32-bit)
    int32  actual_velocity;    // 0x606C: Actual Velocity (32-bit)
    int16  actual_torque;      // 0x6077: Actual Torque (16-bit)
    uint16 error_code;         // 0x603F: Error Code (16-bit)
    int8   mode_display;       // 0x6061: Mode Display (8-bit)
    uint8  dummy;              // 0x5FFE: Dummy (8-bit)
} InputPDO;

// Global variables
static int run_flag = 1;
static char io_map[4096];
static ec_slavet ec_slave[EC_MAXSLAVE];
static int expected_wkc;

// Signal handler
void signal_handler(int sig)
{
    run_flag = 0;
    printf("\nStopping...\n");
}

/**
 * Auto-detect network interface with EtherCAT slave
 * Returns interface name in static buffer, or NULL if not found
 */
char* detect_ethercat_interface(void)
{
    static char detected_interface[16];
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;

    printf("Auto-detecting EtherCAT interface...\n");
    printf("================================\n");

    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return NULL;
    }

    // Try each interface
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        // Skip loopback and non-ethernet interfaces
        if (strncmp(ifa->ifa_name, "lo", 2) == 0)
            continue;
        if (strncmp(ifa->ifa_name, "veth", 4) == 0)
            continue;
        if (strncmp(ifa->ifa_name, "docker", 6) == 0)
            continue;
        if (strncmp(ifa->ifa_name, "br-", 3) == 0)
            continue;

        // Only try physical/ethernet interfaces
        if (!(ifa->ifa_flags & IFF_UP))
            continue;

        printf("  Trying %s... ", ifa->ifa_name);
        fflush(stdout);

        // Try to initialize EtherCAT on this interface
        if (ec_init(ifa->ifa_name))
        {
            // Try to find slaves
            int slaves = ec_config_init(FALSE);

            if (slaves > 0)
            {
                printf("✓ Found %d slave(s)!\n", slaves);
                printf("    Slave: %s\n", ec_slave[1].name);

                // Found it!
                strncpy(detected_interface, ifa->ifa_name, sizeof(detected_interface) - 1);
                detected_interface[sizeof(detected_interface) - 1] = '\0';
                found = 1;

                ec_close();
                break;
            }
            else
            {
                printf("no slaves\n");
                ec_close();
            }
        }
        else
        {
            printf("failed to open\n");
        }
    }

    freeifaddrs(ifaddr);

    if (found)
    {
        printf("\n✓ Detected interface: %s\n", detected_interface);
        return detected_interface;
    }

    return NULL;
}

// Calculate 10 RPM in pulses/second
// Formula from manual: RPM = (pulses * 60) / 131072
// Therefore: pulses = RPM * 131072 / 60
#define TARGET_RPM 10
#define TARGET_VELOCITY ((TARGET_RPM * 131072) / 60)  // = 21845 pulses/s

int main(int argc, char *argv[])
{
    int wkc;
    int slave_count;
    char *ifname;

    OutputPDO *output_pdo;
    InputPDO *input_pdo;

    int cycle_count = 0;
    int motor_enabled = 0;
    int32 start_position = 0;

    // Setup signal handler
    signal(SIGINT, signal_handler);

    // Auto-detect or use specified interface
    if (argc < 2)
    {
        // No interface specified - auto-detect
        printf("No interface specified, auto-detecting...\n\n");
        ifname = detect_ethercat_interface();

        if (ifname == NULL)
        {
            printf("\n❌ No EtherCAT interface found!\n");
            printf("\nTroubleshooting:\n");
            printf("  - Check EtherCAT cable is connected\n");
            printf("  - Verify motor power is on\n");
            printf("  - Try specifying interface manually: %s <interface>\n", argv[0]);
            printf("  - List interfaces: ip link show\n");
            return 1;
        }

        printf("\n");
    }
    else
    {
        // Use specified interface
        ifname = argv[1];
        printf("Using specified interface: %s\n", ifname);
    }

    printf("MyActuator Motor Control - SOEM\n");
    printf("================================\n");
    printf("Network interface: %s\n", ifname);
    printf("Target: %d RPM (%d pulses/s)\n", TARGET_RPM, TARGET_VELOCITY);
    printf("================================\n\n");

    // Initialize SOEM
    if (ec_init(ifname))
    {
        printf("✓ SOEM initialized on %s\n", ifname);

        // Find and configure slaves
        if (ec_config_init(FALSE) > 0)
        {
            slave_count = ec_slavecount;
            printf("✓ Found %d slave(s)\n", slave_count);

            if (slave_count == 0)
            {
                printf("No slaves found!\n");
                ec_close();
                return 1;
            }

            printf("✓ Motor: %s\n", ec_slave[1].name);

            // Configure Distributed Clock with 2ms cycle (2,000,000 ns)
            ec_configdc();
            printf("✓ DC configured\n");

            // Map PDO
            ec_config_map(&io_map);
            printf("✓ PDO mapped\n");

            // Configure DC sync on slave 1 with 2ms cycle
            ec_dcsync0(1, TRUE, 2000000U, 0);
            printf("✓ DC sync activated (2ms cycle)\n");

            // Wait for all slaves to reach SAFE-OP
            ec_statecheck(0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);

            printf("✓ SAFE-OP state\n");

            // Get PDO pointers
            output_pdo = (OutputPDO *)(ec_slave[1].outputs);
            input_pdo = (InputPDO *)(ec_slave[1].inputs);

            // Initialize output PDO
            memset(output_pdo, 0, sizeof(OutputPDO));
            output_pdo->mode = 9;           // CSV mode
            output_pdo->max_torque = 1000;  // Max torque

            // Set interpolation time period (0x60C2:01) to 2ms
            printf("\nSetting interpolation period...\n");
            int8 interp_period = 2;  // 2ms
            int wkc_sdo = ec_SDOwrite(1, 0x60C2, 0x01, FALSE, sizeof(interp_period), &interp_period, EC_TIMEOUTRXM);
            if (wkc_sdo > 0)
                printf("  ✓ Interpolation period set to %d ms\n", interp_period);
            else
                printf("  Warning: Could not set interpolation period\n");

            // Send initial PDO
            ec_send_processdata();
            wkc = ec_receive_processdata(EC_TIMEOUTRET);

            // Transition to OP state
            ec_slave[0].state = EC_STATE_OPERATIONAL;
            ec_writestate(0);

            // Wait for OP state
            int wait_count = 0;
            do
            {
                ec_send_processdata();
                ec_receive_processdata(EC_TIMEOUTRET);
                ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
                wait_count++;
            }
            while ((ec_slave[0].state != EC_STATE_OPERATIONAL) && (wait_count < 100));

            if (ec_slave[0].state == EC_STATE_OPERATIONAL)
            {
                printf("✓ OP state\n\n");

                expected_wkc = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
                printf("Expected WKC: %d\n", expected_wkc);

                printf("\n");
                printf("================================\n");
                printf("REACTIVE STATE MACHINE\n");
                printf("================================\n");
                printf("Status 0x1231 → Control 0x07\n");
                printf("Status 0x1233 → Control 0x0F\n");
                printf("Status 0x1237 → Send velocity\n");
                printf("================================\n\n");

                // Main cyclic loop
                struct timespec sleep_time;
                sleep_time.tv_sec = 0;
                sleep_time.tv_nsec = 2000000;  // 2ms

                while (run_flag)
                {
                    // Send process data
                    ec_send_processdata();

                    // Receive process data
                    wkc = ec_receive_processdata(EC_TIMEOUTRET);

                    // Reactive state machine (like IgH example)
                    uint16 status = input_pdo->status_word;

                    if (status == 0x1208)  // Fault
                    {
                        output_pdo->control_word = 0x80;  // Fault reset
                        output_pdo->target_velocity = 0;
                    }
                    else if (status == 0x1250)  // Switch on disabled
                    {
                        output_pdo->control_word = 0x06;  // Shutdown
                        output_pdo->target_velocity = 0;
                    }
                    else if (status == 0x1231)  // Ready to switch on
                    {
                        output_pdo->control_word = 0x07;  // Switch on
                        output_pdo->target_velocity = 0;
                    }
                    else if (status == 0x1233)  // Switched on
                    {
                        output_pdo->control_word = 0x0F;  // Enable operation
                        output_pdo->target_velocity = 0;
                    }
                    else if (status == 0x1237 || status == 0x1637)  // Operation enabled
                    {
                        output_pdo->control_word = 0x0F;  // Keep enabled
                        output_pdo->target_velocity = TARGET_VELOCITY;  // 10 RPM

                        if (!motor_enabled)
                        {
                            motor_enabled = 1;
                            start_position = input_pdo->actual_position;
                            printf("\n🎉 Motor ENABLED! (Status: 0x%04X)\n", status);
                            printf("   Starting position: %d\n\n", start_position);
                        }
                    }

                    // Always maintain mode and max torque
                    output_pdo->mode = 9;           // CSV mode
                    output_pdo->max_torque = 1000;  // Max torque

                    cycle_count++;

                    // Print status every 500 cycles (~1 second at 2ms/cycle)
                    if (cycle_count % 500 == 0)
                    {
                        double actual_rpm = (input_pdo->actual_velocity * 60.0) / 131072.0;
                        int32 pos_delta = input_pdo->actual_position - start_position;

                        printf("[%6d] Status: 0x%04X | Control: 0x%02X | "
                               "Pos: %10d (Δ%+10d) | "
                               "Vel: %7.2f RPM (%6d p/s) | "
                               "Mode: %d | WKC: %d/%d\n",
                               cycle_count,
                               input_pdo->status_word,
                               output_pdo->control_word,
                               input_pdo->actual_position,
                               pos_delta,
                               actual_rpm,
                               input_pdo->actual_velocity,
                               input_pdo->mode_display,
                               wkc,
                               expected_wkc);

                        if (abs(pos_delta) > 1000)
                        {
                            printf("         🎉 MOTOR IS MOVING! Moved %d counts!\n", pos_delta);
                        }
                    }

                    // Sleep for 2ms cycle time
                    nanosleep(&sleep_time, NULL);
                }

                // Stop motor
                printf("\nStopping motor...\n");
                output_pdo->control_word = 0;
                output_pdo->target_velocity = 0;

                for (int i = 0; i < 50; i++)
                {
                    ec_send_processdata();
                    ec_receive_processdata(EC_TIMEOUTRET);
                    nanosleep(&sleep_time, NULL);
                }

                printf("✓ Motor stopped\n");
            }
            else
            {
                printf("Failed to reach OP state\n");
            }
        }
        else
        {
            printf("No slaves found!\n");
        }

        // Close SOEM
        ec_close();
        printf("\n✓ SOEM closed\n");
    }
    else
    {
        printf("Failed to initialize SOEM on %s\n", ifname);
        printf("Try: sudo ./motor_control %s\n", ifname);
        printf("Or check if interface exists: ip link show\n");
        return 1;
    }

    return 0;
}
