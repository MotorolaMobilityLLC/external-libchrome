// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_RSA_PRIVATE_KEY_H_
#define CRYPTO_RSA_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

#if defined(USE_OPENSSL)
// Forward declaration for openssl/*.h
typedef struct evp_pkey_st EVP_PKEY;
#else
// Forward declaration.
typedef struct PK11SlotInfoStr PK11SlotInfo;
typedef struct SECKEYPrivateKeyStr SECKEYPrivateKey;
typedef struct SECKEYPublicKeyStr SECKEYPublicKey;
#endif


namespace crypto {

// Used internally by RSAPrivateKey for serializing and deserializing
// PKCS #8 PrivateKeyInfo and PublicKeyInfo.
class PrivateKeyInfoCodec {
 public:
  // ASN.1 encoding of the AlgorithmIdentifier from PKCS #8.
  static const uint8_t kRsaAlgorithmIdentifier[];

  // ASN.1 tags for some types we use.
  static const uint8_t kBitStringTag = 0x03;
  static const uint8_t kIntegerTag = 0x02;
  static const uint8_t kNullTag = 0x05;
  static const uint8_t kOctetStringTag = 0x04;
  static const uint8_t kSequenceTag = 0x30;

  // |big_endian| here specifies the byte-significance of the integer components
  // that will be parsed & serialized (modulus(), etc...) during Import(),
  // Export() and ExportPublicKeyInfo() -- not the ASN.1 DER encoding of the
  // PrivateKeyInfo/PublicKeyInfo (which is always big-endian).
  explicit PrivateKeyInfoCodec(bool big_endian);

  ~PrivateKeyInfoCodec();

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the PrivateKeyInfo structure to |output|.
  bool Export(std::vector<uint8_t>* output);

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the PublicKeyInfo structure to |output|.
  bool ExportPublicKeyInfo(std::vector<uint8_t>* output);

  // Exports the contents of the integer components to the ASN.1 DER encoding
  // of the RSAPublicKey structure to |output|.
  bool ExportPublicKey(std::vector<uint8_t>* output);

  // Parses the ASN.1 DER encoding of the PrivateKeyInfo structure in |input|
  // and populates the integer components with |big_endian_| byte-significance.
  // IMPORTANT NOTE: This is currently *not* security-approved for importing
  // keys from unstrusted sources.
  bool Import(const std::vector<uint8_t>& input);

  // Accessors to the contents of the integer components of the PrivateKeyInfo
  // structure.
  std::vector<uint8_t>* modulus() { return &modulus_; }
  std::vector<uint8_t>* public_exponent() { return &public_exponent_; }
  std::vector<uint8_t>* private_exponent() { return &private_exponent_; }
  std::vector<uint8_t>* prime1() { return &prime1_; }
  std::vector<uint8_t>* prime2() { return &prime2_; }
  std::vector<uint8_t>* exponent1() { return &exponent1_; }
  std::vector<uint8_t>* exponent2() { return &exponent2_; }
  std::vector<uint8_t>* coefficient() { return &coefficient_; }

 private:
  // Utility wrappers for PrependIntegerImpl that use the class's |big_endian_|
  // value.
  void PrependInteger(const std::vector<uint8_t>& in, std::list<uint8_t>* out);
  void PrependInteger(uint8_t* val, int num_bytes, std::list<uint8_t>* data);

  // Prepends the integer stored in |val| - |val + num_bytes| with |big_endian|
  // byte-significance into |data| as an ASN.1 integer.
  void PrependIntegerImpl(uint8_t* val,
                          int num_bytes,
                          std::list<uint8_t>* data,
                          bool big_endian);

  // Utility wrappers for ReadIntegerImpl that use the class's |big_endian_|
  // value.
  bool ReadInteger(uint8_t** pos, uint8_t* end, std::vector<uint8_t>* out);
  bool ReadIntegerWithExpectedSize(uint8_t** pos,
                                   uint8_t* end,
                                   size_t expected_size,
                                   std::vector<uint8_t>* out);

