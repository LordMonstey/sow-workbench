// Private qualification of the selected bank helper, not a public test hook.
#include "../src/Monstey_AnimationPreview.cpp"
#include <iostream>
using namespace Monstey;
namespace
{
// The historical negative control must remain independent of corrected production helpers.
namespace Monstey_LegacyBankScalar
{
#include "Monstey_Legacy017.inc"
}
void Monstey_Test(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
Monstey_Json Monstey_JsonWords(std::span<const std::uint32_t> words)
{
    auto result = Monstey_Json::array();
    for (auto word : words)
        result.as_array().emplace_back(Monstey_Hex(word));
    return result;
}
} // namespace
int main()
{
    try
    {
        constexpr std::array<std::uint32_t, 4> current_words{0x3d180520, 0x3f048c8e, 0x3f470592, 0xbeb5d817};
        constexpr std::array<std::uint32_t, 4> inverse_words{0x3efcc539, 0x3ec1fc0d, 0x3e994b72, 0xbf39265b};
        constexpr std::array<std::uint32_t, 4> expected_q{0xbeaef4cc, 0xbe0b9c0b, 0xbf690077, 0xbe42f55f};
        constexpr std::array<std::uint32_t, 4> old_q{0xbeaef4cc, 0xbe0b9c0a, 0xbf690078, 0xbe42f55e};
        constexpr std::array<std::uint32_t, 3> current_xyz{0x40500000, 0xc0d00000, 0x3e000000};
        constexpr std::array<std::uint32_t, 3> inverse_xyz{0x41480000, 0x80000000, 0x40980000};
        constexpr std::array<std::uint32_t, 3> expected_xyz{0xc0f11de4, 0xc10f9240, 0x40f447e7};
        constexpr std::array<std::uint32_t, 3> old_xyz{0xc0f11de4, 0xc10f923f, 0x40f447e8};
        Monstey_Pose current, inverse;
        for (unsigned k = 0; k < 4; ++k)
        {
            current.quaternion_xyzw[k] = std::bit_cast<float>(current_words[k]);
            inverse.quaternion_xyzw[k] = std::bit_cast<float>(inverse_words[k]);
        }
        for (unsigned k = 0; k < 3; ++k)
        {
            current.position_cm[k] = std::bit_cast<float>(current_xyz[k]);
            inverse.position_cm[k] = std::bit_cast<float>(inverse_xyz[k]);
        }
        const auto result = Monstey_APBankCompose(current, inverse);
        const Monstey_LegacyBankScalar::Monstey_Pose old_current{current.position_cm,current.quaternion_xyzw};
        const Monstey_LegacyBankScalar::Monstey_Pose old_inverse{inverse.position_cm,inverse.quaternion_xyzw};
        const auto old = Monstey_LegacyBankScalar::Monstey_APCompose(old_current, old_inverse);
        for (unsigned k = 0; k < 4; ++k)
        {
            Monstey_Test(std::bit_cast<std::uint32_t>(result.quaternion_xyzw[k]) == expected_q[k],
                         "Bank quaternion differs from selected native SIMD words");
            Monstey_Test(std::bit_cast<std::uint32_t>(old.quaternion_xyzw[k]) == old_q[k],
                         "Scalar negative control differs from independently pinned words");
        }
        for (unsigned k = 0; k < 3; ++k)
        {
            Monstey_Test(std::bit_cast<std::uint32_t>(result.position_cm[k]) == expected_xyz[k],
                         "Bank position differs from selected native SIMD words");
            Monstey_Test(std::bit_cast<std::uint32_t>(old.position_cm[k]) == old_xyz[k],
                         "Scalar position negative control differs from pinned words");
        }
        const auto reversed = Monstey_APBankCompose(inverse, current);
        double reversed_difference = 0;
        for (unsigned k = 0; k < 4; ++k)
            reversed_difference += std::abs(result.quaternion_xyzw[k] - reversed.quaternion_xyzw[k]);
        Monstey_Test(reversed_difference > 0.1, "Reversed operand negative control did not differentiate");
        auto nc = current, ni = inverse;
        for (float &q : nc.quaternion_xyzw)
            q *= 2;
        for (float &q : ni.quaternion_xyzw)
            q *= 3;
        const auto nonunit = Monstey_APBankCompose(nc, ni);
        double norm = 0;
        for (float q : nonunit.quaternion_xyzw)
            norm += double(q) * q;
        Monstey_Test(norm > 35 && norm < 37, "Bank helper normalized nonunit quaternion inputs");
        auto report = Monstey_Json::object(
            {{"status", "passed"},
             {"groups", 4},
             {"selected_bank_rva", "0x183B90"},
             {"selected_rotate_rva", "0x34F540"},
             {"native_proof", "bank-producer-continuation-001/native-byteproof-037.json"},
             {"current_q_words", Monstey_JsonWords(current_words)},
             {"inverse_q_words", Monstey_JsonWords(inverse_words)},
             {"expected_bank_q_words", Monstey_JsonWords(expected_q)},
             {"old_scalar_q_words", Monstey_JsonWords(old_q)},
             {"scalar_negative_control_source", "Frozen checkpoint017 scalar helpers; independent of production hierarchy"},
             {"current_xyz_words", Monstey_JsonWords(current_xyz)},
             {"inverse_xyz_words", Monstey_JsonWords(inverse_xyz)},
             {"expected_bank_xyz_words", Monstey_JsonWords(expected_xyz)},
             {"old_scalar_xyz_words", Monstey_JsonWords(old_xyz)},
             {"full_native_gpu_verified", false},
             {"full_hierarchy_fp_order_verified", false}});
        std::cout << report.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
