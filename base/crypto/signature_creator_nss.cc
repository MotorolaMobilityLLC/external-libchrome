// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/crypto/signature_creator.h"

#include <cryptohi.h>
#include <keyhi.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/nss_init.h"
#include "base/scoped_ptr.h"

namespace base {

// static
SignatureCreator* SignatureCreator::Create(RSAPrivateKey* key) {
  scoped_ptr<SignatureCreator> result(new SignatureCreator);
  result->key_ = key;

  result->sign_context_ = SGN_NewContext(SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION,
      key->key());
  if (!result->sign_context_) {
    NOTREACHED();
    return NULL;
  }

  SECStatus rv = SGN_Begin(result->sign_context_);
  if (rv != SECSuccess) {
    NOTREACHED();
    return NULL;
  }

  return result.release();
}

SignatureCreator::SignatureCreator() : sign_context_(NULL) {
  EnsureNSSInit();
}

SignatureCreator::~SignatureCreator() {
  if (sign_context_) {
    SGN_DestroyContext(sign_context_, PR_TRUE);
    sign_context_ = NULL;
  }
}

bool SignatureCreator::Update(const uint8* data_part, int data_part_len) {
  // TODO(wtc): Remove this const_cast when we require NSS 3.12.5.
  // See NSS bug https://bugzilla.mozilla.org/show_bug.cgi?id=518255
  SECStatus rv = SGN_Update(sign_context_,
                            const_cast<unsigned char*>(data_part),
                            data_part_len);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SignatureCreator::Final(std::vector<uint8>* signature) {
  SECItem signature_item;
  SECStatus rv = SGN_End(sign_context_, &signature_item);
  if (rv != SECSuccess) {
    NOTREACHED();
    return false;
  }
  signature->assign(signature_item.data,
                    signature_item.data + signature_item.len);
  SECITEM_FreeItem(&signature_item, PR_FALSE);
  return true;
}

}  // namespace base
