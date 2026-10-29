#include "animation/Monstey_AnixPrivate.hpp"

namespace Monstey
{
using namespace Monstey_AnixPrivate;

Monstey_Anix::Monstey_Anix(std::span<const std::uint8_t> bytes) : state_(std::make_unique<Monstey_State>())
{
    static_cast<Monstey_ALibrary &>(*state_) = Monstey_AParse(bytes);
}

Monstey_Anix::Monstey_Anix(std::unique_ptr<Monstey_State> state) : state_(std::move(state)) {}

Monstey_Anix::~Monstey_Anix() = default;

Monstey_Anix::Monstey_Anix(const Monstey_Anix &o) : state_(std::make_unique<Monstey_State>(*o.state_)) {}

Monstey_Anix &Monstey_Anix::operator=(const Monstey_Anix &o)
{
    if (this != &o)
        state_ = std::make_unique<Monstey_State>(*o.state_);
    return *this;
}

Monstey_Anix::Monstey_Anix(Monstey_Anix &&) noexcept = default;

Monstey_Anix &Monstey_Anix::operator=(Monstey_Anix &&) noexcept = default;

} // namespace Monstey
