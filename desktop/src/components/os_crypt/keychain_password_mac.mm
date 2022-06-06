// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/keychain_password_mac.h"

#import <Security/Security.h>

#include "base/base64.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "crypto/apple_keychain.h"

using crypto::AppleKeychain;

namespace {

// Generates a random password and adds it to the Keychain.  The added password
// is returned from the function.  If an error occurs, an empty password is
// returned.
std::string AddRandomPasswordToKeychain(const AppleKeychain& keychain,
                                        const std::string& service_name,
                                        const std::string& account_name) {
  // Generate a password with 128 bits of randomness.
  const int kBytes = 128 / 8;
  std::string password;
  base::Base64Encode(base::RandBytesAsString(kBytes), &password);
  void* password_data =
      const_cast<void*>(static_cast<const void*>(password.data()));

  OSStatus error = keychain.AddGenericPassword(
      service_name.size(), service_name.data(), account_name.size(),
      account_name.data(), password.size(), password_data, NULL);

  if (error != noErr) {
    OSSTATUS_DLOG(ERROR, error) << "Keychain add failed";
    return std::string();
  }

  return password;
}

}  // namespace

// These two strings ARE indeed user facing.  But they are used to access
// the encryption keyword.  So as to not lose encrypted data when system
// locale changes we DO NOT LOCALIZE.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
KeychainPassword::KeychainNameType KeychainPassword::service_name("Chrome Safe Storage");
KeychainPassword::KeychainNameType KeychainPassword::account_name("Chrome");
#else
KeychainPassword::KeychainNameType KeychainPassword::service_name("NetboxBrowser Safe Storage");
KeychainPassword::KeychainNameType KeychainPassword::account_name("NetboxBrowser");
#endif

KeychainPassword::KeychainPassword(const AppleKeychain& keychain)
    : keychain_(keychain) {}

KeychainPassword::~KeychainPassword() = default;

std::string KeychainPassword::GetPassword() const {
  UInt32 password_length = 0;
  void* password_data = nullptr;
  OSStatus error = keychain_.FindGenericPassword(
      service_name->size(), service_name->c_str(),
      account_name->size(), account_name->c_str(), &password_length,
      &password_data, nullptr);

  if (error == noErr) {
    std::string password =
        std::string(static_cast<char*>(password_data), password_length);
    keychain_.ItemFreeContent(password_data);
    return password;
  }

  if (error == errSecItemNotFound) {
    std::string password = AddRandomPasswordToKeychain(
        keychain_, *service_name, *account_name);
    return password;
  }

  OSSTATUS_LOG(ERROR, error) << "Keychain lookup failed";
  return std::string();
}
