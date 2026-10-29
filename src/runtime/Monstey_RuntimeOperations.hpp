#pragma once
#include <span>
namespace Monstey::Monstey_RuntimeDetail
{
struct Monstey_Operation
{
    const char *name;
    const char *description;
    bool read;
    const char *fields;
};
std::span<const Monstey_Operation> Monstey_RuntimeOperations();
} // namespace Monstey::Monstey_RuntimeDetail
