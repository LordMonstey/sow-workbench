#include "Monstey_WorldSubmitContract.hpp"
namespace Monstey {
namespace {
void Monstey_WorldExtent(const Monstey_WorldSubmitSnapshot& s,std::uint64_t expected,std::size_t count) {
    if(!expected||s.address!=expected||s.bytes.size()<count)throw Monstey_Error("world_submit_snapshot","Required snapshot pointer/extent mismatch");
}
const Monstey_WorldSubmitSnapshot& Monstey_WorldRequired(const std::optional<Monstey_WorldSubmitSnapshot>& s,std::uint64_t expected,std::size_t count) {
    if(!s)throw Monstey_Error("world_submit_snapshot","Required native source snapshot absent");Monstey_WorldExtent(*s,expected,count);return *s;
}
std::string Monstey_WorldPointer(std::uint64_t p){return Monstey_Hex(p,16);}
}
Monstey_WorldModelResult Monstey_WorldResolveModel(const Monstey_WorldModelSnapshots& s) {
    Monstey_WorldExtent(s.context,s.context.address,56);Monstey_WorldModelResult r;r.context=s.context.address;
    r.wrapper=Monstey_U64(s.context.bytes,0);const auto& wrapper=Monstey_WorldRequired(s.wrapper,r.wrapper,26);
    r.handle=Monstey_U64(wrapper.bytes,0);
    const auto& handle=Monstey_WorldRequired(s.handle,r.handle,16);r.slot=Monstey_U64(handle.bytes,8);
    if(!r.slot){r.status=Monstey_WorldResolveStatus::NullSlot;r.diagnostic_45A24C=true;return r;}
    const auto& slot=Monstey_WorldRequired(s.slot,r.slot,16);r.container=Monstey_U64(slot.bytes,8);
    if(!r.container){r.status=Monstey_WorldResolveStatus::NullPayload;return r;}
    r.ordinal=Monstey_U16(wrapper.bytes,24);r.ordinal_read=true;const auto& container=Monstey_WorldRequired(s.container,r.container,12);r.count=Monstey_U32(container.bytes,8);r.count_read=true;
    if(r.ordinal>=r.count){r.status=Monstey_WorldResolveStatus::OrdinalHardStop;r.native_hard_stop=true;return r;}
    // Native SHL RBX,8; ADD RBX,QWORD[payload] uses unsigned 64-bit wrapping.
    r.model_array=Monstey_U64(container.bytes,0);r.model=r.model_array+(static_cast<std::uint64_t>(r.ordinal)<<8);r.status=Monstey_WorldResolveStatus::Resolved;return r;
}
Monstey_WorldGenericDrawResult Monstey_WorldGenericDrawInspection(const Monstey_WorldSubmitSnapshot& cull,const Monstey_WorldSubmitSnapshot& context,const Monstey_WorldModelResult& resolved,const std::optional<Monstey_WorldSubmitSnapshot>& model,const std::optional<Monstey_WorldSubmitSnapshot>& draw) {
    Monstey_WorldExtent(cull,cull.address,48);Monstey_WorldGenericDrawResult r;r.context=Monstey_U64(cull.bytes,32);Monstey_WorldExtent(context,r.context,56);if(resolved.context!=r.context)throw Monstey_Error("world_submit_snapshot","Resolver/context association mismatch");r.draw=Monstey_U16(cull.bytes,40);r.cull_mask=cull.bytes[42];r.cull_flag=cull.bytes[43];r.selected_255920_generic_branch=r.cull_flag==0;
    // Inspection of the resolver result is conditional; it does not claim this cull
    // was passed to 255920 or that a flag!=0 cull takes the generic call.
    r.native_hard_stop=resolved.native_hard_stop;r.model_result_available=!resolved.native_hard_stop;if(resolved.native_hard_stop||resolved.status!=Monstey_WorldResolveStatus::Resolved)return r;
    r.model=resolved.model;r.model_nonnull=r.model!=0;if(!r.model_nonnull)return r;
    const auto& m=Monstey_WorldRequired(model,r.model,28);r.draw_count=Monstey_U32(m.bytes,24);r.draw_in_range=r.draw<r.draw_count;r.model_header_read=true;
    if(!r.draw_in_range)return r;r.draw_pointer=Monstey_U64(m.bytes,16)+(static_cast<std::uint64_t>(r.draw)<<7);r.model_table0_pointer=Monstey_U64(m.bytes,0)+(static_cast<std::uint64_t>(r.draw)<<5);
    const auto& d=Monstey_WorldRequired(draw,r.draw_pointer,128);r.draw_record_read=true;r.material_slot=Monstey_U16(d.bytes,54);
    // 613A34 supplies material-array selector(arg7)=0, independently of render flag(arg5)=2.
    r.material_count=Monstey_U32(context.bytes,16);r.material_array=Monstey_U64(context.bytes,8);r.material_in_range=r.material_slot<r.material_count;return r;
}
Monstey_WorldCachedGateResult Monstey_WorldCachedGate(const Monstey_WorldSubmitSnapshot& object,const std::optional<Monstey_WorldSubmitSnapshot>& reference,const std::optional<Monstey_WorldSubmitSnapshot>& slot,const std::optional<Monstey_WorldSubmitSnapshot>& model) {
    Monstey_WorldExtent(object,object.address,96);Monstey_WorldCachedGateResult r;r.flag=object.bytes[56];r.draw=Monstey_U16(object.bytes,58);r.bit2_enabled=((r.flag>>2)&1)!=0;if(!r.bit2_enabled)return r;
    r.resource_reference=Monstey_U64(object.bytes,32);const auto& ref=Monstey_WorldRequired(reference,r.resource_reference,16);r.slot=Monstey_U64(ref.bytes,8);r.slot_nonnull=r.slot!=0;
    if(!r.slot_nonnull){r.diagnostic_45A24C=true;return r;}
    const auto& s=Monstey_WorldRequired(slot,r.slot,16);r.model=Monstey_U64(s.bytes,8);r.model_nonnull=r.model!=0;if(!r.model_nonnull)return r;
    const auto& m=Monstey_WorldRequired(model,r.model,28);r.draw_count=Monstey_U32(m.bytes,24);r.draw_in_range=r.draw<r.draw_count;return r;
}
std::uint64_t Monstey_WorldCachedEntryOwner(const Monstey_WorldSubmitSnapshot& entry){Monstey_WorldExtent(entry,entry.address,64);return Monstey_U64(entry.bytes,56);}
Monstey_Json Monstey_WorldDescribeModel(const Monstey_WorldModelResult& r) {
    const char* status=r.status==Monstey_WorldResolveStatus::Resolved?"RESOLVED_MODEL_RECORD":r.status==Monstey_WorldResolveStatus::NullSlot?"NULL_SLOT_DIAGNOSTIC_THEN_NULL_RESULT":r.status==Monstey_WorldResolveStatus::NullPayload?"NULL_PAYLOAD_RESULT":"NATIVE_ORDINAL_HARDSTOP_CONDITION";
    return Monstey_Json::object({{"status",status},{"context",Monstey_WorldPointer(r.context)},{"wrapper",Monstey_WorldPointer(r.wrapper)},{"handle",Monstey_WorldPointer(r.handle)},{"slot",Monstey_WorldPointer(r.slot)},{"container_payload",Monstey_WorldPointer(r.container)},{"container_model_array",r.count_read&&!r.native_hard_stop?Monstey_Json(Monstey_WorldPointer(r.model_array)):Monstey_Json()},{"ordinal_U16",r.ordinal_read?Monstey_Json(r.ordinal):Monstey_Json()},{"container_count_U32",r.count_read?Monstey_Json(r.count):Monstey_Json()},{"resolved_model_record",!r.native_hard_stop?Monstey_Json(Monstey_WorldPointer(r.model)):Monstey_Json()},{"model_stride",256},{"diagnostic_45A24C_would_be_called",r.diagnostic_45A24C},{"native_hardstop_condition",r.native_hard_stop},{"native_game_function_executed",false},{"GPU_or_draw_success_verified",false}});
}
Monstey_Json Monstey_WorldDescribeGeneric(const Monstey_WorldGenericDrawResult& r) {
    return Monstey_Json::object({{"cull_flag_U8_at_0x2B",r.cull_flag},{"cull_mask_U8_at_0x2A",r.cull_mask},{"draw_U16_at_0x28",r.draw},{"conditional_255920_second_vector_generic_call",r.selected_255920_generic_branch},{"actual_255920_vector_membership_observed",false},{"context",Monstey_WorldPointer(r.context)},{"model_record",r.model_result_available?Monstey_Json(Monstey_WorldPointer(r.model)):Monstey_Json()},{"native_hardstop_condition",r.native_hard_stop},{"model_nonnull",r.model_result_available?Monstey_Json(r.model_nonnull):Monstey_Json()},{"draw_count_U32",r.model_header_read?Monstey_Json(r.draw_count):Monstey_Json()},{"draw_index_in_range",r.model_header_read?Monstey_Json(r.draw_in_range):Monstey_Json()},{"draw_pointer_stride128",r.draw_in_range?Monstey_Json(Monstey_WorldPointer(r.draw_pointer)):Monstey_Json()},{"model_table0_pointer_stride32",r.draw_in_range?Monstey_Json(Monstey_WorldPointer(r.model_table0_pointer)):Monstey_Json()},{"material_slot_U16_at_draw0x36",r.draw_record_read?Monstey_Json(r.material_slot):Monstey_Json()},{"selected_material_array_selector_arg7",0},{"material_count_U32",r.draw_record_read?Monstey_Json(r.material_count):Monstey_Json()},{"material_array_pointer",r.draw_record_read?Monstey_Json(Monstey_WorldPointer(r.material_array)):Monstey_Json()},{"material_index_in_range",r.draw_record_read?Monstey_Json(r.material_in_range):Monstey_Json()},{"material_shader_resolution", "UNVERIFIED"},{"distance_helpers_256B1C_2597C4","OPAQUE"},{"cached_object_producer","UNRESOLVED"},{"actual_rendering_verified",false}});
}
std::uint8_t Monstey_WorldStaticPassMode(std::uint8_t view_mode,std::uint8_t requested_mode) {
    // 140613A18: literal3 is a sentinel before the unsigned minimum.
    if(view_mode==3 || requested_mode==3)return 3;
    return view_mode<requested_mode?view_mode:requested_mode;
}
bool Monstey_WorldStaticFlagAccepted(std::uint8_t flag,std::uint8_t mode) {
    // 140613AA0 zero-extends U8, decrements I32, then uses SETLE (signed).
    return static_cast<std::int32_t>(flag)-1<=static_cast<std::int32_t>(mode);
}
Monstey_WorldStaticPassResult Monstey_WorldStaticPassInspection(
    const Monstey_WorldSubmitSnapshot& cull,std::uint8_t view_mode,
    std::uint8_t requested_mode,std::uint8_t requested_mask) {
    Monstey_WorldExtent(cull,cull.address,48);
    Monstey_WorldStaticPassResult result;
    result.cull_flag=cull.bytes[43];result.cull_mask=cull.bytes[42];
    result.view_mode=view_mode;result.requested_mode=requested_mode;result.requested_mask=requested_mask;
    result.merged_mode=Monstey_WorldStaticPassMode(view_mode,requested_mode);
    result.mode_enabled=result.merged_mode!=3;
    result.flag_accepted=Monstey_WorldStaticFlagAccepted(result.cull_flag,result.merged_mode);
    result.conditional_613628_call=result.mode_enabled&&result.flag_accepted;
    result.conditional_25619C_call=result.conditional_613628_call&&((result.cull_mask&requested_mask)!=0);
    result.conditional_255920_generic_call=result.cull_flag==0;
    return result;
}
Monstey_Json Monstey_WorldDescribeStaticPass(const Monstey_WorldStaticPassResult& r) {
    return Monstey_Json::object({
        {"cull_flag_U8_at_0x2B",r.cull_flag},{"cull_mask_U8_at_0x2A",r.cull_mask},
        {"view_mode_U8_at_view0xE1",r.view_mode},{"requested_mode_U8",r.requested_mode},
        {"merged_mode_U8_613A18",r.merged_mode},{"literal3_disables_mode",!r.mode_enabled},
        {"flag_minus_one_signed_I32",static_cast<std::int32_t>(r.cull_flag)-1},
        {"conditional_613AA0_signed_comparison",r.flag_accepted},
        {"conditional_613628_call_613A34",r.conditional_613628_call},
        {"requested_mask_U8_25619C",r.requested_mask},
        {"conditional_25619C_call_613A34",r.conditional_25619C_call},
        {"conditional_255920_call_613A34",r.conditional_255920_generic_call},
        {"613628_25619C_render_flag_arg5",3},{"255920_render_flag_arg5",2},
        {"material_array_selector_arg7",0},
        {"actual_gather_membership_observed",false},{"actual_view_mode_observed",false},
        {"model_material_shader_readiness","UNVERIFIED"},
        {"native_game_function_executed",false},{"actual_rendering_verified",false}});
}
}