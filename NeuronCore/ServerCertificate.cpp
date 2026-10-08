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

#include <cstring>
#include <fstream>

namespace
{
// The key's name in the user's key store starts with this, and ends with a GUID, so that two servers never share one.
constexpr std::wstring_view KEY_NAME_PREFIX = L"Neuron.QuicListener.";
constexpr DWORD KEY_BITS = 2048;
// A kept certificate's files in its folder: the certificate's DER encoding, and its key's name in the store as UTF-16
// (ADR-078).
constexpr std::wstring_view CERTIFICATE_FILE = L"Certificate.der";
constexpr std::wstring_view KEY_NAME_FILE = L"Certificate.key";
// How long a kept certificate is valid for: longer than any world. One made for a server alone is valid for a year, the
// default, which outlasts any match.
constexpr int KEPT_YEARS = 5;

std::vector<BYTE> ReadBytes(const std::filesystem::path& _path)
{
  std::ifstream file(_path, std::ios::binary);
  if (!file)
    return {};
  const std::vector<char> characters{std::istreambuf_iterator<char>(file), {}};
  return {characters.begin(), characters.end()};
}

void WriteBytes(const std::filesystem::path& _path, std::span<const std::byte> _bytes)
{
  std::ofstream file(_path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char*>(_bytes.data()), static_cast<std::streamsize>(_bytes.size()));
  file.close();
  if (!file)
    throw Neuron::Exception(std::format("ServerCertificate: {} cannot be written.", _path.string()));
}

// What a certificate tells Schannel of its key: its store and its name.
struct KeyInfo
{
  std::wstring name;
  std::wstring provider = MS_KEY_STORAGE_PROVIDER;
  CRYPT_KEY_PROV_INFO info{};

  explicit KeyInfo(std::wstring _name)
    : name(std::move(_name))
  {
    info.pwszContainerName = name.data();
    info.pwszProvName = provider.data();
    info.dwKeySpec = AT_KEYEXCHANGE;
  }

  KeyInfo(const KeyInfo&) = delete;
  KeyInfo& operator=(const KeyInfo&) = delete;
};
} // namespace

struct Neuron::ServerCertificate::State
{
  NCRYPT_PROV_HANDLE provider = 0;
  NCRYPT_KEY_HANDLE key = 0;
  PCCERT_CONTEXT certificate = nullptr;
  CertificateHash hash{};
  // A kept certificate's key stays in the store when this goes (ADR-078).
  bool kept = false;

  ~State()
  {
    if (certificate != nullptr)
      CertFreeCertificateContext(certificate);
    // Deleting the key takes it out of the user's key store, and frees its handle once it has.
    if (key != 0 && (kept || NCryptDeleteKey(key, 0) != ERROR_SUCCESS))
      NCryptFreeObject(key);
    if (provider != 0)
      NCryptFreeObject(provider);
  }

  // The certificate _folder keeps, with its key in the store, when both are there and it is valid now; false otherwise,
  // having changed nothing.
  [[nodiscard]] bool OpenKept(const std::filesystem::path& _folder)
  {
    const std::vector<BYTE> der = ReadBytes(_folder / CERTIFICATE_FILE);
    const std::vector<BYTE> nameBytes = ReadBytes(_folder / KEY_NAME_FILE);
    if (der.empty() || nameBytes.empty() || nameBytes.size() % sizeof(wchar_t) != 0)
      return false;
    std::wstring name(nameBytes.size() / sizeof(wchar_t), L'\0');
    std::memcpy(name.data(), nameBytes.data(), nameBytes.size());
    NCRYPT_KEY_HANDLE keptKey = 0;
    if (NCryptOpenKey(provider, &keptKey, name.c_str(), AT_KEYEXCHANGE, 0) != ERROR_SUCCESS)
      return false;
    PCCERT_CONTEXT context = CertCreateCertificateContext(X509_ASN_ENCODING, der.data(), static_cast<DWORD>(der.size()));
    KeyInfo keyInfo(name);
    // The certificate names its key by store and name, as CertCreateSelfSignCertificate's does, so that Schannel finds it.
    if (context == nullptr || CertVerifyTimeValidity(nullptr, context->pCertInfo) != 0 ||
        CertSetCertificateContextProperty(context, CERT_KEY_PROV_INFO_PROP_ID, 0, &keyInfo.info) == FALSE)
    {
      if (context != nullptr)
        CertFreeCertificateContext(context);
      // The key of a certificate that is no longer valid is of no more use, and a new one takes its place.
      if (NCryptDeleteKey(keptKey, 0) != ERROR_SUCCESS)
        NCryptFreeObject(keptKey);
      return false;
    }
    key = keptKey;
    certificate = context;
    kept = true;
    return true;
  }

