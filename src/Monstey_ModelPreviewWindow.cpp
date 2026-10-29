#include "Monstey_Theme.hpp"
#include <windows.h>

#include "Monstey_ModelPreviewWindow.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <windowsx.h>
#include <commctrl.h>
#include <optional>
#include <cerrno>
#include <cwchar>
namespace Monstey
{
namespace
{
constexpr wchar_t Monstey_PreviewClass[] = L"Monstey_SoWNativeModelPreview";
constexpr int Monstey_PreviewMesh = 100, Monstey_PreviewBones = 101, Monstey_PreviewFront = 102,
              Monstey_PreviewWire = 103, Monstey_PreviewColors = 104, Monstey_PreviewFit = 105,
              Monstey_PreviewContainer = 106, Monstey_PreviewClose = 107,
              Monstey_PreviewAnimation = 110, Monstey_PreviewClip = 111, Monstey_PreviewPlay = 112,
              Monstey_PreviewStop = 113, Monstey_PreviewLoop = 114, Monstey_PreviewRate = 115,
              Monstey_PreviewTimeline = 116, Monstey_PreviewTick = 117, Monstey_PreviewSeek = 118;
constexpr int Monstey_PreviewTop = 140;
constexpr UINT_PTR Monstey_PreviewTimer = 19;
struct Monstey_PreviewPaint
{
    HWND window;
    PAINTSTRUCT state{};
    HDC dc;
    explicit Monstey_PreviewPaint(HWND target) : window(target), dc(BeginPaint(target, &state))
    {
    }
    ~Monstey_PreviewPaint()
    {
        EndPaint(window, &state);
    }
    Monstey_PreviewPaint(const Monstey_PreviewPaint &) = delete;
    Monstey_PreviewPaint &operator=(const Monstey_PreviewPaint &) = delete;
};
struct Monstey_PreviewAccelerators
{
    HACCEL handle = nullptr;
    Monstey_PreviewAccelerators()
    {
        ACCEL keys[] = {{FVIRTKEY, 'F', Monstey_PreviewFit}, {FVIRTKEY, VK_ESCAPE, Monstey_PreviewClose}};
        handle = CreateAcceleratorTableW(keys, 2);
        Monstey_Require(handle != nullptr, "Preview accelerator creation failed");
    }
    ~Monstey_PreviewAccelerators()
    {
        DestroyAcceleratorTable(handle);
    }
    Monstey_PreviewAccelerators(const Monstey_PreviewAccelerators &) = delete;
    Monstey_PreviewAccelerators &operator=(const Monstey_PreviewAccelerators &) = delete;
};

} // namespace
struct Monstey_ModelPreviewWindow::Monstey_State
{
    HWND window = nullptr;
    Monstey_PreviewAccelerators accelerators;
    std::shared_ptr<const Monstey_ModelScene> scene, framingScene;
    std::filesystem::path skeletonPath, animationPath;
    std::function<void(const Monstey_PreviewAnimationRequest&)> animationLoader;
    std::shared_ptr<const Monstey_AnimationPreviewBinding> animation;
    std::unique_ptr<Monstey_AnimationPreviewFrame> animationFrame;
    Monstey_Json clips = Monstey_Json::array();
    bool animationLoading = false, playing = false, loop = true, clipListValid = true;
    std::uint32_t lastClock = 0;
    float rate = 1;
    double firstTick = 0, lastTick = 0;
    std::wstring animationError, pendingAnimation;
    Monstey_ModelPreviewOptions options;
    Monstey_ModelPreviewFrame frame;
    std::vector<std::size_t> containers;
    std::uint64_t request = 0;
    bool dirty = true, drag = false, pan = false, failed = false, loading = false;
    int x = 0, y = 0;
    unsigned renders = 0;
    std::wstring error, sourceTitle;
    void Controls()
    {
        if (!window) return;
        const bool enabled = scene && !loading && !failed;
        for (int id = Monstey_PreviewMesh; id <= Monstey_PreviewContainer; ++id)
            EnableWindow(GetDlgItem(window, id), enabled);
        EnableWindow(GetDlgItem(window, Monstey_PreviewAnimation), enabled && !skeletonPath.empty() && bool(animationLoader));
        EnableWindow(GetDlgItem(window, Monstey_PreviewClip), enabled && !animationLoading && !clips.empty() && clipListValid);
        const bool movable = enabled && !animationLoading && animationFrame && lastTick > firstTick;
        for (int id : {Monstey_PreviewPlay, Monstey_PreviewStop, Monstey_PreviewLoop, Monstey_PreviewRate,
                       Monstey_PreviewTimeline, Monstey_PreviewTick, Monstey_PreviewSeek})
            EnableWindow(GetDlgItem(window, id), movable);
        SetWindowTextW(GetDlgItem(window, Monstey_PreviewPlay), playing ? L"Pause" : L"Play");
    }
    void ResetAnimation()
    {
        if (window) KillTimer(window, Monstey_PreviewTimer);
        playing = animationLoading = false; clipListValid = true;
        animationFrame.reset(); animation.reset(); animationPath.clear();
        clips = Monstey_Json::array(); animationError.clear(); pendingAnimation.clear();
        firstTick = lastTick = 0; lastClock = 0; rate = 1; loop = true;
        if (window) {
            SendDlgItemMessageW(window, Monstey_PreviewClip, CB_RESETCONTENT, 0, 0);
            SendDlgItemMessageW(window, Monstey_PreviewRate, CB_SETCURSEL, 2, 0);
            SendDlgItemMessageW(window, Monstey_PreviewLoop, BM_SETCHECK, BST_CHECKED, 0);
        }
    }
    void UpdateTimeline()
    {
        if (!window || !animationFrame) return;
        const auto tick = animationFrame->Tick();
        const auto position = lastTick > firstTick ? static_cast<LPARAM>(std::clamp((tick-firstTick)/(lastTick-firstTick),0.0,1.0)*100000) : 0;
        SendDlgItemMessageW(window, Monstey_PreviewTimeline, TBM_SETPOS, TRUE, position);
        if (GetFocus() != GetDlgItem(window, Monstey_PreviewTick))
            SetWindowTextW(GetDlgItem(window, Monstey_PreviewTick), std::to_wstring(static_cast<std::uint64_t>(tick)).c_str());
    }
    void SyncClip()
    {
        if (!window || !animation || !clipListValid) return;
        const auto selected = animation->Inspect().at("clip").as_string();
        for (std::size_t i=0;i<clips.size();++i)
            if (clips[i].at("name").as_string()==selected) {
                SendDlgItemMessageW(window, Monstey_PreviewClip, CB_SETCURSEL, i, 0); break;
            }
    }
    void Play(bool value)
    {
        if (window) KillTimer(window, Monstey_PreviewTimer);
        playing = value && animation && animationFrame && !animationLoading && lastTick > firstTick;
        if (playing) {
            if (animationFrame->Tick() >= lastTick) animation->Evaluate(firstTick, *animationFrame);
            lastClock = GetTickCount();
            Monstey_Require(SetTimer(window, Monstey_PreviewTimer, 16, nullptr)!=0, "Cannot start preview playback timer");
        }
        Controls(); UpdateTimeline(); Refresh();
    }
    bool Seek(double tick, bool resetClock = true)
    {
        if (!animation || !animationFrame || animationLoading) return false;
        try {
            Monstey_Require(std::isfinite(tick) && std::floor(tick)==tick, "The preview timeline uses integer milliseconds");
            animation->Evaluate(tick, *animationFrame);
            if (resetClock) lastClock = GetTickCount();
            animationError.clear(); UpdateTimeline(); Refresh(); return true;
        } catch (const std::exception& e) {
            Play(false); animationError = Monstey_Wide(Monstey_DisplayUtf8(e.what())); Refresh(); return false;
        }
    }
    void SeekEdit()
    {
        wchar_t text[64]{};
        const int size = GetWindowTextLengthW(GetDlgItem(window, Monstey_PreviewTick));
        if (size <= 0 || size >= 64) { animationError=L"Enter an integer time in milliseconds."; Refresh(); return; }
        GetDlgItemTextW(window, Monstey_PreviewTick, text, 64);
        wchar_t* end=nullptr; errno=0;
        const auto tick=std::wcstoull(text,&end,10);
        if (errno || !end || end==text || *end || text[0]==L'-' || tick>UINT32_MAX) {
            animationError=L"Enter an integer time from 0 to 4294967295 ms."; Refresh(); return;
        }
        Play(false); (void)Seek(static_cast<double>(tick));
    }
    bool Advance(std::uint32_t now)
    {
        if (!playing || !animation || !animationFrame || animationLoading || IsIconic(window)) return false;
        const auto elapsed=std::min<std::uint32_t>(now-lastClock,1000);
        if (elapsed<16) return false;
        const auto scaled=static_cast<std::uint32_t>(static_cast<float>(elapsed)*rate);
        lastClock=now;
        auto tick=animationFrame->Tick()+scaled;
        bool end=false;
        if (tick>lastTick || (!loop && tick==lastTick)) {
            if (loop) tick=firstTick+std::fmod(tick-firstTick,lastTick-firstTick);
            else { tick=lastTick; end=true; }
        }
        if (!Seek(tick,false)) return false;
        if (end) Play(false);
        return true;
    }
    void Request(const std::filesystem::path& path, std::string clip)
    {
        Monstey_Require(window && scene && !loading && !failed && !skeletonPath.empty() && animationLoader,
                        "Load a model with its explicit matching skeleton before choosing animation");
        if (path.empty()) {
            Play(false);
            const auto pickerWindow=window;
            const auto pickerRequest=request;
            Monstey_PreviewAnimationRequest pick;
            try { const auto callback=animationLoader; callback(pick); }
            catch (const std::exception& e) {
                if (window==pickerWindow && request==pickerRequest && scene && !failed) {
                    animationError=Monstey_Wide(Monstey_DisplayUtf8(e.what()));
                    SyncClip(); Controls(); Refresh();
                }
            }
            return;
        }
        Play(false);
        ++request; if (!request) ++request;
        Monstey_PreviewAnimationRequest load;
        load.animation=path; load.skeleton=skeletonPath; load.clip=std::move(clip);
        load.source=scene; load.request=request;
        if (!load.clip.empty() && path==animationPath && animation)
            load.expected_anix_sha256=animation->Inspect().at("anix_sha256").as_string();
        animationLoading=true; animationError.clear();
        pendingAnimation=path.filename().wstring();
        Controls(); Refresh();
        try { const auto callback=animationLoader; callback(load); }
        catch (const std::exception& e) {
            if (window && scene && animationLoading && request==load.request) {
                animationLoading=false; animationError=Monstey_Wide(Monstey_DisplayUtf8(e.what()));
                SyncClip(); Controls(); Refresh();
            }
        }
    }
    void LayoutAnimation()
    {
        if (!window) return;
        RECT r{}; GetClientRect(window,&r);
        MoveWindow(GetDlgItem(window,Monstey_PreviewTimeline),10,74,std::max(32L,r.right-230),28,TRUE);
        MoveWindow(GetDlgItem(window,Monstey_PreviewTick),std::max(44L,r.right-210),74,140,26,TRUE);
        MoveWindow(GetDlgItem(window,Monstey_PreviewSeek),std::max(188L,r.right-60),74,50,26,TRUE);
    }
    void Fail(std::string_view message) noexcept
    {
        failed = true;
        loading = false;
        ResetAnimation();
        scene.reset();
        framingScene.reset();
        frame = {};
        try
        {
            error = Monstey_Wide(Monstey_DisplayUtf8(message));
        }
        catch (...)
        {
            error.clear();
        }
        if (window)
        {
            Controls();
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    void Render()
    {
        if (!dirty || !scene || loading || failed)
            return;
        RECT r{};
        GetClientRect(window, &r);
        const double w = std::max(32L, r.right), h = std::max(32L, r.bottom - Monstey_PreviewTop);
        const double reduction = std::min({1.0, 4096 / w, 4096 / h, std::sqrt(8388608 / (w * h))});
        options.width = static_cast<std::uint32_t>(std::max(32.0, std::floor(w * reduction)));
        options.height = static_cast<std::uint32_t>(std::max(32.0, std::floor(h * reduction)));
        const auto& displayed = animationFrame ? animationFrame->Scene() : *scene;
        frame = Monstey_ModelPreviewRenderFramed(displayed, framingScene ? *framingScene : *scene, options);
        if (animationFrame) {
            frame.report["host_bind_geometry_only"] = false;
            frame.report["animation_sample_evaluated"] = true;
            frame.report["host_animation_playback"] = playing;
            frame.report["native_animation_controller"] = false;
            frame.report["animation_tick_ms"] = animationFrame->Tick();
        }
        dirty = false;
        ++renders;
    }
    void Refresh()
    {
        dirty = true;
        if (window) InvalidateRect(window, nullptr, FALSE);
    }
    void Fit()
    {
        framingScene = animationFrame ? std::make_shared<Monstey_ModelScene>(animationFrame->Scene()) : scene;
        options.yaw_degrees = 0;
        options.pitch_degrees = 0;
        options.zoom = 1;
        options.pan_x = options.pan_y = 0;
        Refresh();
    }
    bool Checked(int id) const
    {
        return SendDlgItemMessageW(window, id, BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    void Check(int id, bool value)
    {
        SendDlgItemMessageW(window, id, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
    }
    static LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l)
    {
        auto *s = reinterpret_cast<Monstey_State *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            s = static_cast<Monstey_State *>(reinterpret_cast<const CREATESTRUCTW *>(l)->lpCreateParams);
            s->window = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        }
        if (!s)
            return DefWindowProcW(hwnd, message, w, l);
        try
        {
            switch (message)
            {
            case WM_CREATE: {
                const auto create = [&](const wchar_t *type, const wchar_t *text, DWORD style, int id, int x,
                                        int width) {
                    auto control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, x, 8,
                                                   width, 26, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                                   GetModuleHandleW(nullptr), nullptr);
                    Monstey_Require(control != nullptr, "Preview control creation failed");
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                    return control;
                };
                create(L"BUTTON", L"Mesh", BS_AUTOCHECKBOX, Monstey_PreviewMesh, 10, 65);
                create(L"BUTTON", L"Bones", BS_AUTOCHECKBOX, Monstey_PreviewBones, 80, 68);
                create(L"BUTTON", L"Bones in front", BS_AUTOCHECKBOX, Monstey_PreviewFront, 152, 110);
                create(L"BUTTON", L"Wireframe", BS_AUTOCHECKBOX, Monstey_PreviewWire, 266, 86);
                create(L"BUTTON", L"Vertex RGB", BS_AUTOCHECKBOX, Monstey_PreviewColors, 356, 94);
                create(L"BUTTON", L"Fit (F)", BS_PUSHBUTTON, Monstey_PreviewFit, 456, 70);
                const auto combo =
                    create(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, Monstey_PreviewContainer, 536, 190);
                MoveWindow(combo, 536, 8, 190, 220, TRUE);
                for (const auto c : s->containers)
                {
                    const auto label = L"Container " + std::to_wstring(c);
                    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                }
                SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All containers"));
                SendMessageW(combo, CB_SETCURSEL, 0, 0);
                for (const auto id : {Monstey_PreviewMesh, Monstey_PreviewBones, Monstey_PreviewFront})
                    SendDlgItemMessageW(hwnd, id, BM_SETCHECK, BST_CHECKED, 0);
                const auto createAt = [&](const wchar_t* type,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height) {
                    auto child=CreateWindowExW(type==std::wstring_view(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,
                        WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,x,y,width,height,hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
                    Monstey_Require(child!=nullptr,"Animation preview control creation failed");
                    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return child;
                };
                createAt(L"BUTTON",L"Open animation...",BS_PUSHBUTTON,Monstey_PreviewAnimation,10,40,136,26);
                createAt(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,Monstey_PreviewClip,154,40,286,320);
                createAt(L"BUTTON",L"Play",BS_PUSHBUTTON,Monstey_PreviewPlay,448,40,70,26);
                createAt(L"BUTTON",L"Stop",BS_PUSHBUTTON,Monstey_PreviewStop,526,40,70,26);
                createAt(L"BUTTON",L"Loop",BS_AUTOCHECKBOX,Monstey_PreviewLoop,604,40,65,26);
                const auto rate=createAt(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,Monstey_PreviewRate,677,40,83,180);
                for (const auto text:{L"0.25x",L"0.5x",L"1x",L"2x",L"4x"}) SendMessageW(rate,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));
                SendMessageW(rate,CB_SETCURSEL,2,0);
                SendDlgItemMessageW(hwnd,Monstey_PreviewLoop,BM_SETCHECK,BST_CHECKED,0);
                createAt(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,Monstey_PreviewTimeline,10,74,600,28);
                SendDlgItemMessageW(hwnd,Monstey_PreviewTimeline,TBM_SETRANGEMIN,FALSE,0);
                SendDlgItemMessageW(hwnd,Monstey_PreviewTimeline,TBM_SETRANGEMAX,FALSE,100000);
                createAt(L"EDIT",L"0",ES_AUTOHSCROLL|ES_NUMBER,Monstey_PreviewTick,630,74,140,26);
                SendDlgItemMessageW(hwnd,Monstey_PreviewTick,EM_SETLIMITTEXT,32,0);
                createAt(L"BUTTON",L"Go",BS_PUSHBUTTON,Monstey_PreviewSeek,780,74,50,26);
                s->LayoutAnimation(); s->Controls();
                return 0;
            }
            case WM_GETMINMAXINFO: {
                auto *limits = reinterpret_cast<MINMAXINFO *>(l);
                limits->ptMinTrackSize = {820, 430};
                return 0;
            }
            case WM_SIZE:
                s->LayoutAnimation(); s->Refresh();
                return 0;
            case WM_COMMAND: {
                const auto id = LOWORD(w);
                if (id == Monstey_PreviewAnimation) s->Request({},{});
                else if (id == Monstey_PreviewClip && HIWORD(w)==CBN_SELCHANGE) {
                    const auto selected=SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0);
                    if (selected>=0 && static_cast<std::size_t>(selected)<s->clips.size())
                        s->Request(s->animationPath,s->clips[static_cast<std::size_t>(selected)].at("name").as_string());
                }
                else if (id == Monstey_PreviewPlay) s->Play(!s->playing);
                else if (id == Monstey_PreviewStop) { s->Play(false); (void)s->Seek(s->firstTick); }
                else if (id == Monstey_PreviewLoop) s->loop=s->Checked(id);
                else if (id == Monstey_PreviewRate && HIWORD(w)==CBN_SELCHANGE) {
                    const auto selected=SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0);
                    constexpr float rates[]={0.25f,0.5f,1,2,4};
                    if (selected>=0 && selected<5) {s->rate=rates[selected];s->lastClock=GetTickCount();}
                }
                else if (id == Monstey_PreviewSeek) s->SeekEdit();
                else if (id == Monstey_PreviewClose)
                    DestroyWindow(hwnd);
                else if (id == Monstey_PreviewFit)
                    s->Fit();
                else if (id == Monstey_PreviewContainer && HIWORD(w) == CBN_SELCHANGE)
                {
                    const auto selected = SendDlgItemMessageW(hwnd, id, CB_GETCURSEL, 0, 0);
                    if (selected >= 0 && static_cast<std::size_t>(selected) <= s->containers.size())
                    {
                        s->options.all_containers = static_cast<std::size_t>(selected) == s->containers.size();
                        if (!s->options.all_containers)
                            s->options.container = s->containers[static_cast<std::size_t>(selected)];
                        s->Refresh();
                    }
                }
                else if (id >= Monstey_PreviewMesh && id <= Monstey_PreviewColors)
                {
                    s->options.mesh = s->Checked(Monstey_PreviewMesh);
                    s->options.bones = s->Checked(Monstey_PreviewBones);
                    s->options.bones_in_front = s->Checked(Monstey_PreviewFront);
                    s->options.wireframe = s->Checked(Monstey_PreviewWire);
                    s->options.vertex_colors = s->Checked(Monstey_PreviewColors);
                    s->Refresh();
                }
                return 0;
            }
            case WM_HSCROLL:
                if (reinterpret_cast<HWND>(l)==GetDlgItem(hwnd,Monstey_PreviewTimeline) && s->animationFrame) {
                    const auto position=SendDlgItemMessageW(hwnd,Monstey_PreviewTimeline,TBM_GETPOS,0,0);
                    s->Play(false);
                    (void)s->Seek(std::floor(s->firstTick+double(position)/100000*(s->lastTick-s->firstTick)));
                }
                return 0;
            case WM_TIMER:
                if (w==Monstey_PreviewTimer) (void)s->Advance(GetTickCount());
                return 0;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
                if (GET_Y_LPARAM(l) >= Monstey_PreviewTop)
                {
                    s->drag = true;
                    s->pan = message == WM_RBUTTONDOWN;
                    s->x = GET_X_LPARAM(l);
                    s->y = GET_Y_LPARAM(l);
                    SetFocus(hwnd);
                    SetCapture(hwnd);
                }
                return 0;
            case WM_MOUSEMOVE:
                if (s->drag)
                {
                    const auto x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                    RECT r{};
                    GetClientRect(hwnd, &r);
                    if (s->pan)
                    {
                        s->options.pan_x =
                            std::clamp(s->options.pan_x + double(x - s->x) / std::max(32L, r.right), -8.0, 8.0);
                        s->options.pan_y =
                            std::clamp(s->options.pan_y + double(y - s->y) / std::max(32L, r.bottom - Monstey_PreviewTop), -8.0, 8.0);
                    }
                    else
                    {
                        s->options.yaw_degrees = std::remainder(s->options.yaw_degrees + double(x - s->x) * 0.5, 360.0);
                        s->options.pitch_degrees =
                            std::clamp(s->options.pitch_degrees + double(y - s->y) * 0.5, -89.0, 89.0);
                    }
                    s->x = x;
                    s->y = y;
                    s->Refresh();
                }
                return 0;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
                s->drag = false;
                ReleaseCapture();
                return 0;
            case WM_CAPTURECHANGED:
                s->drag = false;
                return 0;
            case WM_MOUSEWHEEL:
                s->options.zoom = std::clamp(
                    s->options.zoom * std::pow(1.1, double(GET_WHEEL_DELTA_WPARAM(w)) / WHEEL_DELTA), 0.05, 20.0);
                s->Refresh();
                return 0;
            case WM_KEYDOWN:
                if (w == VK_LEFT || w == VK_RIGHT || w == VK_UP || w == VK_DOWN)
                {
                    if (w == VK_LEFT || w == VK_RIGHT)
                        s->options.yaw_degrees += w == VK_LEFT ? -5 : 5;
                    else
                        s->options.pitch_degrees =
                            std::clamp(s->options.pitch_degrees + (w == VK_UP ? -5 : 5), -89.0, 89.0);
                    s->Refresh();
                }
                else if (w == VK_SPACE)
                    s->Play(!s->playing);
                else if (w == 'F')
                    s->Fit();
                else if (w == VK_ESCAPE)
                    DestroyWindow(hwnd);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_GETDLGCODE:
                return DLGC_WANTARROWS | DLGC_WANTCHARS;
            case WM_PAINT: {
                Monstey_PreviewPaint paint(hwnd);
                const auto dc = paint.dc;
                RECT r{};
                GetClientRect(hwnd, &r);
                SetDCBrushColor(dc, RGB(27, 31, 37));
                FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, RGB(220, 227, 235));
                try
                {
                    if (s->scene && !s->loading && !s->failed)
                    {
                        s->Render();
                        BITMAPINFO info{};
                        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        info.bmiHeader.biWidth = static_cast<LONG>(s->frame.width);
                        info.bmiHeader.biHeight = -static_cast<LONG>(s->frame.height);
                        info.bmiHeader.biPlanes = 1;
                        info.bmiHeader.biBitCount = 32;
                        info.bmiHeader.biCompression = BI_RGB;
                        StretchDIBits(dc, 0, Monstey_PreviewTop, r.right, std::max(0L, r.bottom - Monstey_PreviewTop), 0, 0,
                                      static_cast<int>(s->frame.width), static_cast<int>(s->frame.height),
                                      s->frame.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
                        const auto mode = s->animationFrame ? L"ANIX " + Monstey_Wide(s->animation->Inspect().at("clip").as_string()) +
                            L" | " + std::to_wstring(static_cast<std::uint64_t>(s->animationFrame->Tick())) + L" ms | " : std::wstring(L"Bind geometry | ");
                        const auto text = mode + std::to_wstring(s->frame.report["vertices"].as_uint()) +
                                          L" vertices | " + std::to_wstring(s->frame.report["triangles"].as_uint()) +
                                          L" triangles | " + std::to_wstring(s->scene->bones.size()) +
                                          L" joints | Drag: orbit, right drag: pan, wheel: zoom";
                        RECT status{10, 108, r.right - 10, 134};
                        const auto statusText = !s->animationError.empty() ? s->animationError :
                            s->animationLoading ? L"Loading animation: " + s->pendingAnimation : text;
                        DrawTextW(dc, statusText.c_str(), -1, &status, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                    else
                    {
                        RECT area{16, Monstey_PreviewTop+20, r.right - 16, r.bottom - 16};
                        const wchar_t *text = s->failed
                                                  ? (s->error.empty() ? L"Preview operation failed." : s->error.c_str())
                                              : s->loading ? L"Loading native geometry and skeleton..."
                                                           : L"Choose a model from the Models tab.";
                        DrawTextW(dc, text, -1, &area, DT_LEFT | DT_WORDBREAK);
                        RECT status{10, 108, r.right - 10, 134};
                        DrawTextW(dc, s->sourceTitle.c_str(), -1, &status, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                }
                catch (const std::exception &e)
                {
                    s->Fail(e.what());
                    RECT area{16, Monstey_PreviewTop+20, r.right - 16, r.bottom - 16};
                    DrawTextW(dc, s->error.empty() ? L"Preview rendering failed." : s->error.c_str(), -1, &area,
                              DT_WORDBREAK);
                }
                return 0;
            }
            case WM_NCDESTROY:
                s->ResetAnimation();
                s->framingScene.reset(); s->skeletonPath.clear();
                s->window = nullptr;
                ++s->request;
                s->loading = false;
                s->drag = false;
                s->scene.reset();
                s->frame = {};
                s->containers.clear();
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                return DefWindowProcW(hwnd, message, w, l);
            }
        }
        catch (const std::exception &e)
        {
            s->failed = true;
            if (message == WM_CREATE || message == WM_NCCREATE)
                return message == WM_CREATE ? -1 : FALSE;
            s->Fail(e.what());
        }
        return DefWindowProcW(hwnd, message, w, l);
    }
};
Monstey_ModelPreviewWindow::Monstey_ModelPreviewWindow() : state_(std::make_unique<Monstey_State>())
{
}
Monstey_ModelPreviewWindow::~Monstey_ModelPreviewWindow()
{
    Close();
}
void Monstey_ModelPreviewWindow::Create(HWND owner, int show)
{
    if (state_->window)
    {
        ShowWindow(state_->window, show);
        SetForegroundWindow(state_->window);
        return;
    }
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};
    Monstey_Require(InitCommonControlsEx(&controls)!=FALSE,"Cannot initialize preview timeline controls");
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW existing{};
    if (!GetClassInfoW(instance, Monstey_PreviewClass, &existing))
    {
        WNDCLASSW klass{};
        klass.hInstance = instance;
        klass.lpfnWndProc = Monstey_State::Procedure;
        klass.lpszClassName = Monstey_PreviewClass;
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        Monstey_Require(RegisterClassW(&klass) != 0, "Cannot register native model preview");
    }
    else
        Monstey_Require(existing.lpfnWndProc == Monstey_State::Procedure, "Native preview class ownership differs");
    state_->failed = false;
    state_->error.clear();
    const auto hwnd = CreateWindowExW(WS_EX_CONTROLPARENT, Monstey_PreviewClass, L"Model Preview",
                                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1050, 1050, owner, nullptr,
                                      instance, state_.get());
    Monstey_Require(hwnd != nullptr, "Cannot create native model preview");
    Monstey_ThemeApply(hwnd);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
}
HWND Monstey_ModelPreviewWindow::Handle() const
{
    return state_->window;
}
std::uint64_t Monstey_ModelPreviewWindow::BeginLoad(const std::filesystem::path &source,
                                                    const std::filesystem::path &skeleton)
{
    state_->skeletonPath = skeleton;
    Monstey_Require(state_->window != nullptr, "Preview window is closed");
    const auto title = L"Model Preview - " + source.filename().wstring();
    ++state_->request;
    if (!state_->request)
        ++state_->request;
    state_->ResetAnimation();
    state_->scene.reset(); state_->framingScene.reset();
    state_->frame = {};
    state_->containers.clear();
    state_->loading = true;
    state_->failed = false;
    state_->error.clear();
    state_->sourceTitle = source.wstring();
    state_->options = {};
    state_->dirty = true;
    SendDlgItemMessageW(state_->window, Monstey_PreviewContainer, CB_RESETCONTENT, 0, 0);
    SetWindowTextW(state_->window, title.c_str());
    state_->Controls();
    state_->Refresh();
    return state_->request;
}
bool Monstey_ModelPreviewWindow::Accepts(std::uint64_t request) const
{
    return state_->window && state_->loading && request != 0 && state_->request == request;
}
bool Monstey_ModelPreviewWindow::SetScene(std::uint64_t request, std::shared_ptr<const Monstey_ModelScene> scene)
{
    if (!Accepts(request))
        return false;
    try
    {
        Monstey_Require(scene && !scene->parts.empty(), "Preview has no geometry");
        std::set<std::size_t> unique;
        for (const auto &part : scene->parts)
            unique.insert(part.container);
        std::vector<std::size_t> containers(unique.begin(), unique.end());
        const auto combo = GetDlgItem(state_->window, Monstey_PreviewContainer);
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const auto container : containers)
        {
            const auto label = L"Container " + std::to_wstring(container);
            const auto added = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
            Monstey_Require(added != CB_ERR && added != CB_ERRSPACE, "Cannot populate preview containers");
        }
        const auto added = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"All containers"));
        Monstey_Require(added != CB_ERR && added != CB_ERRSPACE, "Cannot populate preview container choice");
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
        state_->containers = std::move(containers);
        state_->options.container = state_->containers.front();
        state_->scene = std::move(scene);
        state_->framingScene = state_->scene;
        state_->loading = false;
        state_->failed = false;
        for (const auto id : {Monstey_PreviewMesh, Monstey_PreviewBones, Monstey_PreviewFront})
            SendDlgItemMessageW(state_->window, id, BM_SETCHECK, BST_CHECKED, 0);
        for (const auto id : {Monstey_PreviewWire, Monstey_PreviewColors})
            SendDlgItemMessageW(state_->window, id, BM_SETCHECK, BST_UNCHECKED, 0);
        state_->Controls();
        state_->Fit();
        UpdateWindow(state_->window);
        return !state_->failed;
    }
    catch (const std::exception &e)
    {
        state_->Fail(e.what());
        return false;
    }
}
bool Monstey_ModelPreviewWindow::SetError(std::uint64_t request, std::string_view message)
{
    if (!Accepts(request))
        return false;
    state_->Fail(message);
    return true;
}
void Monstey_ModelPreviewWindow::SetAnimationLoader(std::function<void(const Monstey_PreviewAnimationRequest&)> loader) {
    state_->animationLoader=std::move(loader);state_->Controls();
}
void Monstey_ModelPreviewWindow::RequestAnimation(const std::filesystem::path& path,std::string clip) {
    state_->Request(path,std::move(clip));
}
bool Monstey_ModelPreviewWindow::AcceptsAnimation(std::uint64_t request) const {
    return state_->window && state_->scene && state_->animationLoading && request!=0 && request==state_->request;
}
bool Monstey_ModelPreviewWindow::SetAnimation(std::uint64_t request,
    std::shared_ptr<const Monstey_AnimationPreviewBinding> binding,
    std::unique_ptr<Monstey_AnimationPreviewFrame> frame,const Monstey_Json& clips,
    const std::filesystem::path& path) {
    if (!AcceptsAnimation(request)) return false;
    try {
        Monstey_Require(binding && frame && clips.is_array() && !clips.empty() && clips.size()<=65536,
            "Invalid animation preview completion");
        const auto info=binding->Inspect();
        Monstey_Require(info.at("model_sha256").as_string()==state_->scene->source_sha256 &&
            info.at("skel_sha256").as_string()==state_->scene->skeleton_sha256,"Animation completion source differs");
        binding->Evaluate(frame->Tick(),*frame); // Also verifies exact frame ownership before UI publication.
        const auto makeLabels=[](const Monstey_Json& rows) {
            std::vector<std::wstring> labels; labels.reserve(rows.size());
            for (const auto& c:rows.as_array()) {
                auto label=Monstey_Wide(c.at("name").as_string());
                if (!c.value("sample_supported",true).as_bool()) label+=L" (empty)";
                else if (c.value("flags",0u).as_uint()!=0) label+=L" (alternate)";
                labels.push_back(std::move(label));
            }
            return labels;
        };
        auto labels=makeLabels(clips);
        const auto oldLabels=makeLabels(state_->clips);
        auto newClips=clips; auto newPath=path;
        const auto first=info.at("first_tick_ms").as_number(),last=info.at("last_tick_ms").as_number();
        bool selectedPresent=false;
        for (const auto& c:clips.as_array()) selectedPresent|=c.at("name").as_string()==info.at("clip").as_string();
        Monstey_Require(selectedPresent,"Animation clip is missing from completion catalog");
        const auto combo=GetDlgItem(state_->window,Monstey_PreviewClip);
        const auto install=[&](const auto& values) {
            SendMessageW(combo,CB_RESETCONTENT,0,0);
            for (const auto& label:values) {
                const auto added=SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
                if (added==CB_ERR || added==CB_ERRSPACE) return false;
            }
            return true;
        };
        if (!install(labels)) {
            state_->clipListValid=install(oldLabels);
            state_->SyncClip();
            throw Monstey_Error("preview","Cannot populate animation clips"+
                std::string(state_->clipListValid?"; previous list restored":"; reopen animation to recover the clip list"));
        }
        state_->animation=std::move(binding);state_->animationFrame=std::move(frame);
        state_->clips=std::move(newClips);state_->animationPath=std::move(newPath);state_->clipListValid=true;
        state_->firstTick=first;state_->lastTick=last;
        state_->animationLoading=false;state_->animationError.clear();state_->pendingAnimation.clear();
        state_->SyncClip();state_->Controls();state_->UpdateTimeline();state_->Refresh();UpdateWindow(state_->window);
        return state_->window && !state_->failed && state_->animationFrame;
    } catch (const std::exception& e) { SetAnimationError(request,e.what()); return false; }
}
bool Monstey_ModelPreviewWindow::SetAnimationError(std::uint64_t request,std::string_view error) {
    if (!AcceptsAnimation(request)) return false;
    state_->animationLoading=false;state_->animationError=Monstey_Wide(Monstey_DisplayUtf8(error));
    state_->pendingAnimation.clear();state_->SyncClip();state_->Controls();state_->Refresh();return true;
}
bool Monstey_ModelPreviewWindow::PlayAnimation(bool playing) {state_->Play(playing);return state_->playing;}
bool Monstey_ModelPreviewWindow::SeekAnimation(double tick) {return state_->Seek(tick);}
bool Monstey_ModelPreviewWindow::AdvanceAnimationClock(std::uint32_t now) {return state_->Advance(now);}
void Monstey_ModelPreviewWindow::Close()
{
    if (state_ && state_->window)
        DestroyWindow(state_->window);
}
bool Monstey_ModelPreviewWindow::RouteMessage(MSG &message)
{
    const auto window = state_->window;
    if (!window || !message.hwnd)
        return false;
    auto root = GetAncestor(message.hwnd, GA_ROOT);
    bool owned = root == window || message.hwnd == window || IsChild(window, message.hwnd);
    for (auto owner = GetWindow(root, GW_OWNER); !owned && owner; owner = GetWindow(owner, GW_OWNER))
        owned = owner == window || IsChild(window, owner);
    if (!owned)
        return false;
    if (message.hwnd==GetDlgItem(window,Monstey_PreviewTick) && message.message==WM_KEYDOWN && message.wParam==VK_RETURN) {
        state_->SeekEdit(); return true;
    }
    bool comboTarget=false;
    for (const auto id:{Monstey_PreviewContainer,Monstey_PreviewClip,Monstey_PreviewRate}) {
        const auto combo=GetDlgItem(window,id);
        const bool dropped=SendMessageW(combo,CB_GETDROPPEDSTATE,0,0)!=FALSE;
        if (message.message==WM_KEYDOWN && message.wParam==VK_ESCAPE && dropped) {
            SendMessageW(combo,CB_SHOWDROPDOWN,FALSE,0);return true;
        }
        comboTarget|=message.hwnd==combo || IsChild(combo,message.hwnd) || dropped;
    }
    const bool nativeComboKey=comboTarget && message.message==WM_KEYDOWN && message.wParam=='F';
    if ((nativeComboKey || !TranslateAcceleratorW(window, state_->accelerators.handle, &message)) && !IsDialogMessageW(window, &message))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}
Monstey_Json Monstey_ModelPreviewWindow::Inspect() const
{
    return Monstey_Json::object({{"animation_loading",state_->animationLoading},
                                 {"animation_ready",state_->animation && state_->animationFrame},
                                 {"playing",state_->playing},{"loop",state_->loop},{"rate",state_->rate},{"clip_list_valid",state_->clipListValid},
                                 {"last_clock",state_->lastClock},{"first_tick_ms",state_->firstTick},{"last_tick_ms",state_->lastTick},
                                 {"animation_error",Monstey_Utf8(state_->animationError)},
                                 {"animation",state_->animation ? state_->animation->Inspect() : Monstey_Json(nullptr)},
                                 {"animation_frame",state_->animationFrame ? state_->animationFrame->Inspect() : Monstey_Json(nullptr)},
                                 {"window_open", state_->window != nullptr},
                                 {"loading", state_->loading},
                                 {"ready", state_->scene != nullptr && !state_->loading && !state_->failed},
                                 {"failed", state_->failed},
                                 {"request", state_->request},
                                 {"renders", state_->renders},
                                 {"report", state_->frame.report},
                                 {"error", Monstey_Utf8(state_->error)},
                                 {"zoom", state_->options.zoom},
                                 {"yaw_degrees", state_->options.yaw_degrees},
                                 {"pitch_degrees", state_->options.pitch_degrees},
                                 {"pan_x", state_->options.pan_x},
                                 {"pan_y", state_->options.pan_y}});
}
Monstey_Bytes Monstey_ModelPreviewWindow::Snapshot()
{
    Monstey_Require(state_->window && state_->scene && !state_->loading && !state_->failed, "Preview is not ready");
    state_->Render();
    return Monstey_ModelPreviewBitmap(state_->frame);
}

} // namespace Monstey
