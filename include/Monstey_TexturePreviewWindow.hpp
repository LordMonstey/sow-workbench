#pragma once
#include "Monstey_Core.hpp"
#include <memory>
#include <windows.h>
namespace Monstey {
// UI-thread owned, modeless native CPU texture view. Closing the HWND cancels
// queued work and invalidates completions immediately. Destruction drains the
// active owned worker before releasing its state, like the Workbench worker.
class Monstey_TexturePreviewWindow {
public:
  Monstey_TexturePreviewWindow();
  ~Monstey_TexturePreviewWindow();
  Monstey_TexturePreviewWindow(const Monstey_TexturePreviewWindow &) = delete;
  Monstey_TexturePreviewWindow &
  operator=(const Monstey_TexturePreviewWindow &) = delete;
  std::uint64_t Open(HWND owner, const std::filesystem::path &source,
                     std::string_view expected_sha256 = {},
                     int show = SW_SHOWNORMAL);
  HWND Handle() const;
  void Close();
  bool RouteMessage(MSG &message);
  // New exclusive offline PNG, from the current pinned subresource and depth.
  // Runs on the same worker; results are reported through Inspect/status text.
  bool SavePng(const std::filesystem::path &new_output);
  Monstey_Json Inspect() const;
  // BMP of this component's client area via WM_PRINT, never the desktop.
  Monstey_Bytes Snapshot();

private:
  struct Monstey_State;
  std::unique_ptr<Monstey_State> state_;
};
} // namespace Monstey