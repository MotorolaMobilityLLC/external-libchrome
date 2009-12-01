// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/rsa_private_key.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <pk11pub.h>

#include <iostream>
#include <list>

#include "base/logging.h"
#include "base/nss_init.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

// TODO(rafaelw): Consider refactoring common functions and definitions from
// rsa_private_key_win.cc or using NSS's ASN.1 encoder.
namespace {

static bool ReadAttribute(SECKEYPrivateKey* key,
                          CK_ATTRIBUTE_TYPE type,
                          std::vector<uint8>* output) {
  SECItem item;
  SECStatus rv;
  rv = PK11_ReadRawAttribute(PK11_TypePrivKey, key, type, &item);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }

  output->assign(item.data, item.data + item.len);
  SECITEM_FreeItem(&item, PR_FALSE);
  return true;
}

}  // namespace

namespace base {

// static
RSAPrivateKey* RSAPrivateKey::Create(uint16 num_bits) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  if (!slot)
    return NULL;

  PK11RSAGenParams param;
  param.keySizeInBits = num_bits;
  param.pe = 65537L;
  result->key_ = PK11_GenerateKeyPair(slot, CKM_RSA_PKCS_KEY_PAIR_GEN, &param,
      &result->public_key_, PR_FALSE, PR_FALSE, NULL);
  PK11_FreeSlot(slot);
  if (!result->key_)
    return NULL;

  return result.release();
}

// static
RSAPrivateKey* RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8>& input) {
  scoped_ptr<RSAPrivateKey> result(new RSAPrivateKey);

  PK11SlotInfo *slot = PK11_GetInternalSlot();
  if (!slot)
    return NULL;

  SECItem der_private_key_info;
  der_private_key_info.data = const_cast<unsigned char*>(&input.front());
  der_private_key_info.len = input.size();
  SECStatus rv =  PK11_ImportDERPrivateKeyInfoAndReturnKey(slot,
      &der_private_key_info, NULL, NULL, PR_FALSE, PR_FALSE,
      KU_DIGITAL_SIGNATURE, &result->key_, NULL);
  PK11_FreeSlot(slot);
  if (rv != SECSuccess) {
    NOTREACHED();
    return NULL;
  }

  result->public_key_ = SECKEY_ConvertToPublicKey(result->key_);
  if (!result->public_key_) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

RSAPrivateKey::RSAPrivateKey() : key_(NULL), public_key_(NULL) {
  EnsureNSSInit();
}

RSAPrivateKey::~RSAPrivateKey() {
  if (key_)
    SECKEY_DestroyPrivateKey(key_);
  if (public_key_)
    SECKEY_DestroyPublicKey(public_key_);
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8>* output) {
  PrivateKeyInfoCodec private_key_info(true);

  // Manually read the component attributes of the private key and build up
  // the PrivateKeyInfo.
  if (!ReadAttribute(key_, CKA_MODULUS, private_key_info.modulus()) ||
      !ReadAttribute(key_, CKA_PUBLIC_EXPONENT,
          private_key_info.public_exponent()) ||
      !ReadAttribute(key_, CKA_PRIVATE_EXPONENT,
          private_key_info.private_exponent()) ||
      !ReadAttribute(key_, CKA_PRIME_1, private_key_info.prime1()) ||
      !ReadAttribute(key_, CKA_PRIME_2, private_key_info.prime2()) ||
      !ReadAttribute(key_, CKA_EXPONENT_1, private_key_info.exponent1()) ||
      !ReadAttribute(key_, CKA_EXPONENT_2, private_key_info.exponent2()) ||
      !ReadAttribute(key_, CKA_COEFFICIENT, private_key_info.coefficient())) {
    NOTREACHED();
    return false;
  }

  return private_key_info.Export(output);
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8>* output) {
  SECItem* der_pubkey = SECKEY_EncodeDERSubjectPublicKeyInfo(public_key_);
  if (!der_pubkey) {
    NOTREACHED();
    return false;
  }

  for (size_t i = 0; i < der_pubkey->len; ++i)
    output->push_back(der_pubkey->data[i]);

  SECITEM_FreeItem(der_pubkey, PR_TRUE);
  return true;
}

}  // namespace base
