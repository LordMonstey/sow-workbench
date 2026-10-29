#include "Monstey_Theme.hpp"
#include "Monstey_TexturePreviewWindow.hpp"
#include "Monstey_TextureApi.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <commctrl.h>
#include <commdlg.h>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iomanip>
#include <locale>
#include <mutex>
#include <sstream>
#include <thread>
#include <windowsx.h>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
constexpr wchar_t Monstey_TextureClass[] = L"Monstey_SoWNativeTexturePreview";
constexpr UINT_PTR Monstey_TextureTimer = 41;
constexpr int Monstey_TextureSelection = 100, Monstey_TextureDepth = 101,
              Monstey_TextureApplyDepth = 102, Monstey_TextureFit = 103,
              Monstey_TextureOne = 104, Monstey_TextureMinus = 105,
              Monstey_TexturePlus = 106, Monstey_TextureAlpha = 107,
              Monstey_TextureSave = 108, Monstey_TextureClose = 109,
              Monstey_TextureSource = 110, Monstey_TextureFormat = 111,
              Monstey_TextureHash = 112, Monstey_TextureStatus = 113,
              Monstey_TextureHover = 114;
constexpr std::uint64_t Monstey_TexturePixelBudget = 16777216;
std::uint8_t Monstey_TextureQuantize(float v) {
  return std::isnan(v) ? 0
                       : static_cast<std::uint8_t>(
                             std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
}
std::wstring Monstey_TextureLabel(const Monstey_TextureSubresource &s,
                                  std::size_t ordinal) {
  return L"#" + std::to_wstring(ordinal) + L"  Mip " + std::to_wstring(s.mip) +
         L" | Array " + std::to_wstring(s.array_slice) + L" | Face " +
         std::to_wstring(s.cube_face) + L" | " + std::to_wstring(s.width) +
         L" x " + std::to_wstring(s.height) + L" x " + std::to_wstring(s.depth);
}
RECT Monstey_TextureWorkArea(HWND owner, const RECT *proposed = nullptr) {
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  const auto monitor = proposed
                           ? MonitorFromRect(proposed, MONITOR_DEFAULTTONEAREST)
                           : MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY);
  if (GetMonitorInfoW(monitor, &info))
    return info.rcWork;
  RECT work{};
  Monstey_Require(SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) != FALSE,
                  "Cannot read texture work area");
  return work;
}
RECT Monstey_TextureClamp(RECT proposed, HWND owner,
                          bool proposedMonitor = false) {
  const auto work =
      Monstey_TextureWorkArea(owner, proposedMonitor ? &proposed : nullptr);
  const auto width = std::clamp(proposed.right - proposed.left, 1L,
                                work.right - work.left),
             height = std::clamp(proposed.bottom - proposed.top, 1L,
                                 work.bottom - work.top);
  const auto x = std::clamp(proposed.left, work.left, work.right - width),
             y = std::clamp(proposed.top, work.top, work.bottom - height);
  return {x, y, x + width, y + height};
}
struct Monstey_TexturePaint {
  HWND window;
  PAINTSTRUCT ps{};
  HDC dc;
  explicit Monstey_TexturePaint(HWND w) : window(w), dc(BeginPaint(w, &ps)) {}
  ~Monstey_TexturePaint() { EndPaint(window, &ps); }
};
struct Monstey_TextureBitmapDC {
  HWND window;
  HDC reference = nullptr, memory = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old = nullptr;
  void *pixels = nullptr;
  Monstey_TextureBitmapDC(HWND w, int width, int height) : window(w) {
    reference = GetDC(w);
    if (reference)
      memory = CreateCompatibleDC(reference);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    if (memory)
      bitmap = CreateDIBSection(reference, &info, DIB_RGB_COLORS, &pixels,
                                nullptr, 0);
    if (bitmap)
      old = SelectObject(memory, bitmap);
  }
  ~Monstey_TextureBitmapDC() {
    if (old && old != HGDI_ERROR)
      SelectObject(memory, old);
    if (bitmap)
      DeleteObject(bitmap);
    if (memory)
      DeleteDC(memory);
    if (reference)
      ReleaseDC(window, reference);
  }
};
B Monstey_TextureCapture(HWND hwnd) {
  RECT r{};
  GetClientRect(hwnd, &r);
  const auto pixels = std::uint64_t(r.right) * r.bottom;
  Monstey_Require(r.right > 0 && r.bottom > 0 &&
                      pixels <= Monstey_TexturePixelBudget,
                  "Texture snapshot exceeds pixel budget");
  Monstey_TextureBitmapDC dc(hwnd, r.right, r.bottom);
  Monstey_Require(dc.pixels && dc.old && dc.old != HGDI_ERROR,
                  "Cannot allocate owned-window snapshot");
  SendMessageW(hwnd, WM_PRINT, reinterpret_cast<WPARAM>(dc.memory),
               PRF_CLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
  GdiFlush();
  B b(static_cast<std::size_t>(54 + pixels * 4));
  b[0] = 'B';
  b[1] = 'M';
  Monstey_PutU32(b, 2, static_cast<std::uint32_t>(b.size()));
  Monstey_PutU32(b, 10, 54);
  Monstey_PutU32(b, 14, 40);
  Monstey_PutU32(b, 18, static_cast<std::uint32_t>(r.right));
  Monstey_PutU32(b, 22, std::bit_cast<std::uint32_t>(-r.bottom));
  Monstey_PutU16(b, 26, 1);
  Monstey_PutU16(b, 28, 32);
  Monstey_PutU32(b, 34, static_cast<std::uint32_t>(pixels * 4));
  std::memcpy(b.data() + 54, dc.pixels, static_cast<std::size_t>(pixels * 4));
  return b;
}
} // namespace
struct Monstey_TexturePreviewWindow::Monstey_State {
  enum class Monstey_Kind { Load, Decode, Save };
  struct Monstey_Job {
    Monstey_Kind kind = Monstey_Kind::Load;
    std::uint64_t request = 0;
    std::filesystem::path source, output;
    std::string hash;
    std::shared_ptr<const Monstey_TextureResource> texture;
    std::size_t subresource = 0;
    std::uint32_t depth = 0;
  };
  struct Monstey_Result {
    Monstey_Kind kind = Monstey_Kind::Load;
    std::uint64_t request = 0;
    std::shared_ptr<const Monstey_TextureResource> texture;
    std::shared_ptr<const Monstey_TextureImage> image;
    J description = J::object(), saved = J::object();
    std::vector<Monstey_TextureSubresource> subresources;
    std::string error;
  };
  HWND window = nullptr;
  HBRUSH background = CreateSolidBrush(RGB(26, 28, 32));
  HFONT font = nullptr;
  unsigned dpi = 96;
  int canvasTop = 184;
  bool paintFailed = false;
  std::thread worker;
  mutable std::mutex mutex;
  std::condition_variable condition;
  std::deque<Monstey_Job> jobs;
  std::deque<Monstey_Result> results;
  bool stopping = false;
  bool acceptingResults = false;
  std::uint64_t acceptedRequest = 0, droppedResults = 0;
  std::atomic<bool> active = false;
  std::atomic<unsigned> phase = 0;
  std::uint64_t request = 0, discarded = 0, completed = 0;
  bool loading = false, savePending = false, fit = true, alpha = true,
       populating = false, drag = false;
  double zoom = 1, panX = 0, panY = 0;
  int dragX = 0, dragY = 0;
  std::size_t selected = 0;
  std::uint32_t depth = 0;
  unsigned paints = 0;
  std::uint64_t saveDialogRequests = 0;
  std::filesystem::path source;
  std::string hash;
  std::wstring error, status;
  std::shared_ptr<const Monstey_TextureResource> texture;
  std::shared_ptr<const Monstey_TextureImage> image;
  J description = J::object(), lastSave = J::object(), hover = J(nullptr);
  std::vector<Monstey_TextureSubresource> subresources;
  ~Monstey_State() {
    Stop();
    if (background)
      DeleteObject(background);
    if (font)
      DeleteObject(font);
  }
  int Scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi), 96);
  }
  void Font(unsigned value) {
    dpi = value ? value : 96;
    const auto fresh = CreateFontW(
        -MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
        FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    Monstey_Require(fresh != nullptr, "Cannot create texture UI font");
    const auto old = font;
    font = fresh;
    if (window)
      for (auto child = GetWindow(window, GW_CHILD); child;
           child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    if (old)
      DeleteObject(old);
  }
  void Start() {
    if (worker.joinable())
      return;
    Monstey_Require(background != nullptr,
                    "Cannot create texture window brush");
    worker = std::thread([this] {
      for (;;) {
        {
          Monstey_Job job;
          {
            std::unique_lock lock(mutex);
            condition.wait(lock, [this] { return stopping || !jobs.empty(); });
            if (stopping)
              break;
            job = std::move(jobs.front());
            jobs.pop_front();
            active = true;
          }
          Monstey_Result result;
          result.kind = job.kind;
          result.request = job.request;
          try {
            if (job.kind == Monstey_Kind::Save) {
              phase = 3;
              result.saved = Monstey_TextureCall(
                  "texture_preview_render",
                  J::object({{"source", Monstey_Utf8(job.source.wstring())},
                             {"expected_source_sha256", job.hash},
                             {"output", Monstey_Utf8(job.output.wstring())},
                             {"subresource",
                              static_cast<std::uint64_t>(job.subresource)},
                             {"depth_slice", job.depth},
                             {"maximum_pixels", Monstey_TexturePixelBudget}}));
            } else {
              auto value = job.texture;
              if (job.kind == Monstey_Kind::Load) {
                phase = 1;
                value = std::make_shared<Monstey_TextureResource>(job.source,
                                                                  job.hash);
                result.description = value->Describe();
                result.subresources = value->Subresources();
                if (result.subresources.size() > 65536) {
                  result.subresources.clear();
                  throw Monstey_Error(
                      "texture_window",
                      "Viewer selector supports at most 65536 subresources");
                }
              }
              result.texture = value;
              if (value) {
                if (job.kind == Monstey_Kind::Load &&
                    !result.description.at("decode_supported").as_bool())
                  result.error =
                      result.description.at("qualification_note").as_string();
                else {
                  phase = 2;
                  result.image = std::make_shared<Monstey_TextureImage>(
                      value->Decode(job.subresource, job.depth,
                                    Monstey_TexturePixelBudget));
                  (void)value->Describe();
                }
              }
            }
          } catch (const std::exception &e) {
            result.error = Monstey_DisplayUtf8(e.what());
          } catch (...) {
            result.error = "Unknown native texture worker failure";
          }
          {
            std::lock_guard lock(mutex);
            // Acceptance metadata belongs to this queue. The worker never reads
            // HWND or UI-owned request/image fields, including during close.
            if (!stopping && acceptingResults &&
                result.request == acceptedRequest)
              results.push_back(std::move(result));
            else
              ++droppedResults;
          }
        } // Release every local job/result owner before reporting idle.
        phase = 0;
        active = false;
      }
      phase = 0;
      active = false;
    });
  }
  void Stop() {
    if (!worker.joinable())
      return;
    {
      std::lock_guard lock(mutex);
      stopping = true;
      acceptingResults = false;
      jobs.clear();
    }
    condition.notify_all();
    worker.join();
  }
  void Enqueue(Monstey_Job job, bool replace) {
    Start();
    {
      std::lock_guard lock(mutex);
      if (replace) {
        discarded += jobs.size() + results.size();
        jobs.clear();
        results.clear();
      }
      jobs.push_back(std::move(job));
    }
    condition.notify_one();
  }
  void Advance() {
    ++request;
    if (!request)
      ++request;
    std::lock_guard lock(mutex);
    acceptedRequest = request;
    acceptingResults = window != nullptr; // UI thread only.
  }
  // A sample belongs to one image and one client-to-texel mapping. Invalidate
  // both representations together instead of interpreting stale coordinates.
  void ResetHover() {
    hover = J(nullptr);
    if (window)
      SetWindowTextW(GetDlgItem(window, Monstey_TextureHover), L"");
  }
  void Refresh() {
    if (window) {
      InvalidateRect(window, nullptr, FALSE);
    }
  }
  RECT Canvas() const {
    RECT r{};
    if (window)
      GetClientRect(window, &r);
    return {Scale(12), canvasTop,
            std::max(static_cast<LONG>(Scale(13)), r.right - Scale(12)),
            std::max(static_cast<LONG>(canvasTop + 1), r.bottom - Scale(12))};
  }
  struct Monstey_Transform {
    double x = 0, y = 0, scale = 1;
  };
  Monstey_Transform Transform() const {
    const auto c = Canvas();
    if (!image)
      return {};
    const auto scale = fit ? std::min(double(c.right - c.left) / image->width,
                                      double(c.bottom - c.top) / image->height)
                           : zoom;
    return {c.left + (c.right - c.left - image->width * scale) / 2 + panX,
            c.top + (c.bottom - c.top - image->height * scale) / 2 + panY,
            scale};
  }
  bool Pixel(int x, int y, std::uint32_t &sx, std::uint32_t &sy) const {
    if (!image)
      return false;
    const auto c = Canvas();
    if (x < c.left || x >= c.right || y < c.top || y >= c.bottom)
      return false;
    const auto t = Transform();
    const auto px = std::floor((x + 0.5 - t.x) / t.scale),
               py = std::floor((y + 0.5 - t.y) / t.scale);
    if (px < 0 || py < 0 || px >= image->width || py >= image->height)
      return false;
    sx = static_cast<std::uint32_t>(px);
    sy = static_cast<std::uint32_t>(py);
    return true;
  }
  void Controls() {
    if (!window)
      return;
    const bool loaded = texture && !subresources.empty() && !savePending;
    EnableWindow(GetDlgItem(window, Monstey_TextureSelection), loaded);
    const bool volume = loaded && selected < subresources.size() &&
                        subresources[selected].depth > 1;
    EnableWindow(GetDlgItem(window, Monstey_TextureDepth), volume);
    EnableWindow(GetDlgItem(window, Monstey_TextureApplyDepth), volume);
    for (int id : {Monstey_TextureFit, Monstey_TextureOne, Monstey_TextureMinus,
                   Monstey_TexturePlus, Monstey_TextureAlpha})
      EnableWindow(GetDlgItem(window, id), bool(image));
    EnableWindow(GetDlgItem(window, Monstey_TextureSave),
                 image && !loading && !savePending);
  }
  void Text() {
    if (!window)
      return;
    SetWindowTextW(GetDlgItem(window, Monstey_TextureSource),
                   (L"Source: " + source.wstring()).c_str());
    std::wstring format = L"Format: ";
    if (description.contains("container")) {
      format += Monstey_Wide(description.at("container").as_string());
      format +=
          L" / " +
          Monstey_Wide(description.value("format", J("UNKNOWN")).as_string());
    } else
      format += L"loading";
    if (image)
      format += L" | " + std::to_wstring(image->width) + L" x " +
                std::to_wstring(image->height);
    SetWindowTextW(GetDlgItem(window, Monstey_TextureFormat), format.c_str());
    SetWindowTextW(GetDlgItem(window, Monstey_TextureHash),
                   (L"SHA256: " + Monstey_Wide(hash)).c_str());
    std::wstring line = loading          ? L"Decoding on native worker ..."
                        : savePending    ? L"Saving new pinned PNG ..."
                        : !error.empty() ? error
                                         : status;
    if (image && !loading && !savePending && error.empty()) {
      const auto percent = std::lround(Transform().scale * 100);
      line += (line.empty() ? L"" : L" | ") +
              std::wstring(fit ? L"Fit " : L"Zoom ") +
              std::to_wstring(percent) + L"% | " +
              (alpha ? L"Straight RGBA over checkerboard"
                     : L"RGB only, alpha ignored");
    }
    SetWindowTextW(GetDlgItem(window, Monstey_TextureStatus), line.c_str());
    Controls();
    Refresh();
  }
  void Layout() {
    if (!window)
      return;
    RECT r{};
    GetClientRect(window, &r);
    const auto margin = Scale(12), gap = Scale(6);
    const auto width = static_cast<int>(r.right);
    const auto move = [&](int id, int x, int y, int w, int h) {
      MoveWindow(GetDlgItem(window, id), x, y, std::max(1, w), h, TRUE);
    };
    move(Monstey_TextureSource, margin, Scale(8), width - margin * 2,
         Scale(24));
    move(Monstey_TextureFormat, margin, Scale(36), width - margin * 2,
         Scale(20));
    move(Monstey_TextureHash, margin, Scale(58), width - margin * 2, Scale(24));
    move(201, margin, Scale(91), Scale(88), Scale(22));
    move(Monstey_TextureSelection, Scale(102), Scale(86), width - Scale(114),
         Scale(320));
    int x = margin, y = Scale(120);
    const auto flow = [&](int units, const auto &place) {
      const auto w = Scale(units);
      if (x > margin && x + w > width - margin) {
        x = margin;
        y += Scale(33);
      }
      place(x, y);
      x += w + gap;
    };
    flow(184, [&](int gx, int gy) {
      move(202, gx, gy + Scale(4), Scale(40), Scale(22));
      move(Monstey_TextureDepth, gx + Scale(44), gy, Scale(58), Scale(26));
      move(Monstey_TextureApplyDepth, gx + Scale(108), gy, Scale(76),
           Scale(27));
    });
    for (const auto &[id, w] :
         std::array<std::pair<int, int>, 7>{{{Monstey_TextureFit, 48},
                                             {Monstey_TextureOne, 48},
                                             {Monstey_TextureMinus, 32},
                                             {Monstey_TexturePlus, 32},
                                             {Monstey_TextureAlpha, 170},
                                             {Monstey_TextureSave, 90},
                                             {Monstey_TextureClose, 82}}})
      flow(w, [&](int gx, int gy) { move(id, gx, gy, Scale(w), Scale(27)); });
    const auto statusTop = y + Scale(34);
    move(Monstey_TextureStatus, margin, statusTop, width - margin * 2,
         Scale(26));
    move(Monstey_TextureHover, margin, statusTop + Scale(28),
         width - margin * 2, Scale(24));
    canvasTop = statusTop + Scale(58);
    ResetHover();
    Text();
  }
  void CreateControls() {
    const auto instance = GetModuleHandleW(nullptr);
    Font(GetDpiForWindow(window));
    const auto make = [&](const wchar_t *cls, const wchar_t *text, DWORD style,
                          int id) {
      auto h = CreateWindowExW(
          0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, window,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
      Monstey_Require(h != nullptr, "Cannot create texture window control");
      SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    };
    for (auto id :
         {Monstey_TextureSource, Monstey_TextureHash, Monstey_TextureHover})
      make(L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
           id);
    for (auto id : {Monstey_TextureFormat, Monstey_TextureStatus})
      make(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, id);
    make(L"STATIC", L"Subresource", SS_LEFT, 201);
    make(L"STATIC", L"Depth", SS_LEFT, 202);
    make(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
         Monstey_TextureSelection);
    make(L"EDIT", L"0", WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
         Monstey_TextureDepth);
    SendDlgItemMessageW(window, Monstey_TextureDepth, EM_SETLIMITTEXT, 10, 0);
    for (const auto &[id, label] :
         std::array<std::pair<int, const wchar_t *>, 8>{
             {{Monstey_TextureApplyDepth, L"View slice"},
              {Monstey_TextureFit, L"Fit"},
              {Monstey_TextureOne, L"1:1"},
              {Monstey_TextureMinus, L"-"},
              {Monstey_TexturePlus, L"+"},
              {Monstey_TextureSave, L"Save PNG"},
              {Monstey_TextureClose, L"Close"},
              {Monstey_TextureAlpha, L"Composite alpha"}}})
      make(L"BUTTON", label,
           WS_TABSTOP |
               (id == Monstey_TextureAlpha ? BS_AUTOCHECKBOX : BS_PUSHBUTTON),
           id);
    SendDlgItemMessageW(window, Monstey_TextureAlpha, BM_SETCHECK,
                        alpha ? BST_CHECKED : BST_UNCHECKED, 0);
    Monstey_Require(SetTimer(window, Monstey_TextureTimer, 25, nullptr) != 0,
                    "Cannot start texture result timer");
    Layout();
  }
  void Populate() {
    populating = true;
    const auto c = GetDlgItem(window, Monstey_TextureSelection);
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < subresources.size(); ++i) {
      const auto label = Monstey_TextureLabel(subresources[i], i);
      const auto added = SendMessageW(c, CB_ADDSTRING, 0,
                                      reinterpret_cast<LPARAM>(label.c_str()));
      if (added == CB_ERR || added == CB_ERRSPACE) {
        populating = false;
        throw Monstey_Error("texture_window",
                            "Cannot populate texture subresources");
      }
    }
    if (!subresources.empty())
      SendMessageW(c, CB_SETCURSEL, selected, 0);
    SetWindowTextW(GetDlgItem(window, Monstey_TextureDepth),
                   std::to_wstring(depth).c_str());
    populating = false;
  }
  void Drain() {
    std::deque<Monstey_Result> ready;
    {
      std::lock_guard lock(mutex);
      ready.swap(results);
    }
    for (auto &r : ready) {
      if (!window || r.request != request) {
        ++discarded;
        continue;
      }
      ++completed;
      if (r.kind == Monstey_Kind::Save) {
        savePending = false;
        lastSave = std::move(r.saved);
        error = Monstey_Wide(r.error);
        status = error.empty()
                     ? L"PNG saved with pinned source and verified pixels"
                     : L"";
      } else {
        loading = false;
        image = std::move(r.image);
        ResetHover();
        error = Monstey_Wide(r.error);
        if (r.kind == Monstey_Kind::Load) {
          texture = std::move(r.texture);
          description = std::move(r.description);
          subresources = std::move(r.subresources);
          if (description.contains("source_sha256"))
            hash = description.at("source_sha256").as_string();
          Populate();
        }
        status = image ? L"CPU numeric texture pixels, no gamma or normal "
                         L"reconstruction"
                       : L"Raw storage only; RGBA decode is not qualified";
      }
      Text();
    }
  }
  void DecodeSelection() {
    Monstey_Require(window && texture && selected < subresources.size(),
                    "No texture subresource is available");
    Advance();
    loading = true;
    savePending = false;
    image.reset();
    error.clear();
    ResetHover();
    Monstey_Job job;
    job.kind = Monstey_Kind::Decode;
    job.request = request;
    job.texture = texture;
    job.subresource = selected;
    job.depth = depth;
    Enqueue(std::move(job), true);
    Text();
  }
  void Select() {
    if (populating)
      return;
    const auto value = SendDlgItemMessageW(window, Monstey_TextureSelection,
                                           CB_GETCURSEL, 0, 0);
    Monstey_Require(value != CB_ERR &&
                        static_cast<std::size_t>(value) < subresources.size(),
                    "No selected texture subresource");
    selected = static_cast<std::size_t>(value);
    depth = 0;
    SetWindowTextW(GetDlgItem(window, Monstey_TextureDepth), L"0");
    DecodeSelection();
  }
  void ApplyDepth() {
    wchar_t b[16]{};
    const auto len =
        GetWindowTextLengthW(GetDlgItem(window, Monstey_TextureDepth));
    Monstey_Require(len > 0 && len < 16 && selected < subresources.size(),
                    "Enter a valid volume depth slice");
    GetDlgItemTextW(window, Monstey_TextureDepth, b, 16);
    std::uint64_t value = 0;
    for (int i = 0; i < len; ++i) {
      Monstey_Require(b[i] >= L'0' && b[i] <= L'9',
                      "Depth slice must contain decimal digits only");
      value = value * 10 + static_cast<unsigned>(b[i] - L'0');
    }
    Monstey_Require(value < subresources[selected].depth,
                    "Depth slice is outside this subresource");
    depth = static_cast<std::uint32_t>(value);
    DecodeSelection();
  }
  void Zoom(double factor) {
    if (!image)
      return;
    zoom = std::clamp(Transform().scale * factor, 1.0 / 64, 4096.0);
    fit = false;
    ResetHover();
    Text();
  }
  bool Save(const std::filesystem::path &output) {
    if (!window || !image || !texture || loading || savePending)
      return false;
    Monstey_Require(!output.empty(), "Select an explicit new PNG output");
    Monstey_Job job;
    job.kind = Monstey_Kind::Save;
    job.request = request;
    job.source = source;
    job.output = output;
    job.hash = hash;
    job.subresource = selected;
    job.depth = depth;
    savePending = true;
    error.clear();
    try {
      Enqueue(std::move(job), false);
    } catch (...) {
      savePending = false;
      throw;
    }
    Text();
    return true;
  }
  void SaveDialog() {
    if (!window || !image || loading || savePending)
      return;
    ++saveDialogRequests;
    const auto target = window;
    const auto identity = request;
    std::array<wchar_t, 32768> path{};
    const auto filename = source.stem().wstring() + L"_sub" +
                          std::to_wstring(selected) + L"_z" +
                          std::to_wstring(depth) + L".png";
    Monstey_Require(filename.size() < path.size(),
                    "PNG name exceeds dialog path limit");
    std::copy(filename.begin(), filename.end(), path.begin());
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = L"PNG image (*.png)\0*.png\0All files\0*.*\0\0";
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"png";
    dialog.lpstrTitle =
        L"Save a new pinned PNG (existing files are never replaced)";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog) && window == target && request == identity)
      (void)Save(path.data());
  }
  void Hover(int x, int y) {
    std::uint32_t px = 0, py = 0;
    if (!Pixel(x, y, px, py)) {
      ResetHover();
      SetWindowTextW(
          GetDlgItem(window, Monstey_TextureHover),
          L"Move over the image to inspect its exact raw RGBA pixel");
      return;
    }
    const auto &v = image->rgba[std::size_t(py) * image->width + px];
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << "Pixel " << px << ", " << py << " | Raw RGBA ("
         << std::setprecision(9) << v[0] << ", " << v[1] << ", " << v[2] << ", "
         << v[3] << ") | 8-bit (";
    auto values = J::array();
    for (unsigned i = 0; i < 4; ++i) {
      const auto q = Monstey_TextureQuantize(v[i]);
      values.as_array().emplace_back(q);
      if (i)
        text << ", ";
      text << unsigned(q);
    }
    text << ")";
    hover = J::object({{"x", px},
                       {"y", py},
                       {"rgba8", values},
                       {"raw_rgba_text", text.str()}});
    SetWindowTextW(GetDlgItem(window, Monstey_TextureHover),
                   Monstey_Wide(text.str()).c_str());
  }
  void Paint(HDC dc) {
    RECT r{};
    GetClientRect(window, &r);
    paintFailed = false;
    FillRect(dc, &r, background);
    const auto c = Canvas();
    const auto width = c.right - c.left, height = c.bottom - c.top;
    const auto count = std::uint64_t(width) * height;
    Monstey_Require(count <= Monstey_TexturePixelBudget,
                    "Texture viewport exceeds pixel budget");
    const auto transform = Transform();
    B pixels(static_cast<std::size_t>(count * 4));
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        auto *out = pixels.data() + (std::size_t(y) * width + x) * 4;
        out[0] = out[1] = out[2] = 34;
        out[3] = 255;
        if (image) {
          const auto px =
              std::floor((c.left + x + 0.5 - transform.x) / transform.scale);
          const auto py =
              std::floor((c.top + y + 0.5 - transform.y) / transform.scale);
          if (px >= 0 && py >= 0 && px < image->width && py < image->height) {
            const auto sx = static_cast<std::uint32_t>(px),
                       sy = static_cast<std::uint32_t>(py);
            const auto &value =
                image->rgba[std::size_t(sy) * image->width + sx];
            const auto a = Monstey_TextureQuantize(value[3]);
            const auto checker = ((x / 12 + y / 12) & 1) ? 88U : 56U;
            for (unsigned channel = 0; channel < 3; ++channel) {
              const auto component =
                  unsigned(Monstey_TextureQuantize(value[2 - channel]));
              out[channel] = static_cast<std::uint8_t>(
                  alpha ? (component * a + checker * (255 - a) + 127) / 255
                        : component);
            }
          }
        }
      }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    SetDIBitsToDevice(dc, c.left, c.top, static_cast<DWORD>(width),
                      static_cast<DWORD>(height), 0, 0, 0,
                      static_cast<UINT>(height), pixels.data(), &info,
                      DIB_RGB_COLORS);
    if (!image) {
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, RGB(220, 224, 232));
      auto box = c;
      InflateRect(&box, -24, -24);
      const auto message =
          loading ? L"Loading texture on the native worker ..."
          : !error.empty()
              ? error.c_str()
              : L"Open a supported texture to inspect its decoded pixels.";
      DrawTextW(dc, message, -1, &box, DT_WORDBREAK | DT_NOPREFIX);
    }
    ++paints;
  }
  void Closed(HWND hwnd) {
    KillTimer(hwnd, Monstey_TextureTimer);
    Advance();
    {
      std::lock_guard lock(mutex);
      acceptingResults = false;
      discarded += jobs.size() + results.size();
      jobs.clear();
      results.clear();
    }
    window = nullptr;
    loading = savePending = drag = false;
    texture.reset();
    image.reset();
    subresources.clear();
    description = J::object();
    ResetHover();
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  }
  static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM w,
                                    LPARAM l) {
    auto *s = reinterpret_cast<Monstey_State *>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      s = static_cast<Monstey_State *>(
          reinterpret_cast<const CREATESTRUCTW *>(l)->lpCreateParams);
      s->window = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
    }
    if (!s)
      return DefWindowProcW(hwnd, message, w, l);
    try {
      switch (message) {
      case WM_CREATE:
        s->CreateControls();
        return 0;
      case WM_GETMINMAXINFO: {
        auto *info = reinterpret_cast<MINMAXINFO *>(l);
        const auto work = Monstey_TextureWorkArea(hwnd);
        info->ptMinTrackSize = {
            std::min(static_cast<LONG>(s->Scale(780)), work.right - work.left),
            std::min(static_cast<LONG>(s->Scale(520)), work.bottom - work.top)};
        info->ptMaxTrackSize = {work.right - work.left, work.bottom - work.top};
        return 0;
      }
      case WM_DPICHANGED: {
        s->Font(HIWORD(w));
        const auto proposed = Monstey_TextureClamp(
            *reinterpret_cast<const RECT *>(l), hwnd, true);
        SetWindowPos(hwnd, nullptr, proposed.left, proposed.top,
                     proposed.right - proposed.left,
                     proposed.bottom - proposed.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        s->Layout();
        return 0;
      }
      case WM_MOVE:
        s->ResetHover();
        return 0;
      case WM_SIZE:
        s->Layout();
        return 0;
      case WM_TIMER:
        if (w == Monstey_TextureTimer)
          s->Drain();
        return 0;
      case WM_ERASEBKGND:
        return 1;
      case WM_CTLCOLORSTATIC:
        SetTextColor(reinterpret_cast<HDC>(w), RGB(220, 224, 232));
        SetBkColor(reinterpret_cast<HDC>(w), RGB(26, 28, 32));
        return reinterpret_cast<LRESULT>(s->background);
      case WM_COMMAND: {
        const auto id = LOWORD(w);
        if (id == Monstey_TextureSelection && HIWORD(w) == CBN_SELCHANGE) {
          s->Select();
          return 0;
        }
        if (HIWORD(w) != BN_CLICKED)
          return 0;
        switch (id) {
        case Monstey_TextureApplyDepth:
          s->ApplyDepth();
          break;
        case Monstey_TextureFit:
          s->fit = true;
          s->panX = s->panY = 0;
          s->ResetHover();
          s->Text();
          break;
        case Monstey_TextureOne:
          s->fit = false;
          s->zoom = 1;
          s->panX = s->panY = 0;
          s->ResetHover();
          s->Text();
          break;
        case Monstey_TextureMinus:
          s->Zoom(0.5);
          break;
        case Monstey_TexturePlus:
          s->Zoom(2);
          break;
        case Monstey_TextureAlpha:
          s->alpha = SendDlgItemMessageW(hwnd, Monstey_TextureAlpha,
                                         BM_GETCHECK, 0, 0) == BST_CHECKED;
          s->Text();
          break;
        case Monstey_TextureSave:
          s->SaveDialog();
          break;
        case Monstey_TextureClose:
          DestroyWindow(hwnd);
          break;
        }
        return 0;
      }
      case WM_MOUSEWHEEL:
        s->Zoom(GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1.25 : 0.8);
        return 0;
      case WM_LBUTTONDOWN:
        s->drag = true;
        s->dragX = GET_X_LPARAM(l);
        s->dragY = GET_Y_LPARAM(l);
        SetCapture(hwnd);
        return 0;
      case WM_LBUTTONUP:
        s->drag = false;
        if (GetCapture() == hwnd)
          ReleaseCapture();
        return 0;
      case WM_CAPTURECHANGED:
        s->drag = false;
        return 0;
      case WM_MOUSEMOVE: {
        const auto x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (s->drag) {
          s->panX += x - s->dragX;
          s->panY += y - s->dragY;
          s->dragX = x;
          s->dragY = y;
          s->Refresh();
        }
        s->Hover(x, y);
        return 0;
      }
      case WM_PAINT: {
        Monstey_TexturePaint paint(hwnd);
        s->Paint(paint.dc);
        return 0;
      }
      case WM_PRINTCLIENT:
        s->Paint(reinterpret_cast<HDC>(w));
        return 0;
      case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
      case WM_NCDESTROY:
        s->Closed(hwnd);
        return DefWindowProcW(hwnd, message, w, l);
      }
    } catch (const std::exception &e) {
      if (message == WM_NCCREATE)
        return FALSE;
      if (message == WM_CREATE)
        return -1;
      s->error = Monstey_Wide(Monstey_DisplayUtf8(e.what()));
      s->loading = false;
      if (message == WM_PAINT || message == WM_PRINTCLIENT) {
        s->paintFailed = true;
        SetWindowTextW(GetDlgItem(hwnd, Monstey_TextureStatus),
                       s->error.c_str());
      } else {
        s->savePending = false;
        s->Text();
      }
    }
    return DefWindowProcW(hwnd, message, w, l);
  }
};
Monstey_TexturePreviewWindow::Monstey_TexturePreviewWindow()
    : state_(std::make_unique<Monstey_State>()) {}
