#include "Monstey_TextureApi.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <wincodec.h>
namespace Monstey {
namespace {
using J = Monstey_Json;
template <class T> struct Monstey_ComOwner {
  T *p = nullptr;
  ~Monstey_ComOwner() {
    if (p)
      p->Release();
  }
  Monstey_ComOwner() = default;
  Monstey_ComOwner(const Monstey_ComOwner &) = delete;
  Monstey_ComOwner &operator=(const Monstey_ComOwner &) = delete;
  T **Put() {
    Monstey_Require(!p, "COM owner already initialized");
    return &p;
  }
  T *operator->() const { return p; }
};
void Monstey_WicCheck(HRESULT h, const char *message) {
  if (FAILED(h))
    throw Monstey_Error("texture_png",
                        std::string(message) + ": " +
                            Monstey_Hex(static_cast<std::uint32_t>(h)));
}
Monstey_Bytes Monstey_Png(const Monstey_TextureImage &image) {
  const auto pixels = std::uint64_t(image.width) * image.height;
  Monstey_Require(image.rgba.size() == pixels && pixels <= UINT32_MAX / 4 &&
                      image.width <= UINT32_MAX / 4,
                  "PNG image exceeds WIC buffer bounds");
  Monstey_Bytes bytes(static_cast<std::size_t>(pixels * 4));
  for (std::size_t i = 0; i < image.rgba.size(); ++i)
    for (unsigned c = 0; c < 4; ++c) {
      const auto v = image.rgba[i][c == 0 ? 2 : c == 2 ? 0 : c];
      bytes[i * 4 + c] =
          std::isnan(v) ? 0
                        : static_cast<std::uint8_t>(
                              std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    }
  Monstey_ComOwner<IWICImagingFactory> factory;
  Monstey_WicCheck(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(factory.Put())),
                   "Create WIC factory");
  Monstey_ComOwner<IStream> stream;
  Monstey_WicCheck(CreateStreamOnHGlobal(nullptr, TRUE, stream.Put()),
                   "Create PNG memory stream");
  Monstey_ComOwner<IWICBitmapEncoder> encoder;
  Monstey_WicCheck(
      factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.Put()),
      "Create PNG encoder");
  Monstey_WicCheck(encoder->Initialize(stream.p, WICBitmapEncoderNoCache),
                   "Initialize PNG encoder");
  Monstey_ComOwner<IWICBitmapFrameEncode> frame;
  Monstey_ComOwner<IPropertyBag2> properties;
  Monstey_WicCheck(encoder->CreateNewFrame(frame.Put(), properties.Put()),
                   "Create PNG frame");
  Monstey_WicCheck(frame->Initialize(properties.p), "Initialize PNG frame");
  Monstey_WicCheck(frame->SetSize(image.width, image.height), "Set PNG extent");
  auto format = GUID_WICPixelFormat32bppBGRA;
  Monstey_WicCheck(frame->SetPixelFormat(&format), "Set PNG pixel format");
  Monstey_Require(IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA),
                  "WIC did not retain BGRA pixel format");
  Monstey_WicCheck(frame->WritePixels(image.height, image.width * 4,
                                      static_cast<UINT>(bytes.size()),
                                      bytes.data()),
                   "Write PNG pixels");
  Monstey_WicCheck(frame->Commit(), "Commit PNG frame");
  Monstey_WicCheck(encoder->Commit(), "Commit PNG encoder");
  STATSTG info{};
  Monstey_WicCheck(stream->Stat(&info, STATFLAG_NONAME),
                   "Read PNG stream size");
  Monstey_Require(info.cbSize.QuadPart <= UINT32_MAX,
                  "PNG encoded result exceeds stream read limit");
  Monstey_Bytes encoded(static_cast<std::size_t>(info.cbSize.QuadPart));
  LARGE_INTEGER start{};
  Monstey_WicCheck(stream->Seek(start, STREAM_SEEK_SET, nullptr),
                   "Seek PNG stream");
  ULONG read = 0;
  Monstey_WicCheck(
      stream->Read(encoded.data(), static_cast<ULONG>(encoded.size()), &read),
      "Read PNG stream");
  Monstey_Require(read == encoded.size(), "PNG stream short read");

