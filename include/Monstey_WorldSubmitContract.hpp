#pragma once
#include "Monstey_Core.hpp"
#include <optional>
namespace Monstey {
struct Monstey_WorldSubmitSnapshot { std::uint64_t address{}; Monstey_Bytes bytes; };
struct Monstey_WorldModelSnapshots {
    Monstey_WorldSubmitSnapshot context;
    std::optional<Monstey_WorldSubmitSnapshot> wrapper,handle,slot,container;
};
enum class Monstey_WorldResolveStatus { NullSlot,NullPayload,OrdinalHardStop,Resolved };
struct Monstey_WorldModelResult {
    Monstey_WorldResolveStatus status{};
    std::uint64_t context{},wrapper{},handle{},slot{},container{},model_array{},model{};
    std::uint16_t ordinal{};
    std::uint32_t count{};
    bool diagnostic_45A24C{},native_hard_stop{},ordinal_read{},count_read{};
};
// Pure reads of caller-owned snapshots. No pointers are dereferenced or retained.
// Non-null required source pointers and exact supplied-address/extent agreement are
// offline preconditions. Out-of-range native ordinal reports a hard stop, never executes it.
Monstey_WorldModelResult Monstey_WorldResolveModel(const Monstey_WorldModelSnapshots& sources);
struct Monstey_WorldGenericDrawResult {
    std::uint16_t draw{},material_slot{};
    std::uint32_t draw_count{},material_count{};
    std::uint64_t context{},model{},draw_pointer{},model_table0_pointer{},material_array{};
    std::uint8_t cull_flag{},cull_mask{};
    bool selected_255920_generic_branch{},model_nonnull{},draw_in_range{},material_in_range{};
    bool model_result_available{},native_hard_stop{},model_header_read{},draw_record_read{};
};
Monstey_WorldGenericDrawResult Monstey_WorldGenericDrawInspection(
    const Monstey_WorldSubmitSnapshot& cull,const Monstey_WorldSubmitSnapshot& context,
    const Monstey_WorldModelResult& resolved,
    const std::optional<Monstey_WorldSubmitSnapshot>& model,
    const std::optional<Monstey_WorldSubmitSnapshot>& draw);
struct Monstey_WorldCachedGateResult {
    std::uint8_t flag{}; std::uint16_t draw{};std::uint32_t draw_count{};
    std::uint64_t resource_reference{},slot{},model{};
    bool bit2_enabled{},slot_nonnull{},model_nonnull{},draw_in_range{},diagnostic_45A24C{};
};
// Source is an expanded render object (selected 96-byte prefix including transform),
// not the distinct 64-byte broadphase entry that contains a pointer at +0x38.
Monstey_WorldCachedGateResult Monstey_WorldCachedGate(
    const Monstey_WorldSubmitSnapshot& expanded_object,
    const std::optional<Monstey_WorldSubmitSnapshot>& reference,
    const std::optional<Monstey_WorldSubmitSnapshot>& slot,
    const std::optional<Monstey_WorldSubmitSnapshot>& model);
std::uint64_t Monstey_WorldCachedEntryOwner(const Monstey_WorldSubmitSnapshot& entry64);
Monstey_Json Monstey_WorldDescribeModel(const Monstey_WorldModelResult& result);
Monstey_Json Monstey_WorldDescribeGeneric(const Monstey_WorldGenericDrawResult& result);
// Exact selected native predicates. Modes are raw U8: literal3 disables the
// combined pass; other values use an unsigned minimum. Flag acceptance itself
// uses signed I32(flag)-1 <= I32(mode), including flag0 -> -1.
std::uint8_t Monstey_WorldStaticPassMode(std::uint8_t view_mode,std::uint8_t requested_mode);
bool Monstey_WorldStaticFlagAccepted(std::uint8_t cull_flag,std::uint8_t mode);
struct Monstey_WorldStaticPassResult {
    std::uint8_t cull_flag{},cull_mask{},view_mode{},requested_mode{},merged_mode{},requested_mask{};
    bool mode_enabled{},flag_accepted{},conditional_613628_call{},conditional_25619C_call{};
    bool conditional_255920_generic_call{};
};
// Inspects only the supplied48-byte cull and explicit caller inputs. It does not
// establish actual gather membership, view state, resource readiness or a draw.
Monstey_WorldStaticPassResult Monstey_WorldStaticPassInspection(
    const Monstey_WorldSubmitSnapshot& cull,std::uint8_t view_mode,
    std::uint8_t requested_mode,std::uint8_t requested_mask);
Monstey_Json Monstey_WorldDescribeStaticPass(const Monstey_WorldStaticPassResult& result);
}