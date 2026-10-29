#include "Monstey_TextureResource.hpp"
#include "BC.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <optional>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_TRequire(bool v, const char *m) {
  if (!v)
    throw Monstey_Error("texture_resource", m);
}
std::uint64_t Monstey_TAdd(std::uint64_t a, std::uint64_t b) {
  Monstey_TRequire(b <= UINT64_MAX - a, "Texture addition overflow");
  return a + b;
}
std::uint64_t Monstey_TMul(std::uint64_t a, std::uint64_t b) {
  Monstey_TRequire(!b || a <= UINT64_MAX / b,
                   "Texture multiplication overflow");
  return a * b;
}
std::string Monstey_THex(std::span<const std::uint8_t> b) {
  std::string s;
  for (auto v : b) {
    s += "0123456789abcdef"[v >> 4];
    s += "0123456789abcdef"[v & 15];
  }
  return s;
}
std::string Monstey_TText(std::span<const std::uint8_t> b) {
  return Monstey_DisplayUtf8(
      std::string(reinterpret_cast<const char *>(b.data()), b.size()));
}
std::string Monstey_TExpected(std::string s) {
  Monstey_TRequire(s.size() == 64, "Texture source SHA256 needs64 hex digits");
  for (auto &c : s) {
    if (c >= 'A' && c <= 'F')
      c = static_cast<char>(c - 'A' + 'a');
    Monstey_TRequire((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
                     "Invalid texture source SHA256");
  }
  return s;
}
struct Monstey_TFormat {
  std::uint32_t dxgi = 0, block_bytes = 0, pixel_bytes = 0;
  const char *name = "UNKNOWN";
  bool srgb = false;
};
Monstey_TFormat Monstey_TFormatOf(std::uint32_t d) {
  switch (d) {
  case 71:
    return {d, 8, 0, "BC1_UNORM", false};
  case 72:
    return {d, 8, 0, "BC1_UNORM_SRGB", true};
  case 74:
    return {d, 16, 0, "BC2_UNORM", false};
  case 75:
    return {d, 16, 0, "BC2_UNORM_SRGB", true};
  case 77:
    return {d, 16, 0, "BC3_UNORM", false};
  case 78:
    return {d, 16, 0, "BC3_UNORM_SRGB", true};
  case 80:
    return {d, 8, 0, "BC4_UNORM", false};
  case 81:
    return {d, 8, 0, "BC4_SNORM", false};
  case 83:
    return {d, 16, 0, "BC5_UNORM", false};
  case 84:
    return {d, 16, 0, "BC5_SNORM", false};
  case 95:
    return {d, 16, 0, "BC6H_UF16", false};
  case 96:
    return {d, 16, 0, "BC6H_SF16", false};
  case 98:
    return {d, 16, 0, "BC7_UNORM", false};
  case 99:
    return {d, 16, 0, "BC7_UNORM_SRGB", true};
  case 28:
    return {d, 0, 4, "R8G8B8A8_UNORM", false};
  case 29:
    return {d, 0, 4, "R8G8B8A8_UNORM_SRGB", true};
  case 87:
    return {d, 0, 4, "B8G8R8A8_UNORM", false};
  case 91:
    return {d, 0, 4, "B8G8R8A8_UNORM_SRGB", true};
  case 88:
    return {d, 0, 4, "B8G8R8X8_UNORM", false};
  case 93:
    return {d, 0, 4, "B8G8R8X8_UNORM_SRGB", true};
  case 61:
    return {d, 0, 1, "R8_UNORM", false};
  case 49:
    return {d, 0, 2, "R8G8_UNORM", false};
  case 41:
    return {d, 0, 4, "R32_FLOAT", false};
  default:
    return {d, 0, 0, "UNKNOWN", false};
  }
}
struct Monstey_TParsed {
  J header = J::object();
  std::vector<Monstey_TextureSubresource> subresources;
  Monstey_TFormat format;
  std::size_t dds_offset = 0, payload_offset = 0;
  bool layout = false, cpu = false, decode = false;
  std::string reason;
};
Monstey_TParsed Monstey_TParse(std::span<const std::uint8_t> b) {
  Monstey_TParsed p;
  Monstey_CheckRange(b, 0, 4);
  const auto magic = Monstey_U32(b, 0);
  std::size_t at = 0;
  std::uint32_t logicalWidth = 0, logicalHeight = 0, logicalDepth = 1,
                logicalMips = 0, nativeLayers = 1, nativeKind = 0;
  bool native = false, arrayNative = false, volumeNative = false;
  p.header["container_magic_hex"] = Monstey_THex(b.first(4));
  if (magic == 0x54555043) {
    Monstey_CheckRange(b, 0, 8);
    const auto version = Monstey_U32(b, 4);
    p.header["container"] = "CPUT";
    p.header["version"] = version;
    p.cpu = true;
    if (version != 1) {
      p.reason = "Unqualified CPUT version; raw source preserved";
      return p;
    }
    const auto n = Monstey_U16(b, 8);
    Monstey_CheckRange(b, 10, n);
    at = 10 + std::size_t(n);
    Monstey_CheckRange(b, at, 12);
    const auto w = Monstey_U32(b, at), h = Monstey_U32(b, at + 4),
               bits = Monstey_U32(b, at + 8);
    p.payload_offset = at + 12;
    p.header["resource_name_raw_hex"] = Monstey_THex(b.subspan(10, n));
    p.header["width"] = w;
    p.header["height"] = h;
    p.header["stored_bits_per_element"] = bits;
    p.header["native_bytes_per_element_shift3"] = bits >> 3;
    Monstey_TRequire(w && h, "CPUT zero extent");
    const auto row = Monstey_TMul(w, bits >> 3), size = Monstey_TMul(row, h);
    Monstey_TRequire((bits >> 3) > 0, "CPUT element size is zero");
    Monstey_TRequire(size <= SIZE_MAX,
                     "CPUT payload exceeds addressable range");
    Monstey_CheckRange(b, p.payload_offset, static_cast<std::size_t>(size));
    p.subresources.push_back(
        {0, 0, 0, w, h, 1, p.payload_offset, size, row, size});
    p.layout = true;
    p.reason =
        "CPUT raw element storage has no declared color/channel encoding";
    p.header["trailing_bytes"] = b.size() - p.payload_offset - size;
    p.header["format"] = "CPUT_OPAQUE_ELEMENTS";
    return p;
  }
  if (magic == 0x52584554 || magic == 0x41584554) {
    native = true;
    Monstey_CheckRange(b, 0, 8);
    const auto version = Monstey_U32(b, 4);
    arrayNative = magic == 0x41584554;
    p.header["container"] = arrayNative ? "TEXA" : "TEXR";
    p.header["version"] = version;
    if (version != (arrayNative ? 1U : 8U)) {
      p.reason = "Unqualified native container version; raw source preserved";
      return p;
    }
    Monstey_CheckRange(b, 0, 18);
    const auto n = Monstey_U16(b, 16);
    Monstey_CheckRange(b, 18, n);
    const auto meta = 18 + std::size_t(n);
    Monstey_CheckRange(b, meta, 1);
    nativeKind = b[meta];
    volumeNative = nativeKind == 5;
    if ((arrayNative && nativeKind != 4) ||
        (!arrayNative && nativeKind != 3 && nativeKind != 5)) {
      p.reason = "Unqualified native texture kind; raw source preserved";
      return p;
    }
    const auto metadataBytes = volumeNative ? 22U : 23U;
    Monstey_CheckRange(b, meta, metadataBytes);
    logicalWidth = Monstey_U16(b, meta + 1);
    logicalHeight = Monstey_U16(b, meta + 3);
    logicalDepth = volumeNative ? Monstey_U16(b, meta + 5) : 1;
    nativeLayers = arrayNative ? Monstey_U16(b, meta + 5) : 1;
    logicalMips = b[meta + 7];
    const auto dxgi = Monstey_U32(b, meta + (volumeNative ? 8 : 9));
    p.header["native_metadata_format_raw"] = dxgi;
    p.header["native_kind"] = nativeKind;
    p.header["native_metadata_bytes"] = metadataBytes;
    p.header["native_metadata_hex"] =
        Monstey_THex(b.subspan(meta, metadataBytes));
    p.header["native_header8_15_hex"] = Monstey_THex(b.subspan(8, 8));
    p.header["native_peer_raw_hex"] = Monstey_THex(b.subspan(18, n));
    p.header["native_logical_dimensions"] =
        J::array({logicalWidth, logicalHeight, logicalDepth});
    p.header["native_logical_mips"] = logicalMips;
    p.header["native_layers"] = nativeLayers;
    p.header["effective_native_color_space_qualified"] = false;
    at = meta + metadataBytes;
    if (arrayNative) {
      auto names = J::array();
      for (std::uint32_t i = 0; i < nativeLayers; ++i) {
        auto len = Monstey_U16(b, at);
        at += 2;
        Monstey_CheckRange(b, at, len);
        names.as_array().push_back(
            J::object({{"raw_hex", Monstey_THex(b.subspan(at, len))},
                       {"display", Monstey_TText(b.subspan(at, len))}}));
        at += len;
      }
      p.header["native_slice_source_names"] = names;
    }
  } else if (magic == 0x20534444)
    p.header["container"] = "DDS";
  else {
    p.header["container"] = "UNKNOWN";
    p.reason = "Unqualified resource magic; raw source preserved";
    return p;
  }
  p.dds_offset = at;
  Monstey_CheckRange(b, at, 128);
  Monstey_TRequire(Monstey_U32(b, at) == 0x20534444,
                   "DDS absent at qualified native boundary");
  Monstey_TRequire(Monstey_U32(b, at + 4) == 124 &&
                       Monstey_U32(b, at + 76) == 32,
                   "Invalid DDS header size");
  const auto width = Monstey_U32(b, at + 16), height = Monstey_U32(b, at + 12),
             rawDepth = Monstey_U32(b, at + 24),
             rawMips = Monstey_U32(b, at + 28),
             fourcc = Monstey_U32(b, at + 84),
             pfFlags = Monstey_U32(b, at + 80),
             caps2 = Monstey_U32(b, at + 112);
  Monstey_TRequire(width && height, "DDS zero extent");
  const auto count = std::max(1U, rawMips);
  std::uint32_t depth = std::max(1U, rawDepth), arraySize = 1, faces = 1,
                dimension = 3, dxgi = 0;
  std::vector<std::uint32_t> faceIds{0};
  p.header["dds_fourcc_hex"] = Monstey_THex(b.subspan(at + 84, 4));
  p.header["dds_pf_flags"] = pfFlags;
  p.header["dds_rgb_bits"] = Monstey_U32(b, at + 88);
  p.header["dds_channel_masks"] =
      J::array({Monstey_U32(b, at + 92), Monstey_U32(b, at + 96),
                Monstey_U32(b, at + 100), Monstey_U32(b, at + 104)});
  p.payload_offset = at + 128;
  // DDS_PIXELFORMAT members are valid only under their corresponding dwFlags.
  // Other classes and combinations remain opaque rather than borrowing stale
  // fields.
  if (pfFlags != 0x4 && pfFlags != 0x40 && pfFlags != 0x41) {
    p.header["dds_pixel_format_class"] = "unqualified_flags";
    p.header["format"] = "UNKNOWN";
    p.reason = "Unsupported or conflicting DDS pixel-format flags; raw source "
               "preserved";
    return p;
  }
  const bool hasFourCC = pfFlags == 0x4;
  const bool hasAlpha = pfFlags == 0x41;
  p.header["dds_pixel_format_class"] = hasFourCC  ? "FOURCC"
                                       : hasAlpha ? "RGB_ALPHA"
                                                  : "RGB";
  bool dx10 = hasFourCC && fourcc == 0x30315844;
  if (dx10) {
    Monstey_CheckRange(b, at, 148);
    dxgi = Monstey_U32(b, at + 128);
    dimension = Monstey_U32(b, at + 132);
    const auto misc = Monstey_U32(b, at + 136);
    arraySize = Monstey_U32(b, at + 140);
    p.header["dds_dx10_misc"] = misc;
    p.header["dds_dx10_misc2"] = Monstey_U32(b, at + 144);
    p.header["dds_dimension_raw"] = dimension;
    p.header["dds_array_size_raw"] = arraySize;
    p.payload_offset = at + 148;
    Monstey_TRequire(arraySize > 0, "DDS array size is zero");
    if (arrayNative) {
      if (dimension != 5 || misc != 0) {
        p.reason = "Unqualified native array DDS dimension/misc profile";
        return p;
      }
      depth = 1;
    } else if (dimension == 4) {
      Monstey_TRequire(arraySize == 1 && !(misc & 4),
                       "Invalid DDS volume array/cube");
    } else if (dimension == 2) {
      Monstey_TRequire(height == 1, "DDS 1D resource height must be one");
      depth = 1;
    } else if (dimension == 3) {
      depth = 1;
      if (misc & 4) {
        faces = 6;
        faceIds = {0, 1, 2, 3, 4, 5};
      }
    } else {
      p.reason = "Unqualified DDS dimension";
      return p;
    }
  } else {
    p.payload_offset = at + 128;
    if (hasFourCC) {
      if (fourcc == 0x31545844)
        dxgi = 71;
      else if (fourcc == 0x33545844)
        dxgi = 74;
      else if (fourcc == 0x35545844)
        dxgi = 77;
      else if (fourcc == 0x31495441 || fourcc == 0x55344342)
        dxgi = 80;
      else if (fourcc == 0x53344342)
        dxgi = 81;
      else if (fourcc == 0x32495441 || fourcc == 0x55354342)
        dxgi = 83;
      else if (fourcc == 0x53354342)
        dxgi = 84;
    } else if (Monstey_U32(b, at + 88) == 32) {
      const auto r = Monstey_U32(b, at + 92), g = Monstey_U32(b, at + 96),
                 blue = Monstey_U32(b, at + 100),
                 alpha = Monstey_U32(b, at + 104);
      if (r == 0xff0000 && g == 0xff00 && blue == 0xff) {
        if (hasAlpha && alpha == 0xff000000)
          dxgi = 87;
        else if (!hasAlpha)
          dxgi = 88;
      } else if (hasAlpha && r == 0xff && g == 0xff00 && blue == 0xff0000 &&
                 alpha == 0xff000000)
        dxgi = 28;
    }
    if (caps2 & 0x200000)
      dimension = 4;
    else {
      depth = 1;
      if (caps2 & 0x200) {
        faceIds.clear();
        for (unsigned face = 0; face < 6; ++face)
          if (caps2 & (0x400U << face))
            faceIds.push_back(face);
        Monstey_TRequire(!faceIds.empty(), "DDS cube has no faces");
        faces = static_cast<std::uint32_t>(faceIds.size());
      }
    }
  }
  if (native && !arrayNative &&
      ((volumeNative && dimension != 4) ||
       (!volumeNative && (dimension != 3 || arraySize != 1 || faces != 1)))) {
    p.reason =
        "Native kind and DDS dimension profile disagree; raw source preserved";
    return p;
  }
  if (dimension == 4)
    Monstey_TRequire(rawDepth > 0, "DDS volume depth is zero");
  p.format = Monstey_TFormatOf(dxgi);
  p.header["dds_format_dxgi"] = dxgi;
  p.header["format"] = p.format.name;
  p.header["dds_fourcc_hex"] = Monstey_THex(b.subspan(at + 84, 4));
  p.header["dds_pf_flags"] = Monstey_U32(b, at + 80);
  p.header["dds_rgb_bits"] = Monstey_U32(b, at + 88);
  p.header["dds_channel_masks"] =
      J::array({Monstey_U32(b, at + 92), Monstey_U32(b, at + 96),
                Monstey_U32(b, at + 100), Monstey_U32(b, at + 104)});
  p.header["dds_caps2"] = caps2;
  p.header["dds_mips_raw"] = rawMips;
  p.header["dds_mips"] = count;
  p.header["dds_dimensions"] = J::array({width, height, depth});
  p.header["dds_declared_color_space"] =
      dx10 ? (p.format.srgb ? "sRGB" : "not_sRGB_format_tag")
           : "unspecified_in_legacy_DDS";
  p.header["decode_color_transform"] = "none";
  if (!p.format.block_bytes && !p.format.pixel_bytes) {
    p.reason = "Unqualified DDS pixel format; raw source preserved";
    return p;
  }
  Monstey_TRequire(count <= static_cast<std::uint32_t>(std::bit_width(
                                std::max({width, height, depth}))),
                   "DDS mip count exceeds extent chain");
  Monstey_TRequire(arraySize <= 65536 && faces <= 6,
                   "DDS subresource count exceeds bounded layout");
  if (native) {
    Monstey_TRequire(logicalWidth && logicalHeight && logicalDepth &&
                         logicalMips,
                     "Native logical extent/mips are zero");
    Monstey_TRequire(!arrayNative || arraySize == nativeLayers,
                     "Native and DDS array sizes differ");
    std::optional<unsigned> start;
    for (unsigned i = 0; i < logicalMips && i < 32; ++i)
      if (std::max(1U, logicalWidth >> i) == width &&
          std::max(1U, logicalHeight >> i) == height &&
          (!volumeNative || std::max(1U, logicalDepth >> i) == depth)) {
        start = i;
        break;
      }
    Monstey_TRequire(start && *start + count <= logicalMips,
                     "DDS mip span does not fit native logical chain");
    p.header["first_logical_mip"] = *start;
    p.header["native_and_dds_format_agree"] =
        p.header["native_metadata_format_raw"] == J(dxgi);
  }
  if (native && p.header["native_metadata_format_raw"] != J(dxgi)) {
    p.reason = "Native and DDS format tags disagree; raw source preserved";
    return p;
  }
  if (p.format.pixel_bytes && (Monstey_U32(b, at + 8) & 8U) &&
      Monstey_U32(b, at + 20) != Monstey_TMul(width, p.format.pixel_bytes)) {
    p.reason = "Unqualified padded DDS row pitch; raw source preserved";
    return p;
  }
  std::uint64_t cursor = p.payload_offset;
  for (std::uint32_t layer = 0; layer < arraySize; ++layer)
    for (std::uint32_t face = 0; face < faces; ++face)
      for (std::uint32_t mip = 0; mip < count; ++mip) {
        const auto w = std::max(1U, width >> mip),
                   h = std::max(1U, height >> mip),
                   z = dimension == 4 ? std::max(1U, depth >> mip) : 1U;
        const auto row =
            p.format.block_bytes
                ? Monstey_TMul((std::uint64_t(w) + 3) / 4, p.format.block_bytes)
                : Monstey_TMul(w, p.format.pixel_bytes);
        const auto slice = Monstey_TMul(row, p.format.block_bytes
                                                 ? (std::uint64_t(h) + 3) / 4
                                                 : h),
                   size = Monstey_TMul(slice, z);
        Monstey_TRequire(cursor <= b.size() && size <= b.size() - cursor,
                         "DDS subresource data is truncated");
        p.subresources.push_back(
            {layer, faceIds[face], mip, w, h, z, cursor, size, row, slice});
        cursor = Monstey_TAdd(cursor, size);
      }
  p.header["trailing_bytes"] = b.size() - cursor;
  p.layout = true;
  p.decode = true;
  return p;
}
} // namespace
struct Monstey_TextureResource::Monstey_Impl {
  B baseline, current;
  Monstey_TParsed parsed;
  std::filesystem::path source;
  std::string sourceHash;
  explicit Monstey_Impl(std::span<const std::uint8_t> b)
      : baseline(b.begin(), b.end()), current(baseline),
        parsed(Monstey_TParse(current)),
        sourceHash(Monstey_HashSha256(baseline)) {}
  void Check() const {
    if (!source.empty() && Monstey_HashSha256(source) != sourceHash)
      throw Monstey_Error("source_hash_mismatch",
                          "Texture source changed; reopen it");
  }
};
Monstey_TextureResource::Monstey_TextureResource(
    std::span<const std::uint8_t> b)
    : impl_(std::make_unique<Monstey_Impl>(b)) {}