  // Native PNG decode/readback checks the quantized pixels, not just the file
  // bytes.
  Monstey_WicCheck(stream->Seek(start, STREAM_SEEK_SET, nullptr),
                   "Seek PNG verification stream");
  Monstey_ComOwner<IWICBitmapDecoder> decoder;
  Monstey_WicCheck(
      factory->CreateDecoderFromStream(
          stream.p, nullptr, WICDecodeMetadataCacheOnLoad, decoder.Put()),
      "Decode PNG verification");
  Monstey_ComOwner<IWICBitmapFrameDecode> decodedFrame;
  Monstey_WicCheck(decoder->GetFrame(0, decodedFrame.Put()),
                   "Read PNG verification frame");
  Monstey_ComOwner<IWICFormatConverter> converter;
  Monstey_WicCheck(factory->CreateFormatConverter(converter.Put()),
                   "Create PNG verification converter");
  Monstey_WicCheck(converter->Initialize(decodedFrame.p,
                                         GUID_WICPixelFormat32bppBGRA,
                                         WICBitmapDitherTypeNone, nullptr, 0,
                                         WICBitmapPaletteTypeCustom),
                   "Convert PNG verification pixels");
  Monstey_Bytes checked(bytes.size());
  Monstey_WicCheck(converter->CopyPixels(nullptr, image.width * 4,
                                         static_cast<UINT>(checked.size()),
                                         checked.data()),
                   "Read PNG verification pixels");
  Monstey_Require(checked == bytes,
                  "PNG decoded pixels differ from numeric preview");
  return encoded;
}

