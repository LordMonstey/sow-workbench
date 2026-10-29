#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
namespace Monstey {
// Native32 identities inside caller-owned stable storage, never host pointers.
struct Monstey_FearDbMemory {std::uint32_t base=0;std::span<const std::uint8_t> bytes;};
struct Monstey_FearDbMutableMemory {std::uint32_t base=0;std::span<std::uint8_t> bytes;};
class Monstey_FearDbFault:public std::runtime_error {
public:std::uint32_t address;std::uint32_t width;bool writing;
 Monstey_FearDbFault(std::uint32_t at,std::uint32_t count,bool write);
};
std::uint32_t Monstey_FearDbNodeName(Monstey_FearDbMemory,std::uint32_t handle); //6610
std::uint32_t Monstey_FearDbNodeParentOrDatabaseName(Monstey_FearDbMemory,std::uint32_t handle); //6750
std::uint32_t Monstey_FearDbChildCount(Monstey_FearDbMemory,std::uint32_t handle); //6710
std::uint32_t Monstey_FearDbChildByIndex(Monstey_FearDbMemory,std::uint32_t parent,std::uint32_t index); //6680
//6730 reads no count and returns the wrapping computed identity even out of range.
std::uint32_t Monstey_FearDbAttributeByIndex(Monstey_FearDbMemory,std::uint32_t record,std::uint32_t index);
std::uint32_t Monstey_FearDbHandleIndex(Monstey_FearDbMemory,std::uint32_t handle); //6790
std::uint32_t Monstey_FearDbValueCount(Monstey_FearDbMemory,std::uint32_t attribute); //6770
std::uint32_t Monstey_FearDbAttributeType(Monstey_FearDbMemory,std::uint32_t attribute); //67B0
std::uint32_t Monstey_FearDbAttributeUsage(Monstey_FearDbMemory,std::uint32_t attribute); //67D0
std::uint32_t Monstey_FearDbInt32Bits(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback); //6850
//6880 payload selection before native FLD. Original x87 return/control-state ABI is not claimed.
std::uint32_t Monstey_FearDbFloat32Bits(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback);
std::uint32_t Monstey_FearDbStringIdentity(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback); //68C0
std::uint32_t Monstey_FearDbWStringIdentity(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback); //68F0
std::uint32_t Monstey_FearDbRecordIdentity(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback); //6A20
//69F0 returns AL unchanged, not a normalized whole-word boolean.
std::uint8_t Monstey_FearDbBoolByte(Monstey_FearDbMemory,std::uint32_t attribute,std::uint32_t index,std::uint8_t fallback);
//DWORD-by-DWORD copies retain overlapping input/default/output behavior and partial writes.
std::uint32_t Monstey_FearDbVector2(Monstey_FearDbMutableMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback,std::uint32_t output); //6920
std::uint32_t Monstey_FearDbVector3(Monstey_FearDbMutableMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback,std::uint32_t output); //6970
std::uint32_t Monstey_FearDbVector4(Monstey_FearDbMutableMemory,std::uint32_t attribute,std::uint32_t index,std::uint32_t fallback,std::uint32_t output); //69D0->67F0
} //namespace Monstey