Monstey_TextureResource::Monstey_TextureResource(
    const std::filesystem::path &path, std::string_view expected)
    : Monstey_TextureResource(Monstey_ReadFile(path)) {
  impl_->source = std::filesystem::absolute(path).lexically_normal();
  if (!expected.empty() &&
      impl_->sourceHash != Monstey_TExpected(std::string(expected)))
    throw Monstey_Error("source_hash_mismatch",
                        "Texture source SHA256 differs");
  impl_->Check();
}
Monstey_TextureResource::~Monstey_TextureResource() = default;
Monstey_TextureResource::Monstey_Impl &
Monstey_TextureResource::Monstey_State() {
  if (!impl_)
    throw Monstey_Error("texture_empty",
                        "Texture resource has been moved from");
  return *impl_;
}
const Monstey_TextureResource::Monstey_Impl &
Monstey_TextureResource::Monstey_State() const {
  if (!impl_)
    throw Monstey_Error("texture_empty",
                        "Texture resource has been moved from");
  return *impl_;
}
Monstey_TextureResource::Monstey_TextureResource(
    const Monstey_TextureResource &o)
    : impl_(std::make_unique<Monstey_Impl>(o.Monstey_State())) {}
Monstey_TextureResource &
Monstey_TextureResource::operator=(const Monstey_TextureResource &o) {
  if (this != &o) {
    auto next = std::make_unique<Monstey_Impl>(o.Monstey_State());
    impl_.swap(next);
  }
  return *this;
}
Monstey_TextureResource::Monstey_TextureResource(
    Monstey_TextureResource &&) noexcept = default;
