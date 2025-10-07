# Makefile for MyActuator EtherCAT Motor Control
# Requires SOEM library installed

# Adjust these paths to match your SOEM installation
SOEM_DIR = /usr/local/SOEM
SOEM_INCLUDE = $(SOEM_DIR)/soem
SOEM_LIB = $(SOEM_DIR)/build/lib

# Compiler and flags
CC = gcc
CFLAGS = -Wall -O2 -I$(SOEM_INCLUDE)
LDFLAGS = -L$(SOEM_LIB) -lsoem -pthread -lrt

# Target
TARGET = motor_control
SOURCE = motor_control.c

# Build rule
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) $(LDFLAGS) -o $(TARGET)
	@echo ""
	@echo "✓ Compiled successfully!"
	@echo "Run with: sudo ./$(TARGET) <network_interface>"
	@echo "Example: sudo ./$(TARGET) eth0"

# Clean rule
clean:
	rm -f $(TARGET)

# Install SOEM (for convenience)
install-soem:
	@echo "Installing SOEM..."
	@if [ ! -d "$(SOEM_DIR)" ]; then \
		echo "Cloning SOEM..."; \
		git clone https://github.com/OpenEtherCATsociety/SOEM.git $(SOEM_DIR); \
		cd $(SOEM_DIR) && cmake --preset default && cmake --build --preset default; \
		echo "✓ SOEM installed to $(SOEM_DIR)"; \
	else \
		echo "SOEM already exists at $(SOEM_DIR)"; \
	fi

.PHONY: clean install-soem
