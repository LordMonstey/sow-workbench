#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

J Monstey_AExportClip(const Monstey_ALibrary &l, const Monstey_AClip &c)
{
    const bool empty = c.segments.empty();
    const auto ticks = Monstey_ASampleTicks(c);
    auto scene = empty ? J::object({{"clip", c.name},
                                    {"ticks", J::array()},
                                    {"tracks", J::array()},
                                    {"ignored_codecs", J::array()},
                                    {"complete_pose_codecs", true},
                                    {"complete_channel_decode", true},
                                    {"bind_fallback_applied", false},
                                    {"space", "native_y_up_cm_xyzw"},
                                    {"runtime_verified", false}})
                       : Monstey_ASample(l, c, ticks, false);
    scene["empty_native_clip"] = empty;
    scene["structural_only"] = empty;
    if (empty)
        scene["semantic_reimport_error"] =
            J::object({{"code", Monstey_AEmptyClipCode}, {"message", Monstey_AEmptyClipMessage}});
    auto unsupported = J::array();
    for (const auto &codec : Monstey_AUnsupportedAuxiliary(l, c))
        unsupported.as_array().emplace_back(codec);
    scene["unsupported_authoring_codecs"] = std::move(unsupported);
    scene["auxiliary_authoring_supported"] = scene["unsupported_authoring_codecs"].empty();
    scene["name"] = c.name;
    scene["duration_ticks"] = c.duration;
    scene["flags"] = c.flags;
    scene["unknown116"] = c.unknown116;
    scene["root_start"] = Monstey_AArray(c.root_start);
    scene["root_end"] = Monstey_AArray(c.root_end);
    if (const auto channel = Monstey_AClipEventChannel(l, c))
        scene["event_channel_index"] = *channel;
    scene["events"] = Monstey_AClipEvents(c);
    scene["extents_cm"] = Monstey_AArray(c.footer.extents);
    scene["source_codecs"] = J::array();
    for (const auto &code : c.codecs)
        scene["source_codecs"].as_array().emplace_back(code);
    const bool range_matches = !ticks.empty() && ticks.front() == 0 && ticks.back() == c.duration;
    scene["segment_range_matches_duration"] = range_matches;
    scene["semantic_reimport_supported"] = !empty && scene["complete_pose_codecs"].as_bool() &&
                                           scene["unsupported_authoring_codecs"].empty() && c.duration > 0 &&
                                           range_matches;
    if (scene["semantic_reimport_supported"].as_bool())
        try
        {
            (void)Monstey_AImportClip(l, scene);
        }
        catch (const Monstey_Error &error)
        {
            scene["semantic_reimport_supported"] = false;
            scene["semantic_reimport_error"] = J::object({{"code", error.code}, {"message", error.what()}});
        }
    return scene;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

J Monstey_Anix::ExportClip(std::string_view name) const
{
    return Monstey_AExportClip(*state_, state_->clips[Monstey_AFind(*state_, name)]);
}

J Monstey_Anix::Export() const
{
    J channels = J::array(), clips = J::array();
    for (const auto &c : state_->channels)
        channels.as_array().push_back(
            J::object({{"type_hash", Monstey_Hex(c.type)}, {"bone_hash", Monstey_Hex(c.bone)}}));
    for (const auto &c : state_->clips)
        clips.as_array().push_back(Monstey_AExportClip(*state_, c));
    return J::object({{"format", "Monstey_ANIX_Semantic_v1"},
                      {"library_path", state_->path},
                      {"channels", channels},
                      {"clips", clips},
                      {"source_sha256", Monstey_HashSha256(state_->original)},
                      {"runtime_verified", false}});
}

} // namespace Monstey