Monstey_TextureResource &Monstey_TextureResource::operator=(
    Monstey_TextureResource &&) noexcept = default;
std::vector<Monstey_TextureSubresource>
Monstey_TextureResource::Subresources() const {
  const auto &p = Monstey_State();
  p.Check();
  return p.parsed.subresources;
}
B Monstey_TextureResource::EncodedSubresource(std::size_t index) const {
  const auto &p = Monstey_State();
  p.Check();
  const auto &r = p.parsed.subresources.at(index);
  auto begin = p.current.begin() + static_cast<std::ptrdiff_t>(r.offset);
  return B(begin, begin + static_cast<std::ptrdiff_t>(r.bytes));
}
B Monstey_TextureResource::Build() const {
  const auto &p = Monstey_State();
  p.Check();
  return p.current;
}
J Monstey_TextureResource::Describe() const {
  const auto &p = Monstey_State();
  p.Check();
  auto r = p.parsed.header;
  auto ranges = J::array();
  for (const auto &s : p.parsed.subresources)
    ranges.as_array().push_back(
        J::object({{"array_slice", s.array_slice},
                   {"cube_face", s.cube_face},
                   {"mip", s.mip},
                   {"dimensions", J::array({s.width, s.height, s.depth})},
                   {"offset", s.offset},
                   {"bytes", s.bytes},
                   {"row_pitch", s.row_pitch},
                   {"slice_pitch", s.slice_pitch}}));
  r["schema"] = "Monstey.TextureResource/1";
  r["source_sha256"] = p.sourceHash;
  r["effective_sha256"] = Monstey_HashSha256(p.current);
  r["source"] =
      p.source.empty() ? J(nullptr) : J(Monstey_Utf8(p.source.wstring()));
  r["modified"] = p.current != p.baseline;
  r["layout_qualified"] = p.parsed.layout;
  r["decode_supported"] = p.parsed.decode;
  r["qualification_note"] = p.parsed.reason;
  r["dds_offset"] = static_cast<std::uint64_t>(p.parsed.dds_offset);
  r["payload_offset"] = static_cast<std::uint64_t>(p.parsed.payload_offset);
  r["file_bytes"] = static_cast<std::uint64_t>(p.current.size());
  r["subresources"] = ranges;
  r["native_rendering_verified"] = false;
  r["product_runtime"] = "C++20";
  return r;
}
void Monstey_TextureResource::Replace(std::size_t index,
                                      std::span<const std::uint8_t> encoded) {
  Monstey_TextureEdit edit{index, B(encoded.begin(), encoded.end())};
  Edit(std::span(&edit, 1));
}
void Monstey_TextureResource::Edit(std::span<const Monstey_TextureEdit> edits) {
  const auto &p = Monstey_State();
  p.Check();
  Monstey_TRequire(p.parsed.layout, "Unqualified texture layout is read-only");
  Monstey_TRequire(!edits.empty() && edits.size() <= 65536,
                   "Texture edit batch must contain 1..65536 replacements");
  std::vector<Monstey_TextureEdit> owned(edits.begin(), edits.end());
  for (const auto &e : owned) {
    Monstey_TRequire(e.subresource < p.parsed.subresources.size(),
                     "Texture subresource index outside layout");
    Monstey_TRequire(e.bytes.size() ==
                         p.parsed.subresources[e.subresource].bytes,
                     "Texture replacement byte count differs from subresource");
  }
  auto next = std::make_unique<Monstey_Impl>(p);
  for (const auto &e : owned) {
    const auto &r = p.parsed.subresources[e.subresource];
    std::copy(e.bytes.begin(), e.bytes.end(),
              next->current.begin() + static_cast<std::ptrdiff_t>(r.offset));
  }
  next->parsed = Monstey_TParse(next->current);
  p.Check();
  impl_.swap(next);
}
J Monstey_TextureResource::Build(const std::filesystem::path &output) const {
  const auto &p = Monstey_State();
  p.Check();
  const std::vector<std::filesystem::path> sources =
      p.source.empty() ? std::vector<std::filesystem::path>{}
                       : std::vector<std::filesystem::path>{p.source};
  Monstey_RequireOfflineOutput(output, sources);
  Monstey_WriteNewFile(output, p.current);
  Monstey_TRequire(Monstey_ReadFile(output) == p.current,
                   "Texture output readback differs");
  p.Check();
  return J::object(
      {{"status", "MONSTEY_TEXTURE_BUILD_VALIDATED"},
       {"path", Monstey_Utf8(std::filesystem::absolute(output).wstring())},
       {"sha256", Monstey_HashSha256(p.current)},
       {"bytes", static_cast<std::uint64_t>(p.current.size())},
       {"no_op_byte_exact", p.current == p.baseline},
       {"game_written", false},
       {"runtime_verified", false}});
}
Monstey_TextureImage
Monstey_TextureResource::Decode(std::size_t index, std::uint32_t depthSlice,
                                std::uint64_t maximumPixels) const {
  const auto &p = Monstey_State();
  p.Check();
  Monstey_TRequire(p.parsed.decode,
                   "Texture encoding has no qualified RGBA decoder");
  const auto &s = p.parsed.subresources.at(index);
  Monstey_TRequire(depthSlice < s.depth,
                   "Texture depth slice outside subresource");
  const auto pixels = Monstey_TMul(s.width, s.height);
  Monstey_TRequire(pixels <= maximumPixels &&
                       pixels <= SIZE_MAX / sizeof(std::array<float, 4>),
                   "Texture decoded image exceeds explicit pixel budget");
  Monstey_TextureImage out{s.width, s.height, {}};
  out.rgba.resize(static_cast<std::size_t>(pixels));
  const auto base =
      Monstey_TAdd(s.offset, Monstey_TMul(depthSlice, s.slice_pitch));
  const auto &f = p.parsed.format;
  if (f.block_bytes) {
    using DecodeFunction =
        void (*)(DirectX::XMVECTOR *, const std::uint8_t *) noexcept;
    DecodeFunction decode = nullptr;
    switch (f.dxgi) {
    case 71:
    case 72:
      decode = DirectX::D3DXDecodeBC1;
      break;
    case 74:
    case 75:
      decode = DirectX::D3DXDecodeBC2;
      break;
    case 77:
    case 78:
      decode = DirectX::D3DXDecodeBC3;
      break;
    case 80:
      decode = DirectX::D3DXDecodeBC4U;
      break;
    case 81:
      decode = DirectX::D3DXDecodeBC4S;
      break;
    case 83:
      decode = DirectX::D3DXDecodeBC5U;
      break;
    case 84:
      decode = DirectX::D3DXDecodeBC5S;
      break;
    case 95:
      decode = DirectX::D3DXDecodeBC6HU;
      break;
    case 96:
      decode = DirectX::D3DXDecodeBC6HS;
      break;
    case 98:
    case 99:
      decode = DirectX::D3DXDecodeBC7;
      break;
    default:
      break;
    }
    Monstey_TRequire(decode != nullptr, "No qualified block decoder");
    for (std::uint64_t by = 0; by < (std::uint64_t(s.height) + 3) / 4; ++by)
      for (std::uint64_t bx = 0; bx < (std::uint64_t(s.width) + 3) / 4; ++bx) {
        alignas(16) std::array<std::uint8_t, 16> block{};
        DirectX::XMVECTOR values[16];
        const auto offset =
            base + by * s.row_pitch + std::uint64_t(bx) * f.block_bytes;
        std::copy_n(p.current.data() + static_cast<std::size_t>(offset),
                    f.block_bytes, block.data());
        decode(values, block.data());
        for (unsigned y = 0; y < 4; ++y)
          for (unsigned x = 0; x < 4; ++x)
            if (bx * 4U + x < s.width && by * 4U + y < s.height) {
              DirectX::XMFLOAT4 q;
              DirectX::XMStoreFloat4(&q, values[y * 4 + x]);
              out.rgba[(by * 4U + y) * std::size_t(s.width) + bx * 4U + x] = {
                  q.x, q.y, q.z, q.w};
            }
      }
  } else {
    for (std::uint32_t y = 0; y < s.height; ++y)
      for (std::uint32_t x = 0; x < s.width; ++x) {
        const auto offset = base + std::uint64_t(y) * s.row_pitch +
                            std::uint64_t(x) * f.pixel_bytes;
        const auto *b = p.current.data() + static_cast<std::size_t>(offset);
        std::array<float, 4> q{0, 0, 0, 1};
        switch (f.dxgi) {
        case 87:
        case 91:
          q = {b[2] / 255.0f, b[1] / 255.0f, b[0] / 255.0f, b[3] / 255.0f};
          break;
        case 88:
        case 93:
          q = {b[2] / 255.0f, b[1] / 255.0f, b[0] / 255.0f, 1};
          break;
        case 28:
        case 29:
          q = {b[0] / 255.0f, b[1] / 255.0f, b[2] / 255.0f, b[3] / 255.0f};
          break;
        case 61:
          q[0] = b[0] / 255.0f;
          break;
        case 49:
          q[0] = b[0] / 255.0f;
          q[1] = b[1] / 255.0f;
          break;
        case 41:
          q[0] = std::bit_cast<float>(
              Monstey_U32(p.current, static_cast<std::size_t>(offset)));
          break;
        default:
          Monstey_TRequire(false, "No qualified packed decoder");
        }
        out.rgba[std::size_t(y) * s.width + x] = q;
      }
  }
  p.Check();
  return out;
}
} // namespace Monstey