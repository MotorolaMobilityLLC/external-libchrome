// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/ec_private_key.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_openssl_types.h"

namespace crypto {

namespace {

// Function pointer definition, for injecting the required key export function
// into ExportKeyWithBio, below. |bio| is a temporary memory BIO object, and
// |key| is a handle to the input key object. Return 1 on success, 0 otherwise.
// NOTE: Used with OpenSSL functions, which do not comply with the Chromium
//       style guide, hence the unusual parameter placement / types.
typedef int (*ExportBioFunction)(BIO* bio, const void* key);

using ScopedPKCS8_PRIV_KEY_INFO =
    ScopedOpenSSL<PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free>;
using ScopedX509_SIG = ScopedOpenSSL<X509_SIG, X509_SIG_free>;

// Helper to export |key| into |output| via the specified ExportBioFunction.
bool ExportKeyWithBio(const void* key,
                      ExportBioFunction export_fn,
                      std::vector<uint8>* output) {
  if (!key)
    return false;

  ScopedBIO bio(BIO_new(BIO_s_mem()));
  if (!bio.get())
    return false;

  if (!export_fn(bio.get(), key))
    return false;

  char* data = NULL;
  long len = BIO_get_mem_data(bio.get(), &data);
  if (!data || len < 0)
    return false;

  output->assign(data, data + len);
  return true;
}

// Function pointer definition, for injecting the required key export function
// into ExportKey below. |key| is a pointer to the input key object,
// and |data| is either NULL, or the address of an 'unsigned char*' pointer
// that points to the start of the output buffer. The function must return
// the number of bytes required to export the data, or -1 in case of error.
typedef int (*ExportDataFunction)(const void* key, unsigned char** data);

// Helper to export |key| into |output| via the specified export function.
bool ExportKey(const void* key,
               ExportDataFunction export_fn,
               std::vector<uint8>* output) {
  if (!key)
    return false;

  int data_len = export_fn(key, NULL);
  if (data_len < 0)
    return false;

  output->resize(static_cast<size_t>(data_len));
  unsigned char* data = &(*output)[0];
  if (export_fn(key, &data) < 0)
    return false;

  return true;
}

}  // namespace

ECPrivateKey::~ECPrivateKey() {
  if (key_)
    EVP_PKEY_free(key_);
}

ECPrivateKey* ECPrivateKey::Copy() const {
  scoped_ptr<ECPrivateKey> copy(new ECPrivateKey);
  if (key_)
    copy->key_ = EVP_PKEY_up_ref(key_);
  return copy.release();
}

// static
bool ECPrivateKey::IsSupported() { return true; }

// static
ECPrivateKey* ECPrivateKey::Create() {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  ScopedEC_KEY ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!ec_key.get() || !EC_KEY_generate_key(ec_key.get()))
    return NULL;

  scoped_ptr<ECPrivateKey> result(new ECPrivateKey());
  result->key_ = EVP_PKEY_new();
  if (!result->key_ || !EVP_PKEY_set1_EC_KEY(result->key_, ec_key.get()))
    return NULL;

  CHECK_EQ(EVP_PKEY_EC, EVP_PKEY_type(result->key_->type));
  return result.release();
}

