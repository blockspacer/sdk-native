#include <Tanker/Crypto/AeadIv.hpp>
#include <Tanker/Encryptor.hpp>
#include <Tanker/Encryptor/v2.hpp>
#include <Tanker/Encryptor/v3.hpp>
#include <Tanker/Encryptor/v5.hpp>
#include <Tanker/Errors/Errc.hpp>
#include <Tanker/Serialization/Varint.hpp>

#include <Helpers/Await.hpp>
#include <Helpers/Buffers.hpp>
#include <Helpers/Errors.hpp>

#include <doctest.h>

using namespace Tanker;
using namespace Tanker::Errors;

namespace
{
template <typename T>
struct TestContext;

template <>
struct TestContext<EncryptorV2>
{
  tc::cotask<EncryptionMetadata> encrypt(
      std::uint8_t* encryptedData,
      gsl::span<std::uint8_t const> clearData) const
  {
    return EncryptorV2::encrypt(encryptedData, clearData);
  }

  Crypto::SymmetricKey keyVector{std::vector<std::uint8_t>{
      0x76, 0xd,  0x8e, 0x80, 0x5c, 0xbc, 0xa8, 0xb6, 0xda, 0xea, 0xcf,
      0x66, 0x46, 0xca, 0xd7, 0xeb, 0x4f, 0x3a, 0xbc, 0x69, 0xac, 0x9b,
      0xce, 0x77, 0x35, 0x8e, 0xa8, 0x31, 0xd7, 0x2f, 0x14, 0xdd}};
  std::vector<std::uint8_t> encryptedTestVector{
      0x02, 0x32, 0x93, 0xa3, 0xf8, 0x6c, 0xa8, 0x82, 0x25, 0xbc, 0x17, 0x7e,
      0xb5, 0x65, 0x9b, 0xee, 0xd,  0xfd, 0xcf, 0xc6, 0x5c, 0x6d, 0xb4, 0x72,
      0xe0, 0x5b, 0x33, 0x27, 0x4c, 0x83, 0x84, 0xd1, 0xad, 0xda, 0x5f, 0x86,
      0x2,  0x46, 0x42, 0x91, 0x71, 0x30, 0x65, 0x2e, 0x72, 0x47, 0xe6, 0x48,
      0x20, 0xa1, 0x86, 0x91, 0x7f, 0x9c, 0xb5, 0x5e, 0x91, 0xb3, 0x65, 0x2d};
};

template <>
struct TestContext<EncryptorV3>
{
  tc::cotask<EncryptionMetadata> encrypt(
      std::uint8_t* encryptedData,
      gsl::span<std::uint8_t const> clearData) const
  {
    return EncryptorV3::encrypt(encryptedData, clearData);
  }

  Crypto::SymmetricKey keyVector{std::vector<std::uint8_t>{
      0x76, 0xd,  0x8e, 0x80, 0x5c, 0xbc, 0xa8, 0xb6, 0xda, 0xea, 0xcf,
      0x66, 0x46, 0xca, 0xd7, 0xeb, 0x4f, 0x3a, 0xbc, 0x69, 0xac, 0x9b,
      0xce, 0x77, 0x35, 0x8e, 0xa8, 0x31, 0xd7, 0x2f, 0x14, 0xdd}};
  std::vector<std::uint8_t> encryptedTestVector{
      0x03, 0x37, 0xb5, 0x3d, 0x55, 0x34, 0xb5, 0xc1, 0x3f, 0xe3, 0x72, 0x81,
      0x47, 0xf0, 0xca, 0xda, 0x29, 0x99, 0x6e, 0x4,  0xa8, 0x41, 0x81, 0xa0,
      0xe0, 0x5e, 0x8e, 0x3a, 0x8,  0xd3, 0x78, 0xfa, 0x5,  0x9f, 0x17, 0xfa};
};

template <>
struct TestContext<EncryptorV5>
{
  tc::cotask<EncryptionMetadata> encrypt(
      std::uint8_t* encryptedData,
      gsl::span<std::uint8_t const> clearData) const
  {
    return EncryptorV5::encrypt(
        encryptedData, clearData, resourceId, keyVector);
  }

