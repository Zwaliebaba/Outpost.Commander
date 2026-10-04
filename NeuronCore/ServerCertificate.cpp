#include "pch.h"
#include "QuicChannel.h"
#include "ServerCertificate.h"

// WIN32_LEAN_AND_MEAN leaves these out of <windows.h>.
#include <wincrypt.h>
#include <bcrypt.h>
#include <ncrypt.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ncrypt.lib")

namespace
{
// The key's name in the user's key store starts with this, and ends with a GUID, so that two servers never share one.
constexpr std::wstring_view KEY_NAME_PREFIX = L"Neuron.QuicListener.";
constexpr DWORD KEY_BITS = 2048;
} // namespace

struct Neuron::ServerCertificate::State
{
  NCRYPT_PROV_HANDLE provider = 0;
  NCRYPT_KEY_HANDLE key = 0;
  PCCERT_CONTEXT certificate = nullptr;
  CertificateHash hash{};

  ~State()
  {
    if (certificate != nullptr)
      CertFreeCertificateContext(certificate);
    // Deleting the key takes it out of the user's key store, and frees its handle once it has.
    if (key != 0 && NCryptDeleteKey(key, 0) != ERROR_SUCCESS)
      NCryptFreeObject(key);
    if (provider != 0)
      NCryptFreeObject(provider);
  }
};

Neuron::ServerCertificate::ServerCertificate()
  : m_state(std::make_unique<State>())
{
  State& state = *m_state;
  winrt::check_hresult(NCryptOpenStorageProvider(&state.provider, MS_KEY_STORAGE_PROVIDER, 0));

  GUID guid{};
  winrt::check_hresult(CoCreateGuid(&guid));
  std::array<wchar_t, 40> guidText{};
  if (StringFromGUID2(guid, guidText.data(), static_cast<int>(guidText.size())) == 0)
    throw Exception("ServerCertificate: a GUID does not fit its buffer.");
  std::wstring keyName = std::wstring(KEY_NAME_PREFIX) + guidText.data();

  // Schannel signs with the key in its own process, so it has to find the key by name in a key store: a key that lives
  // only in this process would not do (ADR-060).
  winrt::check_hresult(NCryptCreatePersistedKey(state.provider, &state.key, NCRYPT_RSA_ALGORITHM, keyName.c_str(), AT_KEYEXCHANGE, 0));
  DWORD keyBits = KEY_BITS;
  winrt::check_hresult(NCryptSetProperty(state.key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&keyBits), sizeof(keyBits), 0));
  winrt::check_hresult(NCryptFinalizeKey(state.key, 0));

  DWORD subjectBytes = 0;
  winrt::check_bool(CertStrToNameW(X509_ASN_ENCODING, L"CN=localhost", CERT_X500_NAME_STR, nullptr, nullptr, &subjectBytes, nullptr));
  std::vector<BYTE> subjectName(subjectBytes);
  winrt::check_bool(
    CertStrToNameW(X509_ASN_ENCODING, L"CN=localhost", CERT_X500_NAME_STR, nullptr, subjectName.data(), &subjectBytes, nullptr));
  CERT_NAME_BLOB subject{.cbData = subjectBytes, .pbData = subjectName.data()};

  // The certificate names the key by its store and its name, which is how Schannel finds it.
  std::wstring providerName = MS_KEY_STORAGE_PROVIDER;
  CRYPT_KEY_PROV_INFO keyInfo{};
  keyInfo.pwszContainerName = keyName.data();
  keyInfo.pwszProvName = providerName.data();
  keyInfo.dwKeySpec = AT_KEYEXCHANGE;
  std::string signatureAlgorithm = szOID_RSA_SHA256RSA;
  CRYPT_ALGORITHM_IDENTIFIER signature{};
  signature.pszObjId = signatureAlgorithm.data();
  // Valid from now for a year, the default, which outlasts any server.
  state.certificate =
    winrt::check_pointer(CertCreateSelfSignedCertificate(state.key, &subject, 0, &keyInfo, &signature, nullptr, nullptr, nullptr));

  state.hash = HashCertificate({state.certificate->pbCertEncoded, state.certificate->cbCertEncoded});
}

Neuron::ServerCertificate::~ServerCertificate() = default;

void* Neuron::ServerCertificate::Context() const noexcept
{
  // MsQuic takes the certificate as a pointer to non-const, and only reads it.
  return const_cast<CERT_CONTEXT*>(m_state->certificate);
}

const Neuron::CertificateHash& Neuron::ServerCertificate::Hash() const noexcept
{
  return m_state->hash;
}

Neuron::CertificateHash Neuron::HashCertificate(std::span<const std::uint8_t> _der)
{
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  winrt::check_nt(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
  // BCryptHash takes its input as a pointer to non-const, so it hashes a copy.
  std::vector<UCHAR> input(_der.begin(), _der.end());
  CertificateHash hash{};
  const NTSTATUS status =
    BCryptHash(algorithm, nullptr, 0, input.data(), static_cast<ULONG>(input.size()), hash.data(), static_cast<ULONG>(hash.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  winrt::check_nt(status);
  return hash;
}
