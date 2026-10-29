#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
namespace Monstey {
// Owned boundary targets retain identity and mutable state. Copying this holder
// shares the configured target; it does not clone the std::function closure.
// Replacing/resetting a holder publishes a new target. An active invocation keeps
// its previous target alive through return/exception, including reentrant calls.
// This is a C++ lifetime contract, not an original engine/CRT ABI or thread lock.
template<class Signature> class Monstey_FearDbCallable;
template<class Result,class... Args> class Monstey_FearDbCallable<Result(Args...)> {
 using Monstey_Function=std::function<Result(Args...)>;
 std::shared_ptr<Monstey_Function> Monstey_target;
public:
 Monstey_FearDbCallable() noexcept=default;
 Monstey_FearDbCallable(const Monstey_FearDbCallable&) noexcept=default;
 Monstey_FearDbCallable(Monstey_FearDbCallable&&) noexcept=default;
 Monstey_FearDbCallable& operator=(const Monstey_FearDbCallable&) noexcept=default;
 Monstey_FearDbCallable& operator=(Monstey_FearDbCallable&&) noexcept=default;
 template<class Function> requires (!std::is_same_v<std::remove_cvref_t<Function>,Monstey_FearDbCallable> &&
                                  std::is_constructible_v<Monstey_Function,Function>)
 Monstey_FearDbCallable(Function&& function)
  :Monstey_target(std::make_shared<Monstey_Function>(std::forward<Function>(function))) {}
 template<class Function> requires (!std::is_same_v<std::remove_cvref_t<Function>,Monstey_FearDbCallable> &&
                                  std::is_constructible_v<Monstey_Function,Function>)
 Monstey_FearDbCallable& operator=(Function&& function) {
  auto next=std::make_shared<Monstey_Function>(std::forward<Function>(function));
  Monstey_target=std::move(next);return *this;
 }
 explicit operator bool() const noexcept {return Monstey_target&&static_cast<bool>(*Monstey_target);}
 Result operator()(Args... args) const {
  const auto active=Monstey_target;
  if(!active||!*active)throw std::bad_function_call();
  return (*active)(std::forward<Args>(args)...);
 }
};
}
