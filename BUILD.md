# Elegoo Centauri Carbon 2 Firmware Build Process

This document describes the build process for the Elegoo Centauri Carbon 2 (CC2) firmware based on the contents of this repository.

## Overview

The firmware consists of three main components:
1.  **MCU Firmware:** A Klipper-based firmware that runs on the printer's microcontrollers (e.g., GD32 or STM32 variants).
2.  **Application Software (`elegoo_printer`):** A C++ application handling the main printer logic, kinematics, and UI interactions.
3.  **Update Packaging:** Scripts to bundle the application into an `.ipk` (opkg) package format, which is typically installed onto the host OS running on the DSP/Application Processor.

*Note: This repository produces an `.ipk` package and `.bin` MCU firmwares. It does **not** contain the necessary files to assemble a full `.swu` (SWUpdate) system image.*

---

## 1. MCU Firmware Build (`mcu_build.sh`)

The `mcu_build.sh` script is responsible for building the bootloader and the main application for the microcontrollers, and then merging them into a single `factory.bin` file.

**Inputs:**
*   **Board Configuration:** Needs a board identifier (e.g., `mcu_gd32`, `bed_sensor`) passed via the `-b` flag.
*   **Version:** A version string passed via the `-v` flag (e.g., `00.00.00.02`).
*   **Klipper Source:** The `src/` and `lib/` directories contain the core Klipper firmware source code.

**Process:**
1.  The script copies a configuration file from `mcu_config/<board>.config` to `.config` (for the app) and `mcu_config/<board>_bootloader.config` (if building the bootloader).
2.  It updates the `MCU_VERSION` macro in `src/version.h`.
3.  It runs `make -j16` using the root `Makefile` to compile the firmware.
4.  It uses Python scripts (e.g., `scripts/firmware_dump_timestamp.py`) to timestamp the binaries.
5.  If the `-f` or `-m` flags are provided, it uses `scripts/firmware_merge.py` to combine the bootloader `.bin` and the application `.bin` into a final `<board>-<timestamp>-<version>-factory.bin` file in the `out/` directory.

**Missing Pieces for this step:**
*   The `mcu_config/` directory containing the `.config` templates for the different boards is missing from this repository.
*   The `scripts/` directory containing `firmware_dump_timestamp.py` and `firmware_merge.py` is missing.

---

## 2. Main Application Build (`elegoo/`)

The core printer application is written in C++ and located in the `elegoo/` directory.

**Process:**
The application is built using CMake. While the top-level scripts aren't fully detailed in the root, typically you would run `elegoo/build.sh` or `elegoo/build_x64.sh` (for local development/testing). This compiles the C++ source files (`printer.cpp`, `gcode.cpp`, `stepper.cpp`, etc.) into an executable named `elegoo_printer` and a debug utility `debug_window`, placing them in `elegoo/build/`.

---

## 3. IPK Packaging (`ipkg-build/`)

Once the main application is built, it needs to be packaged into an installable format for the printer's Linux host system.

**Inputs:**
*   Compiled binaries from `elegoo/build/` (`elegoo_printer`, `debug_window`, `ai_camera`).
*   Shared libraries from `elegoo/lib/` (e.g., `libhv`, `ffmpeg`).
*   Configuration files from `debugfile/`.
*   Daemon scripts from a `daemon-000/` directory.

**Process:**
1.  **Staging (`elegoo-build.sh`):** This script creates a staging environment in `ipkg-build/source/data/`. It copies the executables to `bin/`, libraries to `lib/`, and configuration/daemon files to `inst/`. It also sets up `run_printer.sh` (choosing a DSP-specific version if requested).
2.  **Packaging (`ipkg-build.sh`):** This script is the main entry point. It calls `elegoo-build.sh` to stage the files. Then, it creates a `control.tar.gz` (from `ipkg-build/source/control/`) and a `data.tar.gz` (from `ipkg-build/source/data/`). Finally, it bundles these along with a `debian-binary` file into an `.ipk` package named `<package_name>_<timestamp>.ipk` in the `ipkg-build/output/` directory.

**Missing Pieces for this step:**
*   The `daemon-000/` directory, referenced by `elegoo-build.sh`, is not present in the repository root.
*   The `ipkg-build/source/control/` directory (containing the `control` file with metadata for the IPK) must exist prior to running the packaging script, but it is not tracked in the provided file tree.

---

## 4. SWU Generation (Missing)

An `.swu` file is an SWUpdate package used for full system or robust OTA updates. **This repository does not contain the scripts or configurations to generate an `.swu` file.**

The included script `ipkg-build/update_ipk.sh` is an *on-device* update script. It is designed to run on the printer itself. It handles:
*   Installing `.ipk` files using the `opkg` package manager.
*   Invoking the `swupdate` tool if an `.swu` file is provided (e.g., `swupdate -i <file.swu> -e stable,now_A_next_B`), managing A/B partition updates.

To build an `.swu` file, you would typically need a separate build system (like Yocto or Buildroot) or a standalone packaging script that utilizes a `sw-description` file to define how the root filesystem, kernel, and application packages (`.ipk`) are written to the device partitions.
