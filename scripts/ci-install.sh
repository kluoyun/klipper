#!/bin/bash
# Build setup script for continuous integration testing.
# See ci-build.sh for the actual test steps.

# Stop script early on any error; check variables; be verbose
set -eux

MAIN_DIR=${PWD}
BUILD_DIR=${PWD}/ci_build
CACHE_DIR=${PWD}/ci_cache
mkdir -p ${BUILD_DIR} ${CACHE_DIR}


######################################################################
# Install system dependencies
######################################################################

echo -e "\n\n=============== Install system dependencies\n\n"
PKGS="virtualenv python2-dev libffi-dev build-essential"
PKGS="${PKGS} gcc-avr avr-libc"
PKGS="${PKGS} libnewlib-arm-none-eabi gcc-arm-none-eabi binutils-arm-none-eabi"
sudo apt-get update
sudo apt-get install ${PKGS}


######################################################################
# Install pru gcc
######################################################################

echo -e "\n\n=============== Install embedded pru gcc\n\n"
PRU_ARCHIVE="pru-elf-2024.05.amd64.tar.xz"
PRU_URL="https://github.com/dinuxbg/gnupru/releases/download/2024.05/${PRU_ARCHIVE}"

if [ ! -f ${CACHE_DIR}/${PRU_ARCHIVE} ]; then
    wget "${PRU_URL}" -O "${CACHE_DIR}/${PRU_ARCHIVE}"
fi
cd ${BUILD_DIR}
tar xJf ${CACHE_DIR}/${PRU_ARCHIVE}


######################################################################
# Install or1k toolchain
######################################################################

echo -e "\n\n=============== Install or1k toolchain\n\n"
OR1K_ARCHIVE="or1k-elf-12.0.1-20220210-20220304.tar.xz"
OR1K_REL="or1k-12.0.1-20220210-20220304"
OR1K_URL="https://github.com/openrisc/or1k-gcc/releases/download/${OR1K_REL}/${OR1K_ARCHIVE}"
if [ ! -f ${CACHE_DIR}/${OR1K_ARCHIVE} ]; then
    wget "${OR1K_URL}" -O "${CACHE_DIR}/${OR1K_ARCHIVE}"
fi
cd ${BUILD_DIR}
tar xJf ${CACHE_DIR}/${OR1K_ARCHIVE}


######################################################################
# Install Espressif toolchains
######################################################################

echo -e "\n\n=============== Install Espressif toolchains\n\n"

ESP_GCC_VERSION="15.2.0_20251204"
ESP_GCC_RELEASE="esp-${ESP_GCC_VERSION}"
ESP_GCC_URL="https://github.com/espressif/crosstool-NG/releases/download/${ESP_GCC_RELEASE}"
for ESP_TARGET in riscv32-esp-elf xtensa-esp-elf ; do
    ESP_ARCHIVE="${ESP_TARGET}-${ESP_GCC_VERSION}-x86_64-linux-gnu.tar.xz"
    if [ ! -f ${CACHE_DIR}/${ESP_ARCHIVE} ]; then
        wget "${ESP_GCC_URL}/${ESP_ARCHIVE}" -O "${CACHE_DIR}/${ESP_ARCHIVE}"
    fi
    cd ${BUILD_DIR}
    tar xJf ${CACHE_DIR}/${ESP_ARCHIVE}
done


######################################################################
# Create python3 virtualenv environment
######################################################################

echo -e "\n\n=============== Install python3 virtualenv\n\n"
cd ${MAIN_DIR}
virtualenv -p python3 ${BUILD_DIR}/python-env
${BUILD_DIR}/python-env/bin/pip install -r ${MAIN_DIR}/scripts/klippy-requirements.txt
${BUILD_DIR}/python-env/bin/pip install -r ${MAIN_DIR}/scripts/tests-requirements.txt


######################################################################
# Create python2 virtualenv environment
######################################################################

echo -e "\n\n=============== Install python2 virtualenv\n\n"
cd ${MAIN_DIR}
virtualenv -p python2 ${BUILD_DIR}/python2-env
${BUILD_DIR}/python2-env/bin/pip install -r ${MAIN_DIR}/scripts/klippy-requirements.txt
${BUILD_DIR}/python2-env/bin/pip install -r ${MAIN_DIR}/scripts/tests-requirements.txt
