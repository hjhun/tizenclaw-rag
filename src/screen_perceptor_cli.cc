/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#include "tizenclaw_ocr_api.h"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>

#include <Ecore.h>
#include <Ecore_Wl2.h>
#include <screen_connector_toolkit.h>
#include <tbm_surface.h>

std::mutex g_mutex;
std::atomic<bool> g_captured{false};
void* g_toolkit_handle = nullptr;

struct CapturedFrame {
  std::string appid;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> data;
};

CapturedFrame g_frame;

bool ExtractPixels(struct wl_buffer* tbm, CapturedFrame& frame) {
  if (!tbm) {
    std::cerr << "ExtractPixels: tbm is null\n";
    return false;
  }

  auto surface = reinterpret_cast<tbm_surface_h>(tbm);
  tbm_surface_info_s info;
  if (tbm_surface_map(surface, TBM_SURF_OPTION_READ, &info) != TBM_SURFACE_ERROR_NONE) {
    std::cerr << "ExtractPixels: tbm_surface_map failed\n";
    return false;
  }

  frame.width = info.width;
  frame.height = info.height;

  if (info.num_planes > 0 && info.planes[0].ptr != nullptr) {
    size_t size = info.planes[0].stride * info.height;
    frame.data.assign(info.planes[0].ptr, info.planes[0].ptr + size);
  }

  tbm_surface_unmap(surface);
  return !frame.data.empty();
}

void OnSurfaceAdded(const char* appid, const char* instance_id, const int /*pid*/, void* /*data*/) {
  if (!appid) return;
  std::cerr << "[DEBUG] OnSurfaceAdded: " << appid << "\n";
  // Force an update to get the buffer immediately!
  screen_connector_toolkit_update(appid, instance_id ? instance_id : "");
}

void OnSurfaceUpdated(struct tizen_remote_surface* /*trs*/,
                      uint32_t /*type*/,
                      struct wl_buffer* tbm,
                      int32_t /*img_file_fd*/,
                      uint32_t /*img_file_size*/,
                      uint32_t /*time*/,
                      struct wl_array* /*keys*/,
                      const char* appid,
                      const char* /*instance_id*/,
                      const int /*pid*/, void* /*data*/) {
  std::cerr << "OnSurfaceUpdated called for " << (appid ? appid : "unknown") << "\n";
  if (g_captured.load()) return;

  CapturedFrame frame;
  frame.appid = appid ? appid : "unknown";
  
  if (ExtractPixels(tbm, frame)) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_captured.load()) {
      g_frame = std::move(frame);
      g_captured.store(true);
      ecore_main_loop_quit();
    }
  }
}

int main(int argc, char** argv) {
  // Set Wayland environment manually if run from systemd / SDB
  setenv("XDG_RUNTIME_DIR", "/run", 0);
  setenv("WAYLAND_DISPLAY", "wayland-0", 0);

  ecore_init();
  ecore_wl2_init();

  auto wl2_display = ecore_wl2_display_connect(nullptr);
  if (!wl2_display) {
    std::cerr << "{\"error\": \"Failed to connect to Wayland display\"}\n";
    return 1;
  }

  screen_connector_toolkit_ops ops{};
  ops.added_cb = &OnSurfaceAdded;
  ops.updated_cb = &OnSurfaceUpdated;

  screen_connector_toolkit_init(SCREEN_CONNECTOR_SCREEN_TYPE_ALL);

  g_toolkit_handle = screen_connector_toolkit_add(
      &ops, "", SCREEN_CONNECTOR_SCREEN_TYPE_ALL, nullptr);

  if (!g_toolkit_handle) {
    std::cerr << "{\"error\": \"Failed to register screen_connector\"}\n";
    return 1;
  }

  // Timeout logic to prevent hanging
  std::thread timeout_thread([]() {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    if (!g_captured.load()) {
      std::cerr << "{\"error\": \"Timeout waiting for screen buffers\"}\n";
      ecore_main_loop_quit();
    }
  });

  ecore_main_loop_begin();

  if (timeout_thread.joinable()) {
    timeout_thread.join();
  }

  if (g_toolkit_handle) {
    screen_connector_toolkit_remove(g_toolkit_handle);
  }
  screen_connector_toolkit_fini(SCREEN_CONNECTOR_SCREEN_TYPE_ALL);

  ecore_wl2_display_disconnect(static_cast<Ecore_Wl2_Display*>(wl2_display));
  ecore_wl2_shutdown();
  ecore_shutdown();

  if (!g_captured.load()) {
    return 1;
  }

  std::cerr << "[INFO] Grabbed screen buffer: " << g_frame.width << "x" << g_frame.height << "\n";

  // Now run OCR
  void* ocr_engine = tizenclaw_ocr_create("/opt/usr/share/tizenclaw/models/ppocr");
  if (!ocr_engine) {
    std::cerr << "{\"error\": \"Failed to create OCR engine\"}\n";
    return 1;
  }

  char* ocr_result = tizenclaw_ocr_analyze_buffer(
      ocr_engine,
      g_frame.data.data(),
      g_frame.width,
      g_frame.height,
      4, // bytes per pixel
      1  // is BGRA
  );

  if (ocr_result) {
    // Print directly to stdout for consumers
    std::cout << ocr_result << "\n";
    free(ocr_result);
  } else {
    std::cerr << "{\"error\": \"OCR returned null pointer\"}\n";
  }

  tizenclaw_ocr_destroy(ocr_engine);
  return 0;
}