  // A new key in the store and a certificate for it, valid until _end, or for a year when it has none. Returns the key's
  // name in the store.
  std::wstring Make(const std::optional<SYSTEMTIME>& _end)
  {
    GUID guid{};
    winrt::check_hresult(CoCreateGuid(&guid));
    std::array<wchar_t, 40> guidText{};
    if (StringFromGUID2(guid, guidText.data(), static_cast<int>(guidText.size())) == 0)
      throw Exception("ServerCertificate: a GUID does not fit its buffer.");
    KeyInfo keyInfo(std::wstring(KEY_NAME_PREFIX) + guidText.data());

    // Schannel signs with the key in its own process, so it has to find the key by name in a key store: a key that lives
    // only in this process would not do (ADR-060).
    winrt::check_hresult(NCryptCreatePersistedKey(provider, &key, NCRYPT_RSA_ALGORITHM, keyInfo.name.c_str(), AT_KEYEXCHANGE, 0));
    DWORD keyBits = KEY_BITS;
    winrt::check_hresult(NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&keyBits), sizeof(keyBits), 0));
    winrt::check_hresult(NCryptFinalizeKey(key, 0));

    DWORD subjectBytes = 0;
    winrt::check_bool(CertStrToNameW(X509_ASN_ENCODING, L"CN=localhost", CERT_X500_NAME_STR, nullptr, nullptr, &subjectBytes, nullptr));
    std::vector<BYTE> subjectName(subjectBytes);
    winrt::check_bool(
      CertStrToNameW(X509_ASN_ENCODING, L"CN=localhost", CERT_X500_NAME_STR, nullptr, subjectName.data(), &subjectBytes, nullptr));
    CERT_NAME_BLOB subject{.cbData = subjectBytes, .pbData = subjectName.data()};

    std::string signatureAlgorithm = szOID_RSA_SHA256RSA;
    CRYPT_ALGORITHM_IDENTIFIER signature{};
    signature.pszObjId = signatureAlgorithm.data();
    // Valid from now, until _end or for a year, the default.
    SYSTEMTIME end = _end.value_or(SYSTEMTIME{});
    certificate = winrt::check_pointer(
      CertCreateSelfSignCertificate(key, &subject, 0, &keyInfo.info, &signature, nullptr, _end.has_value() ? &end : nullptr, nullptr));
    return keyInfo.name;
  }
};

Neuron::ServerCertificate::ServerCertificate(const std::filesystem::path& _folder)
  : m_state(std::make_unique<State>())
{
  State& state = *m_state;
  winrt::check_hresult(NCryptOpenStorageProvider(&state.provider, MS_KEY_STORAGE_PROVIDER, 0));
  if (_folder.empty())
    (void)state.Make(std::nullopt);
  else if (!state.OpenKept(_folder))
  {
    // A new certificate for the folder, valid for KEPT_YEARS from now, whose key stays in the store.
    SYSTEMTIME end{};
    GetSystemTime(&end);
    end.wYear = static_cast<WORD>(end.wYear + KEPT_YEARS);
    // The 29th of February has no match five years on.
    if (end.wMonth == 2 && end.wDay == 29)
      end.wDay = 28;
    const std::wstring name = state.Make(end);
    state.kept = true;
    std::filesystem::create_directories(_folder);
    WriteBytes(_folder / KEY_NAME_FILE, std::as_bytes(std::span(name)));
    WriteBytes(_folder / CERTIFICATE_FILE, std::as_bytes(std::span(state.certificate->pbCertEncoded, state.certificate->cbCertEncoded)));
  }

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