// static
ECPrivateKey* ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
    const std::string& password,
    const std::vector<uint8>& encrypted_private_key_info,
    const std::vector<uint8>& subject_public_key_info) {
  // NOTE: The |subject_public_key_info| can be ignored here, it is only
  // useful for the NSS implementation (which uses the public key's SHA1
  // as a lookup key when storing the private one in its store).
  if (encrypted_private_key_info.empty())
    return NULL;

  OpenSSLErrStackTracer err_tracer(FROM_HERE);

  const uint8_t* data = &encrypted_private_key_info[0];
  const uint8_t* ptr = data;
  ScopedX509_SIG p8_encrypted(
      d2i_X509_SIG(NULL, &ptr, encrypted_private_key_info.size()));
  if (!p8_encrypted || ptr != data + encrypted_private_key_info.size())
    return NULL;

  ScopedPKCS8_PRIV_KEY_INFO p8_decrypted;
  if (password.empty()) {
    // Hack for reading keys generated by an older version of the OpenSSL
    // code. OpenSSL used to use "\0\0" rather than the empty string because it
    // would treat the password as an ASCII string to be converted to UCS-2
    // while NSS used a byte string.
    p8_decrypted.reset(PKCS8_decrypt_pbe(
        p8_encrypted.get(), reinterpret_cast<const uint8_t*>("\0\0"), 2));
  }
  if (!p8_decrypted) {
    p8_decrypted.reset(PKCS8_decrypt_pbe(
        p8_encrypted.get(),
        reinterpret_cast<const uint8_t*>(password.data()),
        password.size()));
  }

  if (!p8_decrypted)
    return NULL;

  // Create a new EVP_PKEY for it.
  scoped_ptr<ECPrivateKey> result(new ECPrivateKey);
  result->key_ = EVP_PKCS82PKEY(p8_decrypted.get());
  if (!result->key_ || EVP_PKEY_type(result->key_->type) != EVP_PKEY_EC)
    return NULL;

  return result.release();
}

bool ECPrivateKey::ExportEncryptedPrivateKey(
    const std::string& password,
    int iterations,
    std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Convert into a PKCS#8 object.
  ScopedPKCS8_PRIV_KEY_INFO pkcs8(EVP_PKEY2PKCS8(key_));
  if (!pkcs8.get())
    return false;

  // Encrypt the object.
  // NOTE: NSS uses SEC_OID_PKCS12_V2_PBE_WITH_SHA1_AND_3KEY_TRIPLE_DES_CBC
  // so use NID_pbe_WithSHA1And3_Key_TripleDES_CBC which should be the OpenSSL
  // equivalent.
  ScopedX509_SIG encrypted(PKCS8_encrypt_pbe(
      NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
      reinterpret_cast<const uint8_t*>(password.data()),
      password.size(),
      NULL,
      0,
      iterations,
      pkcs8.get()));
  if (!encrypted.get())
    return false;

  // Write it into |*output|
  return ExportKeyWithBio(encrypted.get(),
                          reinterpret_cast<ExportBioFunction>(i2d_PKCS8_bio),
                          output);
}

bool ECPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  return ExportKeyWithBio(
      key_, reinterpret_cast<ExportBioFunction>(i2d_PUBKEY_bio), output);
}

bool ECPrivateKey::ExportRawPublicKey(std::string* output) {
  // i2d_PublicKey will produce an ANSI X9.62 public key which, for a P-256
  // key, is 0x04 (meaning uncompressed) followed by the x and y field
  // elements as 32-byte, big-endian numbers.
  static const int kExpectedKeyLength = 65;

  int len = i2d_PublicKey(key_, NULL);
  if (len != kExpectedKeyLength)
    return false;

  uint8 buf[kExpectedKeyLength];
  uint8* derp = buf;
  len = i2d_PublicKey(key_, &derp);
  if (len != kExpectedKeyLength)
    return false;

  output->assign(reinterpret_cast<char*>(buf + 1), kExpectedKeyLength - 1);
  return true;
}

bool ECPrivateKey::ExportValue(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  ScopedEC_KEY ec_key(EVP_PKEY_get1_EC_KEY(key_));
  return ExportKey(ec_key.get(),
                   reinterpret_cast<ExportDataFunction>(i2d_ECPrivateKey),
                   output);
}

bool ECPrivateKey::ExportECParams(std::vector<uint8>* output) {
  OpenSSLErrStackTracer err_tracer(FROM_HERE);
  ScopedEC_KEY ec_key(EVP_PKEY_get1_EC_KEY(key_));
  return ExportKey(ec_key.get(),
                   reinterpret_cast<ExportDataFunction>(i2d_ECParameters),
                   output);
}

ECPrivateKey::ECPrivateKey() : key_(NULL) {}

}  // namespace crypto
