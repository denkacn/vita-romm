#!/usr/bin/env bash
set -euo pipefail

LINUX_USER="${1:-denkacn}"
PROJECT_DIR="/home/${LINUX_USER}/vita-projects/hello-vita"

install -d -o "${LINUX_USER}" -g "${LINUX_USER}" "${PROJECT_DIR}"

cat >"${PROJECT_DIR}/main.c" <<'EOF'
#include <psp2/kernel/processmgr.h>

int main(void) {
    sceKernelDelayThread(5 * 1000 * 1000);
    sceKernelExitProcess(0);
    return 0;
}
EOF

cat >"${PROJECT_DIR}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK")
  endif()
endif()

project(hello_vita C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Hello Vita")
set(VITA_TITLEID "ROMM00001")
set(VITA_VERSION "01.00")

add_executable(${PROJECT_NAME}
  main.c
)

target_link_libraries(${PROJECT_NAME}
  SceLibKernel_stub
)

vita_create_self(eboot.bin ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
EOF

chown -R "${LINUX_USER}:${LINUX_USER}" "${PROJECT_DIR}"

su - "${LINUX_USER}" -c "cd '${PROJECT_DIR}' && export VITASDK=/usr/local/vitasdk && export PATH=/usr/local/vitasdk/bin:\$PATH && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j\$(nproc)"

echo "PROJECT_DIR=${PROJECT_DIR}"
echo "VPK=${PROJECT_DIR}/build/hello_vita.vpk"
