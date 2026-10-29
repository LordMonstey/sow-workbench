#pragma once

#include "Monstey_AppSupport.hpp"
#include "Monstey_CurveControl.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace Monstey::Monstey_AppInternal
{

class Monstey_App;

struct Monstey_Inspector
{
    Monstey_App *app = nullptr;
    HWND window = nullptr, list = nullptr;
    J metadata = J::object();
    void Create();
    void Populate();
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
};

struct Monstey_Editor : std::enable_shared_from_this<Monstey_Editor>
{
    Monstey_App *app = nullptr;
    HWND window = nullptr, input = nullptr, combo = nullptr, status = nullptr;
    std::vector<HWND> components, formControls, formLabels;
    J formFields = J::array();
    bool lineList = false;
    std::shared_ptr<Monstey_CurveControl> curve;
    J field = J::object(), arguments = J::object();
    std::string operation, valueKey = "value";
    std::wstring title;
    std::uint64_t generation = 0, revision = 0;
    bool plainText = false, pending = false;
    std::function<void(const J &)> success;
    std::function<void(const J &)> validate;
    void Create();
    void Apply();
    void Close();
    void Layout();
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
};

} // namespace Monstey::Monstey_AppInternal