Monstey_TexturePreviewWindow::~Monstey_TexturePreviewWindow() { Close(); }
std::uint64_t
Monstey_TexturePreviewWindow::Open(HWND owner,
                                   const std::filesystem::path &source,
                                   std::string_view expected, int show) {
  auto &s = *state_;
  Monstey_Require(!source.empty(), "Select an explicit texture source");
  if (!s.window) {
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW existing{};
    if (!GetClassInfoW(instance, Monstey_TextureClass, &existing)) {
      WNDCLASSW klass{};
      klass.hInstance = instance;
      klass.lpfnWndProc = Monstey_State::Procedure;
      klass.lpszClassName = Monstey_TextureClass;
      klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      Monstey_Require(RegisterClassW(&klass) != 0,
                      "Cannot register texture preview class");
    } else
      Monstey_Require(existing.lpfnWndProc == Monstey_State::Procedure,
                      "Texture window class ownership differs");
    const auto value = owner ? GetDpiForWindow(owner) : GetDpiForSystem();
    const auto uiDpi = value ? value : 96U;
    const auto work = Monstey_TextureWorkArea(owner);
    RECT center = work;
    if (owner)
      GetWindowRect(owner, &center);
    const auto width = MulDiv(1060, static_cast<int>(uiDpi), 96),
               height = MulDiv(820, static_cast<int>(uiDpi), 96);
    RECT proposed{center.left + (center.right - center.left - width) / 2,
                  center.top + (center.bottom - center.top - height) / 2, 0, 0};
    proposed.right = proposed.left + width;
    proposed.bottom = proposed.top + height;
    const auto placement = Monstey_TextureClamp(proposed, owner);
    Monstey_Require(CreateWindowExW(WS_EX_CONTROLPARENT, Monstey_TextureClass,
                                    L"Texture Preview",
                                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                    placement.left, placement.top,
                                    placement.right - placement.left,
                                    placement.bottom - placement.top, owner,
                                    nullptr, instance, &s) != nullptr,
                    "Cannot create texture preview window");
  }
  Monstey_ThemeApply(s.window);
  s.Advance();
  s.source = source;
  s.hash = std::string(expected);
  s.loading = true;
  s.savePending = false;
  s.texture.reset();
  s.image.reset();
  s.description = J::object();
  s.subresources.clear();
  s.ResetHover();
  s.error.clear();
  s.status.clear();
  s.lastSave = J::object();
  s.selected = 0;
  s.depth = 0;
  s.fit = true;
  s.zoom = 1;
  s.panX = s.panY = 0;
  s.Populate();
  SetWindowTextW(s.window,
                 (L"Texture Preview - " + source.filename().wstring()).c_str());
  Monstey_State::Monstey_Job job;
  job.request = s.request;
  job.source = source;
  job.hash = std::string(expected);
  try {
    s.Enqueue(std::move(job), true);
  } catch (const std::exception &e) {
    s.loading = false;
    s.error = Monstey_Wide(Monstey_DisplayUtf8(e.what()));
    s.Text();
    throw;
  }
  s.Text();
  ShowWindow(s.window, show);
  UpdateWindow(s.window);
  return s.request;
}
HWND Monstey_TexturePreviewWindow::Handle() const { return state_->window; }
void Monstey_TexturePreviewWindow::Close() {
  if (state_ && state_->window)
    DestroyWindow(state_->window);
}
bool Monstey_TexturePreviewWindow::SavePng(const std::filesystem::path &path) {
  return state_->Save(path);
}
bool Monstey_TexturePreviewWindow::RouteMessage(MSG &message) {
  const auto hwnd = state_->window;
  if (!hwnd || !message.hwnd)
    return false;
  const auto root = GetAncestor(message.hwnd, GA_ROOT);
  bool owned =
      root == hwnd || message.hwnd == hwnd || IsChild(hwnd, message.hwnd);
  for (auto owner = GetWindow(root, GW_OWNER); !owned && owner;
       owner = GetWindow(owner, GW_OWNER))
    owned = owner == hwnd || IsChild(hwnd, owner);
  COMBOBOXINFO popup{};
  popup.cbSize = sizeof(popup);
  if (GetComboBoxInfo(GetDlgItem(hwnd, Monstey_TextureSelection), &popup))
    owned = owned || message.hwnd == popup.hwndList ||
            IsChild(popup.hwndList, message.hwnd);
  if (!owned)
    return false;
  if (message.message == WM_KEYDOWN) {
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
      if (message.wParam == 'O')
        return true;
      if (message.wParam == 'S') {
        try {
          state_->SaveDialog();
        } catch (const std::exception &e) {
          state_->error = Monstey_Wide(Monstey_DisplayUtf8(e.what()));
          state_->Text();
        }
        return true;
      }
    }
    const auto combo = GetDlgItem(hwnd, Monstey_TextureSelection);
    if (message.wParam == VK_ESCAPE) {
      if (SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0))
        SendMessageW(combo, CB_SHOWDROPDOWN, FALSE, 0);
      else
        Close();
      return true;
    }
    if (message.hwnd == GetDlgItem(hwnd, Monstey_TextureDepth) &&
        message.wParam == VK_RETURN) {
      try {
        state_->ApplyDepth();
      } catch (const std::exception &e) {
        state_->error = Monstey_Wide(Monstey_DisplayUtf8(e.what()));
        state_->Text();
      }
      return true;
    }
  }
  if (!IsDialogMessageW(hwnd, &message)) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return true;
}
J Monstey_TexturePreviewWindow::Inspect() const {
  const auto &s = *state_;
  const auto c = s.Canvas();
  const auto t = s.Transform();
  std::uint64_t queuedJobs = 0, queuedResults = 0, queuedImages = 0,
                queuedTextures = 0, dropped = 0;
  {
    std::lock_guard lock(s.mutex);
    queuedJobs = s.jobs.size();
    queuedResults = s.results.size();
    dropped = s.droppedResults;
    for (const auto &result : s.results) {
      queuedImages += bool(result.image);
      queuedTextures += bool(result.texture);
    }
  }
  return J::object(
      {{"dpi", s.dpi},
       {"window_open", s.window != nullptr},
       {"loading", s.loading},
       {"ready", bool(s.image) && !s.loading},
       {"source_loaded", bool(s.texture)},
       {"save_pending", s.savePending},
       {"request", s.request},
       {"discarded", s.discarded + dropped},
       {"dropped_results", dropped},
       {"queued_jobs", queuedJobs},
       {"queued_results", queuedResults},
       {"queued_images", queuedImages},
       {"queued_textures", queuedTextures},
       {"worker_active", s.active.load()},
       {"completed", s.completed},
       {"worker_phase", s.phase.load()},
       {"selected_subresource", static_cast<std::uint64_t>(s.selected)},
       {"depth_slice", s.depth},
       {"subresource_count", static_cast<std::uint64_t>(s.subresources.size())},
       {"width", s.image ? s.image->width : 0U},
       {"height", s.image ? s.image->height : 0U},
       {"source_sha256", s.hash},
       {"description", s.description},
       {"error", Monstey_Utf8(s.error)},
       {"fit", s.fit},
       {"scale", t.scale},
       {"composite_alpha", s.alpha},
       {"canvas",
        J::array({static_cast<int>(c.left), static_cast<int>(c.top),
                  static_cast<int>(c.right), static_cast<int>(c.bottom)})},
       {"image_origin", J::array({t.x, t.y})},
       {"hover", s.hover},
       {"paint_count", s.paints},
       {"save_dialog_requests", s.saveDialogRequests},
       {"last_save", s.lastSave},
       {"pixel_policy", "Raw numeric RGBA; NaN=0; clamp [0,1]; nearest 8-bit; "
                        "no gamma or normal reconstruction"},
       {"native_material_rendering_verified", false}});
}
B Monstey_TexturePreviewWindow::Snapshot() {
  Monstey_Require(state_->window != nullptr, "Texture window is closed");
  auto captured = Monstey_TextureCapture(state_->window);
  Monstey_Require(!state_->paintFailed,
                  "Texture paint failed; no screenshot published");
  return captured;
}
} // namespace Monstey