struct Monstey_TextureComScope {
  HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  Monstey_TextureComScope() {
    if (status != RPC_E_CHANGED_MODE)
      Monstey_WicCheck(status, "Initialize WIC apartment");
  }
  ~Monstey_TextureComScope() {
    if (SUCCEEDED(status))
      CoUninitialize();
  }
};
J Monstey_TextureSchema(std::string_view op) {
  const auto path =
      J::object({{"type", "string"}, {"format", "path"}, {"minLength", 1}});
  auto p = J::object(
      {{"source", path},
       {"expected_source_sha256",
        J::object({{"type", "string"}, {"pattern", "^[a-fA-F0-9]{64}$"}})}});
  auto required = J::array({"source"});
  if (op != "texture_inspect") {
    p["output"] = path;
    required.as_array().emplace_back("output");
  }
  if (op == "texture_extract" || op == "texture_preview_render")
    p["subresource"] =
        J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 65535}});
  if (op == "texture_preview_render") {
    p["depth_slice"] = J::object(
        {{"type", "integer"}, {"minimum", 0}, {"maximum", UINT32_MAX}});
    p["maximum_pixels"] =
        J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 16777216}});
  }
  return J::object({{"type", "object"},
                    {"properties", p},
                    {"required", required},
                    {"additionalProperties", false}});
}
std::filesystem::path Monstey_TexturePath(const J &a, const char *key) {
  Monstey_Require(a.contains(key) && a.at(key).is_string(),
                  std::string("Missing texture path ") + key);
  const auto &s = a.at(key).as_string();
  Monstey_Require(!s.empty() && s.find('\0') == s.npos,
                  std::string("Empty or NUL texture path ") + key);
  return std::filesystem::path(Monstey_Wide(s));
}
std::uint32_t Monstey_TextureIndex(const J &a, const char *key,
                                   std::uint32_t fallback,
                                   std::uint32_t maximum) {
  if (!a.contains(key))
    return fallback;
  Monstey_Require(a.at(key).is_integer(),
                  std::string("Expected texture integer ") + key);
  const auto n = a.at(key).as_uint();
  Monstey_Require(n <= maximum,
                  std::string("Texture index exceeds bound: ") + key);
  return static_cast<std::uint32_t>(n);
}
} // namespace
J Monstey_TextureCatalog() {
  auto result = J::array();
  const std::array<std::pair<const char *, const char *>, 4> ops{
      {{"texture_inspect",
        "Inspect native TEXR8/TEXA1, DDS and CPUT storage, complete "
        "subresource extents and qualified CPU decode availability."},
       {"texture_preview_render",
        "Render one decoded texture subresource/depth slice to a new PNG. Raw "
        "numeric RGBA clamped to [0,1], NaN=0, nearest 8-bit; no gamma "
        "transform or normal reconstruction."},
       {"texture_extract", "Extract one exact encoded texture subresource to a "
                           "new offline file without changing its source."},
       {"texture_roundtrip", "Write and read back a byte-exact texture copy, "
                             "including unknown fields and trailing bytes."}}};
  for (const auto &[name, description] : ops)
    result.as_array().push_back(
        J::object({{"name", name},
                   {"description", description},
                   {"read_only", std::string_view(name) == "texture_inspect"},
                   {"inputSchema", Monstey_TextureSchema(name)}}));
  return result;
}
J Monstey_TextureCall(std::string_view op, const J &a) {
  Monstey_Require(op == "texture_inspect" || op == "texture_preview_render" ||
                      op == "texture_extract" || op == "texture_roundtrip",
                  "Unknown native texture operation");
  Monstey_Require(a.is_object(), "Texture arguments must be an object");
  const auto schema = Monstey_TextureSchema(op);
  for (const auto &[key, value] : a.as_object()) {
    (void)value;
    Monstey_Require(schema.at("properties").contains(key),
                    "Unknown texture argument " + key);
  }
  const auto source = Monstey_TexturePath(a, "source");
  std::string expected;
  if (a.contains("expected_source_sha256")) {
    expected = a.at("expected_source_sha256").as_string();
    Monstey_Require(!expected.empty(),
                    "Explicit texture SHA256 must not be empty");
  }
  std::filesystem::path output;
  if (op != "texture_inspect") {
    output = Monstey_TexturePath(a, "output");
    const std::array inputs{source};
    Monstey_RequireOfflineOutput(output, inputs);
    Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(output)),
                    "Texture output already exists");
  }
  Monstey_TextureResource texture(source, expected);
  auto snapshot = texture.Describe();
  if (op == "texture_inspect")
    return snapshot;
  if (op == "texture_roundtrip")
    return texture.Build(output);
  const auto sub = Monstey_TextureIndex(a, "subresource", 0, 65535);
  Monstey_Bytes bytes;
  std::uint32_t width = 0, height = 0, depth = 0;
  if (op == "texture_extract")
    bytes = texture.EncodedSubresource(sub);
  else {
    depth = Monstey_TextureIndex(a, "depth_slice", 0, UINT32_MAX);
    const auto maximum =
        Monstey_TextureIndex(a, "maximum_pixels", 16777216, 16777216);
    Monstey_Require(maximum > 0, "Texture pixel budget must be positive");
    const auto image = texture.Decode(sub, depth, maximum);
    width = image.width;
    height = image.height;
    Monstey_TextureComScope apartment;
    bytes = Monstey_Png(image);
  }
  // A render/extract is from the pinned snapshot. Check the input immediately
  // before publication. No claim of an immutable external file after return.
  (void)texture.Describe();
  Monstey_WriteNewFile(output, bytes);
  Monstey_Require(Monstey_ReadFile(output) == bytes,
                  "Texture output readback differs");
  auto result =
      J::object({{"schema", "Monstey.TextureOutput/1"},
                 {"operation", std::string(op)},
                 {"source", Monstey_Utf8(source.wstring())},
                 {"source_sha256", snapshot.at("source_sha256")},
                 {"source_verified_before_publication", true},
                 {"subresource", sub},
                 {"output", Monstey_Utf8(output.wstring())},
                 {"output_sha256", Monstey_HashSha256(bytes)},
                 {"output_bytes", static_cast<std::uint64_t>(bytes.size())},
                 {"native_rendering_verified", false}});
  if (op == "texture_preview_render") {
    result["output_format"] = "PNG";
    result["width"] = width;
    result["height"] = height;
    result["depth_slice"] = depth;
    result["preview_policy"] =
        "Raw numeric RGBA clamped [0,1], NaN=0, nearest 8-bit; no gamma "
        "transform or normal reconstruction";
  } else
    result["output_format"] = "ENCODED_SUBRESOURCE";
  return result;
}
} // namespace Monstey