  // Reads an ASN.1 integer from |pos|, and stores the result into |out| with
  // |big_endian| byte-significance.
  bool ReadIntegerImpl(uint8_t** pos,
                       uint8_t* end,
                       std::vector<uint8_t>* out,
                       bool big_endian);

  // Prepends the integer stored in |val|, starting a index |start|, for
  // |num_bytes| bytes onto |data|.
  void PrependBytes(uint8_t* val,
                    int start,
                    int num_bytes,
                    std::list<uint8_t>* data);

  // Helper to prepend an ASN.1 length field.
  void PrependLength(size_t size, std::list<uint8_t>* data);

  // Helper to prepend an ASN.1 type header.
  void PrependTypeHeaderAndLength(uint8_t type,
                                  uint32_t length,
                                  std::list<uint8_t>* output);

  // Helper to prepend an ASN.1 bit string
  void PrependBitString(uint8_t* val,
                        int num_bytes,
                        std::list<uint8_t>* output);

  // Read an ASN.1 length field. This also checks that the length does not
  // extend beyond |end|.
  bool ReadLength(uint8_t** pos, uint8_t* end, uint32_t* result);

  // Read an ASN.1 type header and its length.
  bool ReadTypeHeaderAndLength(uint8_t** pos,
                               uint8_t* end,
                               uint8_t expected_tag,
                               uint32_t* length);

  // Read an ASN.1 sequence declaration. This consumes the type header and
  // length field, but not the contents of the sequence.
  bool ReadSequence(uint8_t** pos, uint8_t* end);

  // Read the RSA AlgorithmIdentifier.
  bool ReadAlgorithmIdentifier(uint8_t** pos, uint8_t* end);

  // Read one of the two version fields in PrivateKeyInfo.
  bool ReadVersion(uint8_t** pos, uint8_t* end);

  // The byte-significance of the stored components (modulus, etc..).
  bool big_endian_;

  // Component integers of the PrivateKeyInfo
  std::vector<uint8_t> modulus_;
  std::vector<uint8_t> public_exponent_;
  std::vector<uint8_t> private_exponent_;
  std::vector<uint8_t> prime1_;
  std::vector<uint8_t> prime2_;
  std::vector<uint8_t> exponent1_;
  std::vector<uint8_t> exponent2_;
  std::vector<uint8_t> coefficient_;

  DISALLOW_COPY_AND_ASSIGN(PrivateKeyInfoCodec);
};

// Encapsulates an RSA private key. Can be used to generate new keys, export
// keys to other formats, or to extract a public key.
// TODO(hclam): This class should be ref-counted so it can be reused easily.
class CRYPTO_EXPORT RSAPrivateKey {
 public:
  ~RSAPrivateKey();

  // Create a new random instance. Can return NULL if initialization fails.
  static RSAPrivateKey* Create(uint16_t num_bits);

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static RSAPrivateKey* CreateFromPrivateKeyInfo(
      const std::vector<uint8_t>& input);

#if defined(USE_OPENSSL)
  // Create a new instance from an existing EVP_PKEY, taking a
  // reference to it. |key| must be an RSA key. Returns NULL on
  // failure.
  static RSAPrivateKey* CreateFromKey(EVP_PKEY* key);
#else
  // Create a new instance by referencing an existing private key
  // structure.  Does not import the key.
  static RSAPrivateKey* CreateFromKey(SECKEYPrivateKey* key);
#endif

#if defined(USE_OPENSSL)
  EVP_PKEY* key() { return key_; }
#else
  SECKEYPrivateKey* key() { return key_; }
  SECKEYPublicKey* public_key() { return public_key_; }
#endif

  // Creates a copy of the object.
  RSAPrivateKey* Copy() const;

  // Exports the private key to a PKCS #1 PrivateKey block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  RSAPrivateKey();

#if defined(USE_OPENSSL)
  EVP_PKEY* key_;
#else
  SECKEYPrivateKey* key_;
  SECKEYPublicKey* public_key_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RSAPrivateKey);
};

}  // namespace crypto

#endif  // CRYPTO_RSA_PRIVATE_KEY_H_
