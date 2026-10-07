#include "pch.h"
#include "ServerCertificate.h"

// WIN32_LEAN_AND_MEAN leaves these out of <windows.h>.
#include <wincrypt.h>
#include <ncrypt.h>

#include <charconv>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameLogicTests
{
namespace
{
Neuron::CertificateHash HashOf(std::string_view _text)
{
  return Neuron::HashCertificate({reinterpret_cast<const std::uint8_t*>(_text.data()), _text.size()});
}

// A hash written as 64 hex digits, as the published vectors are.
Neuron::CertificateHash HashFromHex(std::string_view _hex)
{
  Assert::AreEqual(size_t{64}, _hex.size());
  Neuron::CertificateHash hash{};
  for (size_t i = 0; i < hash.size(); ++i)
  {
    const auto [end, error] = std::from_chars(_hex.data() + (2 * i), _hex.data() + (2 * i) + 2, hash[i], 16);
    Assert::IsTrue(error == std::errc{} && end == _hex.data() + (2 * i) + 2);
  }
  return hash;
}

PCCERT_CONTEXT ContextOf(const Neuron::ServerCertificate& _certificate)
{
  return static_cast<PCCERT_CONTEXT>(_certificate.Context());
}

// The name of the key the certificate names, in the user's key store.
std::wstring KeyNameOf(PCCERT_CONTEXT _certificate)
{
  DWORD bytes = 0;
  Assert::IsTrue(CertGetCertificateContextProperty(_certificate, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &bytes) != FALSE);
  std::vector<BYTE> buffer(bytes);
  Assert::IsTrue(CertGetCertificateContextProperty(_certificate, CERT_KEY_PROV_INFO_PROP_ID, buffer.data(), &bytes) != FALSE);
  const auto* info = reinterpret_cast<const CRYPT_KEY_PROV_INFO*>(buffer.data());
  Assert::AreEqual(std::wstring(MS_KEY_STORAGE_PROVIDER), std::wstring(info->pwszProvName));
  return info->pwszContainerName;
}

bool KeyIsInTheStore(const std::wstring& _name)
{
  NCRYPT_PROV_HANDLE provider = 0;
  Assert::IsTrue(NCryptOpenStorageProvider(&provider, MS_KEY_STORAGE_PROVIDER, 0) == ERROR_SUCCESS);
  NCRYPT_KEY_HANDLE key = 0;
  const SECURITY_STATUS status = NCryptOpenKey(provider, &key, _name.c_str(), AT_KEYEXCHANGE, 0);
  if (key != 0)
    NCryptFreeObject(key);
  NCryptFreeObject(provider);
  return status == ERROR_SUCCESS;
}
} // namespace

// ADR-060: the in-process server's TLS identity, a self-signed certificate for localhost whose SHA-256 a client pins.
// The client and the server hash with one function, so a QUIC connection succeeds even if that function hashed the wrong
// thing; these tests hold it to SHA-256 and to what Windows itself says about the certificate.
TEST_CLASS(ServerCertificateTests)
{
public:
  // FIPS 180-2's vectors: the empty message, "abc", and a message of two blocks.
  TEST_METHOD(HashesWithSha256)
  {
    Assert::IsTrue(HashFromHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == HashOf(""));
    Assert::IsTrue(HashFromHex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == HashOf("abc"));
    Assert::IsTrue(HashFromHex("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") ==
                   HashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
  }

  // What the server offers to be pinned is the hash of its certificate's DER encoding, as Windows computes it.
  TEST_METHOD(HashIsTheCertificatesSha256Thumbprint)
  {
    const Neuron::ServerCertificate certificate;
    Neuron::CertificateHash thumbprint{};
    DWORD bytes = static_cast<DWORD>(thumbprint.size());
    Assert::IsTrue(CertGetCertificateContextProperty(ContextOf(certificate), CERT_SHA256_HASH_PROP_ID, thumbprint.data(), &bytes) != FALSE);
    Assert::AreEqual(static_cast<DWORD>(thumbprint.size()), bytes);
    Assert::IsTrue(thumbprint == certificate.Hash());
  }

  // A self-signed certificate for localhost, signed with SHA-256 by a 2048-bit RSA key, and valid now.
  TEST_METHOD(IsASelfSignedCertificateForLocalhost)
  {
    const Neuron::ServerCertificate certificate;
    const PCCERT_CONTEXT context = ContextOf(certificate);
    Assert::IsNotNull(context);

    std::array<wchar_t, 64> subject{};
    (void)CertGetNameStringW(context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subject.data(), static_cast<DWORD>(subject.size()));
    Assert::AreEqual(std::wstring(L"localhost"), std::wstring(subject.data()));
    Assert::IsTrue(CertCompareCertificateName(X509_ASN_ENCODING, &context->pCertInfo->Subject, &context->pCertInfo->Issuer) != FALSE);

    Assert::AreEqual(std::string(szOID_RSA_SHA256RSA), std::string(context->pCertInfo->SignatureAlgorithm.pszObjId));
    Assert::AreEqual(DWORD{2048}, CertGetPublicKeyLength(X509_ASN_ENCODING, &context->pCertInfo->SubjectPublicKeyInfo));
    Assert::AreEqual(LONG{0}, CertVerifyTimeValidity(nullptr, context->pCertInfo));
  }

  // Schannel finds the key by name in the user's key store, and two servers never share one.
  TEST_METHOD(EachHasAKeyOfItsOwn)
  {
    const Neuron::ServerCertificate first;
    const Neuron::ServerCertificate second;
    const std::wstring firstKey = KeyNameOf(ContextOf(first));
    const std::wstring secondKey = KeyNameOf(ContextOf(second));
    Assert::IsTrue(firstKey.starts_with(L"Neuron.QuicListener."), firstKey.c_str());
    Assert::IsTrue(secondKey.starts_with(L"Neuron.QuicListener."), secondKey.c_str());
    Assert::AreNotEqual(firstKey, secondKey);
    Assert::IsFalse(first.Hash() == second.Hash());
  }

  // The key is persisted only for as long as the certificate lives, so that no run leaves keys behind in the user's store.
  TEST_METHOD(DeletesItsKeyFromTheStoreWhenDestroyed)
  {
    std::wstring keyName;
    {
      const Neuron::ServerCertificate certificate;
      keyName = KeyNameOf(ContextOf(certificate));
      Assert::IsTrue(KeyIsInTheStore(keyName), keyName.c_str());
    }
    Assert::IsFalse(KeyIsInTheStore(keyName), keyName.c_str());
  }
};
} // namespace GameLogicTests
