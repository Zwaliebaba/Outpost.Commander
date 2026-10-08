#pragma once

// Used by QuicChannel.cpp only, so NeuronCore.h does not include it.

namespace Neuron
{
// A QUIC server's TLS identity: a self-signed certificate for localhost (ADR-060). Its RSA key is persisted in the user's
// key store under a name of its own, because Schannel cannot use a key that lives only in this process.
//
// Made without a folder, the certificate is the server's alone: made when it is constructed, valid for a year, and its key
// deleted from the store when it is destroyed. Made with one, it is kept (ADR-078): the folder holds the certificate and
// its key's name, the key stays in the store, and the next certificate made there is the same one, with the same hash, as
// long as it is valid and its key is in the store. Otherwise a new one is made there, valid for five years.
class ServerCertificate : NonCopyable
{
public:
  // Throws winrt::hresult_error when Windows cannot make the key or the certificate, and Neuron::Exception when the
  // folder cannot be written.
  explicit ServerCertificate(const std::filesystem::path& _folder = {});
  ~ServerCertificate();

  // The certificate as MsQuic takes it for QUIC_CREDENTIAL_TYPE_CERTIFICATE_CONTEXT: a PCCERT_CONTEXT.
  [[nodiscard]] void* Context() const noexcept;
  [[nodiscard]] const CertificateHash& Hash() const noexcept;

private:
  struct State;
  std::unique_ptr<State> m_state;
};

// The SHA-256 of a certificate's DER encoding, which is what a client pins.
[[nodiscard]] CertificateHash HashCertificate(std::span<const std::uint8_t> _der);
} // namespace Neuron
