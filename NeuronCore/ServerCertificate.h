#pragma once

// Used by QuicChannel.cpp and its tests only, so NeuronCore.h does not include it.

namespace Neuron
{
// A QUIC server's TLS identity: a self-signed certificate for localhost, made when it is constructed and gone when it is
// destroyed (ADR-060). Its RSA key is persisted in the user's key store under a name of its own, because Schannel cannot
// use a key that lives only in this process, and it is deleted from there with the certificate.
class ServerCertificate : NonCopyable
{
public:
  // Throws winrt::hresult_error when Windows cannot make the key or the certificate.
  ServerCertificate();
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
