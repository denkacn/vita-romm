cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK")
  endif()
endif()

project(vita_romm C)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Vita RomM")
set(VITA_TITLEID "ROMM00001")
set(VITA_VERSION "01.00")
set(ROMM_VITA_VERSION "0.7.1")

set(VITA_IP "" CACHE STRING "PS Vita IP address for FTP/vitacompanion deploy")
set(VITA_FTP_PORT "1337" CACHE STRING "PS Vita FTP port")

find_package(CURL REQUIRED)

add_executable(${PROJECT_NAME}
  src/main.c
  src/app.c
  src/app_error.c
  src/app_state.c
  src/commands.c
  src/config.c
  src/http_buffer.c
  src/input.c
  src/platforms.c
  src/vita_fs.c
  src/vita_paths.c
  src/installer.c
  src/installer_download.c
  src/installer_psx.c
  src/installer_zip.c
  src/install_status.c
  src/json_utils.c
  src/job_queue.c
  src/romm_parse_files.c
  src/romm_parse_platforms.c
  src/romm_parse_roms.c
  src/library_loader.c
  src/livearea.c
  src/net.c
  src/romm_client.c
  src/rom_paths.c
  src/cover_cache.c
  src/ui.c
  src/ui_resources.c
  src/ui_games.c
  src/ui_platforms.c
  src/ui_shell.c
  src/ui_font.c
  src/util.c
  src/zip_layout.c
)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall
  -Wextra
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  ROMM_VITA_VERSION="${ROMM_VITA_VERSION}"
)

target_link_libraries(${PROJECT_NAME}
  vita2d
  CURL::libcurl
  zip
  z
  bz2
  lzma
  zstd
  pthread
  png
  jpeg
  SceAppMgr_stub
  SceCommonDialog_stub
  SceCtrl_stub
  SceDisplay_stub
  SceGxm_stub
  SceHttp_stub
  SceLibKernel_stub
  SceNet_stub
  SceNetCtl_stub
  ScePgf_stub
  ScePaf_stub
  ScePromoterUtil_stub
  ScePvf_stub
  SceSsl_stub
  SceSysmodule_stub
  m
)

vita_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon0.png sce_sys/icon0.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/inter_atlas.png assets/fonts/inter_atlas.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_psvita.png assets/platforms/platform_psvita.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_psp.png assets/platforms/platform_psp.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_psx.png assets/platforms/platform_psx.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_gba.png assets/platforms/platform_gba.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_gb.png assets/platforms/platform_gb.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_gbc.png assets/platforms/platform_gbc.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_nes.png assets/platforms/platform_nes.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_snes.png assets/platforms/platform_snes.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_genesis.png assets/platforms/platform_genesis.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_n64.png assets/platforms/platform_n64.png
  FILE ${CMAKE_CURRENT_SOURCE_DIR}/assets/platforms/platform_fallback.png assets/platforms/platform_fallback.png
)

if(VITA_IP)
  add_custom_target(send-vpk
    COMMAND curl -T "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.vpk" "ftp://${VITA_IP}:${VITA_FTP_PORT}/ux0:data/"
    DEPENDS ${PROJECT_NAME}.vpk-vpk
    COMMENT "Uploading ${PROJECT_NAME}.vpk to VitaShell FTP ux0:data/"
  )

  add_custom_target(send-eboot
    COMMAND curl --ftp-create-dirs -T "${CMAKE_CURRENT_BINARY_DIR}/eboot.bin" "ftp://${VITA_IP}:${VITA_FTP_PORT}/ux0:app/${VITA_TITLEID}/eboot.bin"
    DEPENDS eboot.bin-self
    COMMENT "Uploading eboot.bin to installed app ${VITA_TITLEID}"
  )
endif()






















