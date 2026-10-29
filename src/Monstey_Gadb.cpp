#include "gadb/Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

Monstey_Gadb::Monstey_Gadb(const std::filesystem::path &source, const std::filesystem::path &schema)
    : impl_(std::make_unique<Monstey_Impl>(source, schema))
{
}

Monstey_Gadb::Monstey_Gadb(std::unique_ptr<Monstey_Impl> impl) : impl_(std::move(impl)) {}

std::unique_ptr<Monstey_Gadb> Monstey_Gadb::WithSchema(const std::filesystem::path &source, const J &qualifiedSchema)
{
    return std::unique_ptr<Monstey_Gadb>(
        new Monstey_Gadb(std::make_unique<Monstey_Impl>(source, std::filesystem::path{}, &qualifiedSchema)));
}

std::unique_ptr<Monstey_Gadb> Monstey_Gadb::New(const std::filesystem::path &schema)
{
    B bytes(46, 0);
    bytes[0] = 'G';
    bytes[1] = 'A';
    bytes[2] = 'D';
    bytes[3] = 'B';
    Monstey_PutU32(bytes, 4, 17);
    Monstey_PutU32(bytes, 40, 44);
    Monstey_AppendU32(bytes, 0);
    Monstey_AppendU32(bytes, 0);
    Monstey_AppendU32(bytes, 0);
    return std::unique_ptr<Monstey_Gadb>(
        new Monstey_Gadb(std::make_unique<Monstey_Impl>(std::filesystem::path{}, schema, nullptr, &bytes)));
}

std::unique_ptr<Monstey_Gadb> Monstey_Gadb::NewFx(std::span<const std::uint8_t> prefix,
                                                  const std::filesystem::path &schema)
{
    Monstey_Require(prefix.size() == 8 &&
                        std::string_view(reinterpret_cast<const char *>(prefix.data()), 4) == "LTFX" &&
                        Monstey_U32(prefix, 4) == 8,
                    "A qualified native 8-byte LTFX8 prefix is required");
    auto empty = New(schema);
    B bytes(prefix.begin(), prefix.end());
    bytes.insert(bytes.end(), empty->impl_->data.begin(), empty->impl_->data.end());
    return std::unique_ptr<Monstey_Gadb>(
        new Monstey_Gadb(std::make_unique<Monstey_Impl>(std::filesystem::path{}, schema, nullptr, &bytes)));
}

std::unique_ptr<Monstey_Gadb> Monstey_Gadb::NewFx(std::span<const std::uint8_t> prefix, const J &qualifiedSchema)
{
    Monstey_Require(prefix.size() == 8 &&
                        std::string_view(reinterpret_cast<const char *>(prefix.data()), 4) == "LTFX" &&
                        Monstey_U32(prefix, 4) == 8,
                    "A qualified native 8-byte LTFX8 prefix is required");
    auto empty = New();
    B bytes(prefix.begin(), prefix.end());
    bytes.insert(bytes.end(), empty->impl_->data.begin(), empty->impl_->data.end());
    return std::unique_ptr<Monstey_Gadb>(new Monstey_Gadb(
        std::make_unique<Monstey_Impl>(std::filesystem::path{}, std::filesystem::path{}, &qualifiedSchema, &bytes)));
}

Monstey_Gadb::~Monstey_Gadb() = default;

Monstey_Gadb::Monstey_Gadb(Monstey_Gadb &&) noexcept = default;

Monstey_Gadb &Monstey_Gadb::operator=(Monstey_Gadb &&) noexcept = default;

std::uint32_t Monstey_Gadb::Resolve(std::string_view selector) const
{
    return impl_->Select(selector);
}

J Monstey_Gadb::Schema() const
{
    impl_->Unchanged();
    return impl_->schema;
}

} // namespace Monstey
