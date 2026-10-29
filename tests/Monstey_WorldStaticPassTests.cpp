#include "Monstey_WorldSubmitContract.hpp"
#include <functional>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace {
void Monstey_Check(bool value){if(!value)throw std::runtime_error("Static pass assertion failed");}
void Monstey_Reject(const std::function<void()>& action){bool caught=false;try{action();}catch(const Monstey_Error&){caught=true;}Monstey_Check(caught);}
Monstey_WorldSubmitSnapshot Monstey_Cull(std::uint8_t flag=1,std::uint8_t mask=4){Monstey_WorldSubmitSnapshot s{0x10000,Monstey_Bytes(48,0)};s.bytes[42]=mask;s.bytes[43]=flag;return s;}
}
int wmain(int argc,wchar_t** argv){try{
    Monstey_Require(argc==2,"Usage: StaticPassTests NEW-REPORT.json");Monstey_RequireOfflineOutput(argv[1]);
    Monstey_Json::Array passed;auto Test=[&](const char* name,const std::function<void()>& body){body();passed.push_back(name);};
    Test("native_signed_SETLE_flag0_and_flag1_accept_mode0",[]{
        Monstey_Check(Monstey_WorldStaticFlagAccepted(0,0));Monstey_Check(Monstey_WorldStaticFlagAccepted(1,0));
        Monstey_Check(!Monstey_WorldStaticFlagAccepted(2,0));Monstey_Check(Monstey_WorldStaticFlagAccepted(2,1));
        Monstey_Check(!Monstey_WorldStaticFlagAccepted(3,1));Monstey_Check(Monstey_WorldStaticFlagAccepted(3,2));
        Monstey_Check(!Monstey_WorldStaticFlagAccepted(4,2));Monstey_Check(Monstey_WorldStaticFlagAccepted(255,254));
    });
    Test("all65536_U8_flags_and_modes_match_independent_threshold_oracle",[]{
        unsigned unsigned_dec_differences=0;
        for(unsigned flag=0;flag<256;++flag)for(unsigned mode=0;mode<256;++mode){
            const bool expected=flag<=mode+1u;
            Monstey_Check(Monstey_WorldStaticFlagAccepted(static_cast<std::uint8_t>(flag),static_cast<std::uint8_t>(mode))==expected);
            const auto wrong=static_cast<std::uint32_t>(flag)-1u;
            if((wrong<=mode)!=expected)++unsigned_dec_differences;
        }
        Monstey_Check(unsigned_dec_differences==256);
    });
    Test("all65536_U8_mode_pairs_preserve_literal3_sentinel_and_unsigned_min",[]{
        for(unsigned view=0;view<256;++view)for(unsigned requested=0;requested<256;++requested){
            unsigned expected=3;
            if(view!=3&&requested!=3)expected=view<=requested?view:requested;
            Monstey_Check(Monstey_WorldStaticPassMode(static_cast<std::uint8_t>(view),static_cast<std::uint8_t>(requested))==expected);
        }
        Monstey_Check(Monstey_WorldStaticPassMode(3,0)==3);Monstey_Check(Monstey_WorldStaticPassMode(0,3)==3);
        Monstey_Check(Monstey_WorldStaticPassMode(200,255)==200);Monstey_Check(Monstey_WorldStaticPassMode(255,255)==255);
    });
    Test("all65536_inspection_flag_mode_cases_keep_passes_separate",[]{
        for(unsigned flag=0;flag<256;++flag)for(unsigned mode=0;mode<256;++mode){
            auto cull=Monstey_Cull(static_cast<std::uint8_t>(flag));
            const auto r=Monstey_WorldStaticPassInspection(cull,255,static_cast<std::uint8_t>(mode),4);
            Monstey_Check(r.merged_mode==mode);
            const bool expected=mode!=3&&flag<=mode+1u;
            Monstey_Check(r.conditional_613628_call==expected&&r.conditional_25619C_call==expected);
            Monstey_Check(r.conditional_255920_generic_call==(flag==0));
        }
    });
    Test("all65536_mask_pairs_use_cull2A_and_requested_mask_only_in25619C",[]{
        for(unsigned mask=0;mask<256;++mask)for(unsigned request=0;request<256;++request){
            const auto r=Monstey_WorldStaticPassInspection(Monstey_Cull(1,static_cast<std::uint8_t>(mask)),0,0,static_cast<std::uint8_t>(request));
            Monstey_Check(r.conditional_613628_call);
            Monstey_Check(r.conditional_25619C_call==((mask&request)!=0));
            Monstey_Check(!r.conditional_255920_generic_call);
        }
    });
    Test("literal3_disables_call_even_when_raw_signed_predicate_accepts",[]{
        const auto a=Monstey_WorldStaticPassInspection(Monstey_Cull(0),3,0,255);
        const auto b=Monstey_WorldStaticPassInspection(Monstey_Cull(1),0,3,255);
        Monstey_Check(a.flag_accepted&&b.flag_accepted);
        Monstey_Check(!a.mode_enabled&&!a.conditional_613628_call&&!a.conditional_25619C_call);
        Monstey_Check(!b.mode_enabled&&!b.conditional_613628_call&&!b.conditional_25619C_call);
        Monstey_Check(a.conditional_255920_generic_call&&!b.conditional_255920_generic_call);
    });
    Test("flag1_has_positive_common_static_route_despite255920_skip",[]{
        const auto r=Monstey_WorldStaticPassInspection(Monstey_Cull(1,4),2,0,4);
        Monstey_Check(r.merged_mode==0&&r.flag_accepted&&r.conditional_613628_call&&r.conditional_25619C_call&&!r.conditional_255920_generic_call);
    });
    Test("all48_short_prefixes_and_null_address_reject",[]{
        for(std::size_t count=0;count<48;++count){auto s=Monstey_Cull();s.bytes.resize(count);Monstey_Reject([&]{(void)Monstey_WorldStaticPassInspection(s,0,0,4);});}
        auto s=Monstey_Cull();s.address=0;Monstey_Reject([&]{(void)Monstey_WorldStaticPassInspection(s,0,0,4);});
    });
    Test("result_owns_values_and_full_snapshot_is_unchanged",[]{
        auto s=Monstey_Cull(2,5);const auto before=s.bytes;const auto r=Monstey_WorldStaticPassInspection(s,2,1,4);
        Monstey_Check(before==s.bytes);s.bytes[42]=0;s.bytes[43]=255;
        Monstey_Check(r.cull_flag==2&&r.cull_mask==5&&r.conditional_613628_call&&r.conditional_25619C_call);
    });
    Test("description_preserves_raw_signed_minusone_and_unverified_boundaries",[]{
        const auto j=Monstey_WorldDescribeStaticPass(Monstey_WorldStaticPassInspection(Monstey_Cull(0),0,0,4));
        Monstey_Check(j.at("flag_minus_one_signed_I32").as_int()==-1);
        Monstey_Check(!j.at("actual_gather_membership_observed").as_bool()&&!j.at("actual_view_mode_observed").as_bool());
        Monstey_Check(!j.at("native_game_function_executed").as_bool()&&!j.at("actual_rendering_verified").as_bool());
        Monstey_Check(j.at("model_material_shader_readiness").as_string()=="UNVERIFIED");
        Monstey_Check(j.at("613628_25619C_render_flag_arg5").as_int()==3&&j.at("255920_render_flag_arg5").as_int()==2);
        Monstey_Check(Monstey_Json::Parse(j.dump())==j);
    });
    const auto result=Monstey_Json::object({{"schema","Monstey.WorldStaticPassTests/1"},{"passed",passed},{"groups",static_cast<std::uint64_t>(passed.size())},{"predicate_cases",65536},{"merged_mode_cases",65536},{"inspector_flag_mode_cases",65536},{"mask_cases",65536},{"unsigned_decrement_negative_control_differences",256},{"short_snapshot_negatives",48},{"original_programs_executed",false},{"runtime_visibility_verified",false}});
    Monstey_WriteNewText(argv[1],result.dump(2)+"\n");std::cout<<"PASS "<<passed.size()<<" static pass groups\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
