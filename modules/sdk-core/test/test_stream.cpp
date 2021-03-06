#include <Tanker/Crypto/Crypto.hpp>
#include <Tanker/Encryptor/v4.hpp>
#include <Tanker/Streams/DecryptionStream.hpp>
#include <Tanker/Streams/EncryptionStream.hpp>
#include <Tanker/Streams/Helpers.hpp>
#include <Tanker/Streams/PeekableInputSource.hpp>
#include <Tanker/Trustchain/ResourceId.hpp>

#include <Helpers/Await.hpp>
#include <Helpers/Buffers.hpp>
#include <Helpers/Errors.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace Tanker;
using namespace Tanker::Errors;
using namespace Tanker::Streams;

namespace
{
Crypto::SymmetricKey const key(std::vector<std::uint8_t>{
    0xa,  0x7,  0x3d, 0xd0, 0x2c, 0x2d, 0x17, 0xf9, 0x49, 0xd9, 0x35,
    0x8e, 0xf7, 0xfe, 0x7b, 0xd1, 0xf6, 0xb,  0xf1, 0x5c, 0xa4, 0x32,
    0x1e, 0xe4, 0xaa, 0x18, 0xe1, 0x97, 0xbf, 0xf4, 0x5e, 0xfe});

tc::cotask<std::int64_t> failRead(std::uint8_t*, std::int64_t)
{
  throw Exception(make_error_code(Errc::IOError), "failRead");
}

tc::cotask<std::vector<std::uint8_t>> decryptData(DecryptionStream& decryptor)
{
  std::vector<std::uint8_t> decrypted(
      24 + 5 * Streams::Header::defaultEncryptedChunkSize);
  auto it = decrypted.data();
  std::int64_t totalRead{};
  while (auto const nbRead = TC_AWAIT(decryptor(it, 1024 * 1024 / 2)))
  {
    totalRead += nbRead;
    it += nbRead;
  }
  decrypted.resize(totalRead);
  TC_RETURN(std::move(decrypted));
}

auto fillAndMakePeekableSource(std::vector<uint8_t>& buffer)
{
  Crypto::randomFill(buffer);
  auto source = bufferViewToInputSource(buffer);
  return PeekableInputSource(source);
}
}

TEST_SUITE_BEGIN("PeekableInputSource");

TEST_CASE("reads an underlying stream")
{
  std::vector<uint8_t> buffer(50);
  auto peekable = fillAndMakePeekableSource(buffer);

  auto out = AWAIT(readAllStream(peekable));
  CHECK(out == buffer);
}

TEST_CASE("peeks and reads an underlying stream")
{
  std::vector<uint8_t> buffer(50);
  auto peekable = fillAndMakePeekableSource(buffer);

  auto peek = AWAIT(peekable.peek(30));
  CHECK(peek == gsl::make_span(buffer).subspan(0, 30));

  auto out = AWAIT(readAllStream(peekable));
  CHECK(out == buffer);
}

TEST_CASE("peeks past the end and reads an underlying stream")
{
  std::vector<uint8_t> buffer(50);
  auto peekable = fillAndMakePeekableSource(buffer);

  auto peek = AWAIT(peekable.peek(70));
  CHECK(peek == gsl::make_span(buffer).subspan(0, 50));

  auto out = AWAIT(readAllStream(peekable));
  CHECK(out == buffer);
}

TEST_CASE("alternate between peeks and read on a long underlying stream")
{
  std::vector<uint8_t> buffer(5 * 1024 * 1024);
  auto peekable = fillAndMakePeekableSource(buffer);

  auto peek = AWAIT(peekable.peek(30));
  CHECK(peek == gsl::make_span(buffer).subspan(0, 30));

  peek = AWAIT(peekable.peek(1200));
  CHECK(peek == gsl::make_span(buffer).subspan(0, 1200));

  std::vector<uint8_t> begin(1000);
  AWAIT(readStream(begin, peekable));
  CHECK(gsl::make_span(begin) == gsl::make_span(buffer).subspan(0, 1000));

  peek = AWAIT(peekable.peek(10));
  CHECK(peek == gsl::make_span(buffer).subspan(1000, 10));

  auto out = AWAIT(readAllStream(peekable));
  CHECK(gsl::make_span(out) == gsl::make_span(buffer).subspan(1000));
}

TEST_SUITE_END();