  Crypto::SymmetricKey keyVector{std::vector<std::uint8_t>{
      0x76, 0xd,  0x8e, 0x80, 0x5c, 0xbc, 0xa8, 0xb6, 0xda, 0xea, 0xcf,
      0x66, 0x46, 0xca, 0xd7, 0xeb, 0x4f, 0x3a, 0xbc, 0x69, 0xac, 0x9b,
      0xce, 0x77, 0x35, 0x8e, 0xa8, 0x31, 0xd7, 0x2f, 0x14, 0xdd}};
  std::vector<std::uint8_t> encryptedTestVector{
      0x05, 0xc1, 0x74, 0x53, 0x1e, 0xdd, 0x77, 0x77, 0x87, 0x2c, 0x02,
      0x6e, 0xf2, 0x36, 0xdf, 0x28, 0x7e, 0x70, 0xea, 0xb6, 0xe7, 0x72,
      0x7d, 0xdd, 0x42, 0x5d, 0xa1, 0xab, 0xb3, 0x6e, 0xd1, 0x8b, 0xea,
      0xd7, 0xf5, 0xad, 0x23, 0xc0, 0xbd, 0x8c, 0x1f, 0x68, 0xc7, 0x9e,
      0xf2, 0xe9, 0xd8, 0x9e, 0xf9, 0x7e, 0x93, 0xc4, 0x29, 0x0d, 0x96,
      0x40, 0x2d, 0xbc, 0xf8, 0x0b, 0xb8, 0x4f, 0xfc, 0x48, 0x9b, 0x83,
      0xd1, 0x05, 0x51, 0x40, 0xfc, 0xc2, 0x7f, 0x6e, 0xd9, 0x16};
  Trustchain::ResourceId resourceId{std::vector<std::uint8_t>{
      0xc1,
      0x74,
      0x53,
      0x1e,
      0xdd,
      0x77,
      0x77,
      0x87,
      0x2c,
      0x02,
      0x6e,
      0xf2,
      0x36,
      0xdf,
      0x28,
      0x7e,
  }};
};
}

template <typename T>
void commonEncryptorTests(TestContext<T> ctx)
{
  SUBCASE("decryptedSize and encryptedSize should be symmetrical")
  {
    std::vector<uint8_t> a0(T::encryptedSize(0));
    Serialization::varint_write(a0.data(), T::version());
    std::vector<uint8_t> a42(T::encryptedSize(42));
    Serialization::varint_write(a42.data(), T::version());
    CHECK(T::decryptedSize(a0) == 0);
    CHECK(T::decryptedSize(a42) == 42);
  }

  SUBCASE("decryptedSize should throw if the buffer is truncated")
  {
    std::vector<std::uint8_t> const truncatedBuffer(1, T::version());
    TANKER_CHECK_THROWS_WITH_CODE(T::decryptedSize(truncatedBuffer),
                                  Errc::InvalidArgument);
  }

  SUBCASE("encrypt/decrypt should work with an empty buffer")
  {
    std::vector<uint8_t> clearData;
    std::vector<uint8_t> encryptedData(T::encryptedSize(clearData.size()));

    auto const metadata = AWAIT(ctx.encrypt(encryptedData.data(), clearData));

    std::vector<uint8_t> decryptedData(T::decryptedSize(encryptedData));

    AWAIT_VOID(T::decrypt(decryptedData.data(), metadata.key, encryptedData));

    CHECK(clearData == decryptedData);
  }

  SUBCASE("encrypt/decrypt should work with a normal buffer")
  {
    auto clearData = make_buffer("this is the data to encrypt");
    std::vector<uint8_t> encryptedData(T::encryptedSize(clearData.size()));

    auto const metadata = AWAIT(ctx.encrypt(encryptedData.data(), clearData));

    std::vector<uint8_t> decryptedData(T::decryptedSize(encryptedData));
    AWAIT_VOID(T::decrypt(decryptedData.data(), metadata.key, encryptedData));

    CHECK(clearData == decryptedData);
  }

  SUBCASE("decrypt should work with a test vector")
  {
    auto const clearData = make_buffer("this is very secret");

    std::vector<uint8_t> decryptedData(
        T::decryptedSize(ctx.encryptedTestVector));
    AWAIT_VOID(T::decrypt(
        decryptedData.data(), ctx.keyVector, ctx.encryptedTestVector));

    CHECK(decryptedData == clearData);
  }

  SUBCASE("encrypt should never give the same result twice")
  {
    auto clearData = make_buffer("this is the data to encrypt");
    std::vector<uint8_t> encryptedData1(T::encryptedSize(clearData.size()));
    AWAIT(ctx.encrypt(encryptedData1.data(), clearData));
    std::vector<uint8_t> encryptedData2(T::encryptedSize(clearData.size()));
    AWAIT(ctx.encrypt(encryptedData2.data(), clearData));

    CHECK(encryptedData1 != encryptedData2);
  }

  SUBCASE("decrypt should not work with a corrupted buffer")
  {
    auto const clearData = make_buffer("this is very secret");

    std::vector<uint8_t> decryptedData(
        T::decryptedSize(ctx.encryptedTestVector));

    ctx.encryptedTestVector[2]++;

    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT_VOID(T::decrypt(
            decryptedData.data(), ctx.keyVector, ctx.encryptedTestVector)),
        Errc::DecryptionFailed);
  }

  SUBCASE("extractResourceId should give the same result as encrypt")
  {
    auto clearData = make_buffer("this is the data to encrypt");
    std::vector<uint8_t> encryptedData(T::encryptedSize(clearData.size()));

    auto const metadata = AWAIT(ctx.encrypt(encryptedData.data(), clearData));

    CHECK(T::extractResourceId(encryptedData) == metadata.resourceId);
  }
}