TEST_SUITE("Stream encryption")
{
  auto const mockKeyFinder =
      [](Trustchain::ResourceId const& id) -> tc::cotask<Crypto::SymmetricKey> {
    TC_RETURN(key);
  };

  TEST_CASE("Throws when underlying read fails")
  {
    EncryptionStream encryptor(failRead);

    auto const mockKeyFinder = [](auto) -> tc::cotask<Crypto::SymmetricKey> {
      TC_RETURN(Crypto::SymmetricKey());
    };

    TANKER_CHECK_THROWS_WITH_CODE(AWAIT(encryptor(nullptr, 0)), Errc::IOError);
    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT(DecryptionStream::create(failRead, mockKeyFinder)),
        Errc::IOError);
  }

  TEST_CASE("Encrypt/decrypt huge buffer")
  {
    std::vector<std::uint8_t> buffer(
        24 + 5 * Streams::Header::defaultEncryptedChunkSize);
    Crypto::randomFill(buffer);

    EncryptionStream encryptor(bufferViewToInputSource(buffer));
    auto const keyFinder =
        [&, key = encryptor.symmetricKey()](Trustchain::ResourceId const& id)
        -> tc::cotask<Crypto::SymmetricKey> {
      CHECK(id == encryptor.resourceId());
      TC_RETURN(key);
    };

    auto decryptor = AWAIT(DecryptionStream::create(encryptor, keyFinder));

    auto const decrypted = AWAIT(decryptData(decryptor));

    CHECK(decrypted.size() == buffer.size());
    CHECK(decrypted == buffer);
  }

  TEST_CASE(
      "Performs an underlying read when reading 0 when no buffered output is "
      "left")
  {
    std::vector<std::uint8_t> buffer(
        2 * Streams::Header::defaultEncryptedChunkSize);
    Crypto::randomFill(buffer);
    auto readCallback = bufferViewToInputSource(buffer);
    auto timesCallbackCalled = 0;

    EncryptionStream encryptor(
        [&timesCallbackCalled, cb = std::move(readCallback)](
            std::uint8_t* out,
            std::int64_t n) mutable -> tc::cotask<std::int64_t> {
          ++timesCallbackCalled;
          TC_RETURN(TC_AWAIT(cb(out, n)));
        });

    std::vector<std::uint8_t> encryptedBuffer(
        Streams::Header::defaultEncryptedChunkSize);

    AWAIT(encryptor(encryptedBuffer.data(),
                    Streams::Header::defaultEncryptedChunkSize));
    CHECK(timesCallbackCalled == 1);
    AWAIT(encryptor(encryptedBuffer.data(), 0));
    CHECK(timesCallbackCalled == 2);
    // returns immediately
    AWAIT(encryptor(encryptedBuffer.data(), 0));
    CHECK(timesCallbackCalled == 2);
    AWAIT(encryptor(encryptedBuffer.data(),
                    Streams::Header::defaultEncryptedChunkSize));
    CHECK(timesCallbackCalled == 2);
  }

  TEST_CASE("Decrypt test vector")
  {
    auto clearData = make_buffer("this is a secret");

    auto encryptedTestVector = std::vector<uint8_t>(
        {0x4,  0x46, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b,
         0x27, 0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0,
         0x3a, 0xc8, 0x74, 0xe1, 0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b,
         0xf5, 0xed, 0x22, 0xe6, 0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,
         0x22, 0x67, 0x3d, 0xdf, 0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30,
         0x92, 0x70, 0x23, 0x49, 0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a, 0x4,  0x46,
         0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32,
         0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3, 0xd3, 0x23,
         0x90, 0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88, 0xfc, 0x21,
         0x1e, 0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6, 0xbe, 0x54,
         0xd4, 0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6, 0x44, 0x47,
         0x30, 0x40, 0x2f, 0xe8, 0xf4, 0x50});

    auto decryptor = AWAIT(DecryptionStream::create(
        bufferViewToInputSource(encryptedTestVector), mockKeyFinder));

    auto const decrypted = AWAIT(decryptData(decryptor));

    CHECK(decrypted == clearData);
  }

  TEST_CASE("Truncated header")
  {
    auto const truncated = std::vector<uint8_t>(
        {0x4,  0x46, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xbe, 0x2b, 0x27,
         0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0, 0x3a,
         0xc8, 0x74, 0xe1, 0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b, 0xf5,
         0xed, 0x22, 0xe6, 0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,  0x22,
         0x67, 0x3d, 0xdf, 0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30, 0x92,
         0x70, 0x23, 0x49, 0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a, 0x4,  0x46, 0x0,
         0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32, 0xc9,
         0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3, 0xd3, 0x23, 0x90,
         0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88, 0xfc, 0x21, 0x1e,
         0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6, 0xbe, 0x54, 0xd4,
         0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6, 0x44, 0x47, 0x30,
         0x40, 0x2f, 0xe8, 0xf4, 0x50});
    auto inputSource = bufferViewToInputSource(truncated);
    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT(DecryptionStream::create(inputSource, mockKeyFinder)),
        Errors::Errc::DecryptionFailed);
  }

  TEST_CASE("Different headers between chunks")
  {
    auto clearData = make_buffer("this is a secret");

    // encryptedChunkSize is different
    auto invalidHeaders = std::vector<uint8_t>(
        {0x4,  0x46, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b,
         0x27, 0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0,
         0x3a, 0xc8, 0x74, 0xe1, 0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b,
         0xf5, 0xed, 0x22, 0xe6, 0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,
         0x22, 0x67, 0x3d, 0xdf, 0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30,
         0x92, 0x70, 0x23, 0x49, 0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a, 0x4,  0x45,
         0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32,
         0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3, 0xd3, 0x23,
         0x90, 0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88, 0xfc, 0x21,
         0x1e, 0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6, 0xbe, 0x54,
         0xd4, 0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6, 0x44, 0x47,
         0x30, 0x40, 0x2f, 0xe8, 0xf4, 0x50});

    auto decryptor = AWAIT(DecryptionStream::create(
        bufferViewToInputSource(invalidHeaders), mockKeyFinder));

    TANKER_CHECK_THROWS_WITH_CODE(AWAIT(decryptData(decryptor)),
                                  Errors::Errc::DecryptionFailed);
  }

  TEST_CASE("Wrong chunk order")
  {
    auto reversedTestVector = std::vector<uint8_t>({
        0x4,  0x46, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b,
        0x27, 0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3,
        0xd3, 0x23, 0x90, 0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88,
        0xfc, 0x21, 0x1e, 0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6,
        0xbe, 0x54, 0xd4, 0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6,
        0x44, 0x47, 0x30, 0x40, 0x2f, 0xe8, 0xf4, 0x50, 0x4,  0x46, 0x0,  0x0,
        0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32, 0xc9, 0xa,
        0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0, 0x3a, 0xc8, 0x74, 0xe1,
        0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b, 0xf5, 0xed, 0x22, 0xe6,
        0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,  0x22, 0x67, 0x3d, 0xdf,
        0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30, 0x92, 0x70, 0x23, 0x49,
        0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a,
    });

    auto inputSource = bufferViewToInputSource(reversedTestVector);
    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT(DecryptionStream::create(inputSource, mockKeyFinder)),
        Errors::Errc::DecryptionFailed);
  }

  TEST_CASE("Invalid encryptedChunkSize")
  {
    // encryptedChunkSize == 2, less than the strict minimum
    auto invalidSizeTestVector = std::vector<uint8_t>(
        {0x4,  0x02, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b,
         0x27, 0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0,
         0x3a, 0xc8, 0x74, 0xe1, 0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b,
         0xf5, 0xed, 0x22, 0xe6, 0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,
         0x22, 0x67, 0x3d, 0xdf, 0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30,
         0x92, 0x70, 0x23, 0x49, 0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a, 0x4,  0x02,
         0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32,
         0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3, 0xd3, 0x23,
         0x90, 0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88, 0xfc, 0x21,
         0x1e, 0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6, 0xbe, 0x54,
         0xd4, 0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6, 0x44, 0x47,
         0x30, 0x40, 0x2f, 0xe8, 0xf4, 0x50});

    // encryptedChunkSize == 69, but the chunk is of size 70
    auto smallSizeTestVector = std::vector<uint8_t>(
        {0x4,  0x45, 0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b,
         0x27, 0x32, 0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0xcd, 0x7,  0xd0,
         0x3a, 0xc8, 0x74, 0xe1, 0x8,  0x7e, 0x5e, 0xaa, 0xa2, 0x82, 0xd8, 0x8b,
         0xf5, 0xed, 0x22, 0xe6, 0x30, 0xbb, 0xaa, 0x9d, 0x71, 0xe3, 0x9a, 0x4,
         0x22, 0x67, 0x3d, 0xdf, 0xcf, 0x28, 0x48, 0xe2, 0xeb, 0x4b, 0xb4, 0x30,
         0x92, 0x70, 0x23, 0x49, 0x1c, 0xc9, 0x31, 0xcb, 0xda, 0x1a, 0x4,  0x45,
         0x0,  0x0,  0x0,  0x40, 0xec, 0x8d, 0x84, 0xad, 0xbe, 0x2b, 0x27, 0x32,
         0xc9, 0xa,  0x1e, 0xc6, 0x8f, 0x2b, 0xdb, 0x3f, 0x34, 0xf3, 0xd3, 0x23,
         0x90, 0xfc, 0x6,  0x35, 0xda, 0x99, 0x1e, 0x81, 0xdf, 0x88, 0xfc, 0x21,
         0x1e, 0xed, 0x3a, 0x28, 0x2d, 0x51, 0x82, 0x77, 0x7c, 0xf6, 0xbe, 0x54,
         0xd4, 0x92, 0xcd, 0x86, 0xd4, 0x88, 0x55, 0x20, 0x1f, 0xd6, 0x44, 0x47,
         0x30, 0x40, 0x2f, 0xe8, 0xf4, 0x50});

    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT(DecryptionStream::create(
            bufferViewToInputSource(invalidSizeTestVector), mockKeyFinder)),
        Errors::Errc::DecryptionFailed);
    TANKER_CHECK_THROWS_WITH_CODE(
        AWAIT(DecryptionStream::create(
            bufferViewToInputSource(smallSizeTestVector), mockKeyFinder)),
        Errors::Errc::DecryptionFailed);
  }
}