TEST_CASE("EncryptorV2 tests")
{
  TestContext<EncryptorV2> ctx;

  commonEncryptorTests(ctx);

  SUBCASE("encryptedSize should return the right size")
  {
    auto const versionSize = Serialization::varint_size(EncryptorV2::version());
    constexpr auto MacSize = Trustchain::ResourceId::arraySize;
    constexpr auto IvSize = Crypto::AeadIv::arraySize;
    CHECK(EncryptorV2::encryptedSize(0) == versionSize + 0 + MacSize + IvSize);
    CHECK(EncryptorV2::encryptedSize(1) == versionSize + 1 + MacSize + IvSize);
  }
}

TEST_CASE("EncryptorV3 tests")
{
  TestContext<EncryptorV3> ctx;

  commonEncryptorTests(ctx);

  SUBCASE("encryptedSize should return the right size")
  {
    auto const versionSize = Serialization::varint_size(EncryptorV3::version());
    constexpr auto MacSize = Trustchain::ResourceId::arraySize;
    CHECK(EncryptorV3::encryptedSize(0) == versionSize + 0 + MacSize);
    CHECK(EncryptorV3::encryptedSize(1) == versionSize + 1 + MacSize);
  }
}

TEST_CASE("EncryptorV5 tests")
{
  TestContext<EncryptorV5> ctx;

  commonEncryptorTests(ctx);

  SUBCASE("encryptedSize should return the right size")
  {
    auto const versionSize = Serialization::varint_size(EncryptorV5::version());
    constexpr auto ResourceIdSize = Trustchain::ResourceId::arraySize;
    constexpr auto IvSize = Crypto::AeadIv::arraySize;
    constexpr auto MacSize = Trustchain::ResourceId::arraySize;
    CHECK(EncryptorV5::encryptedSize(0) ==
          versionSize + ResourceIdSize + IvSize + 0 + MacSize);
    CHECK(EncryptorV5::encryptedSize(1) ==
          versionSize + ResourceIdSize + IvSize + 1 + MacSize);
  }
}

TEST_CASE("extractResourceId should throw on a truncated buffer")
{
  auto encryptedData = make_buffer("");

  TANKER_CHECK_THROWS_WITH_CODE(Encryptor::extractResourceId(encryptedData),
                                Errc::InvalidArgument);
}
