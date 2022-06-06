// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/message_formatter.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_compare.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/icu/source/common/unicode/rbbi.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_ANDROID)
#include "base/android/locale_utils.h"
#include "ui/base/l10n/l10n_util_android.h"
#endif

#if defined(USE_GLIB)
#include <glib.h>
#endif

#if defined(OS_WIN)
#include "base/logging.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif  // OS_WIN

namespace {

static const char* const kAcceptLanguageList[] = {
    "af",  // Afrikaans
    "am",  // Amharic
    "an",  // Aragonese
    "ar",  // Arabic
#if defined(ENABLE_PSEUDOLOCALES)
    "ar-XB",           // RTL Pseudolocale
#endif                 // defined(ENABLE_PSEUDOLOCALES)
    "as",              // Assamese
    "ast",             // Asturian
    "az",              // Azerbaijani
    "be",              // Belarusian
    "bg",              // Bulgarian
    "bn",              // Bengali
    "br",              // Breton
    "bs",              // Bosnian
    "ca",              // Catalan
    "ceb",             // Cebuano
    "chr",             // Cherokee
    "ckb",             // Kurdish (Arabic),  Sorani
    "co",              // Corsican
    "cs",              // Czech
    "cy",              // Welsh
    "da",              // Danish
    "de",              // German
    "de-AT",           // German (Austria)
    "de-CH",           // German (Switzerland)
    "de-DE",           // German (Germany)
    "de-LI",           // German (Liechtenstein)
    "el",              // Greek
    "en",              // English
    "en-AU",           // English (Australia)
    "en-CA",           // English (Canada)
    "en-GB",           // English (UK)
    "en-GB-oxendict",  // English (UK, OED spelling)
    "en-IN",           // English (India)
    "en-NZ",           // English (New Zealand)
    "en-US",           // English (US)
#if defined(ENABLE_PSEUDOLOCALES)
    "en-XA",  // Long strings Pseudolocale
#endif        // defined(ENABLE_PSEUDOLOCALES)
    "en-ZA",  // English (South Africa)
    "eo",     // Esperanto
    // TODO(jungshik) : Do we want to list all es-Foo for Latin-American
    // Spanish speaking countries?
    "es",      // Spanish
    "es-419",  // Spanish (Latin America)
    "es-AR",   // Spanish (Argentina)
    "es-CL",   // Spanish (Chile)
    "es-CO",   // Spanish (Colombia)
    "es-CR",   // Spanish (Costa Rica)
    "es-ES",   // Spanish (Spain)
    "es-HN",   // Spanish (Honduras)
    "es-MX",   // Spanish (Mexico)
    "es-PE",   // Spanish (Peru)
    "es-US",   // Spanish (US)
    "es-UY",   // Spanish (Uruguay)
    "es-VE",   // Spanish (Venezuela)
    "et",      // Estonian
    "eu",      // Basque
    "fa",      // Persian
    "fi",      // Finnish
    "fil",     // Filipino
    "fo",      // Faroese
    "fr",      // French
    "fr-CA",   // French (Canada)
    "fr-CH",   // French (Switzerland)
    "fr-FR",   // French (France)
    "fy",      // Frisian
    "ga",      // Irish
    "gd",      // Scots Gaelic
    "gl",      // Galician
    "gn",      // Guarani
    "gu",      // Gujarati
    "ha",      // Hausa
    "haw",     // Hawaiian
    "he",      // Hebrew
    "hi",      // Hindi
    "hmn",     // Hmong
    "hr",      // Croatian
    "ht",      // Haitian Creole
    "hu",      // Hungarian
    "hy",      // Armenian
    "ia",      // Interlingua
    "id",      // Indonesian
    "ig",      // Igbo
    "is",      // Icelandic
    "it",      // Italian
    "it-CH",   // Italian (Switzerland)
    "it-IT",   // Italian (Italy)
    "ja",      // Japanese
    "jv",      // Javanese
    "ka",      // Georgian
    "kk",      // Kazakh
    "km",      // Cambodian
    "kn",      // Kannada
    "ko",      // Korean
    "kok",     // Konkani
    "ku",      // Kurdish
    "ky",      // Kyrgyz
    "la",      // Latin
    "lb",      // Luxembourgish
    "ln",      // Lingala
    "lo",      // Laothian
    "lt",      // Lithuanian
    "lv",      // Latvian
    "mg",      // Malagasy
    "mi",      // Maori
    "mk",      // Macedonian
    "ml",      // Malayalam
    "mn",      // Mongolian
    "mo",      // Moldavian
    "mr",      // Marathi
    "ms",      // Malay
    "mt",      // Maltese
    "my",      // Burmese
    "nb",      // Norwegian (Bokmal)
    "ne",      // Nepali
    "nl",      // Dutch
    "nn",      // Norwegian (Nynorsk)
    "no",      // Norwegian
    "ny",      // Nyanja
    "oc",      // Occitan
    "om",      // Oromo
    "or",      // Odia (Oriya)
    "pa",      // Punjabi
    "pl",      // Polish
    "ps",      // Pashto
    "pt",      // Portuguese
    "pt-BR",   // Portuguese (Brazil)
    "pt-PT",   // Portuguese (Portugal)
    "qu",      // Quechua
    "rm",      // Romansh
    "ro",      // Romanian
    "ru",      // Russian
    "rw",      // Kinyarwanda
    "sd",      // Sindhi
    "sh",      // Serbo-Croatian
    "si",      // Sinhalese
    "sk",      // Slovak
    "sl",      // Slovenian
    "sm",      // Samoan
    "sn",      // Shona
    "so",      // Somali
    "sq",      // Albanian
    "sr",      // Serbian
    "st",      // Sesotho
    "su",      // Sundanese
    "sv",      // Swedish
    "sw",      // Swahili
    "ta",      // Tamil
    "te",      // Telugu
    "tg",      // Tajik
    "th",      // Thai
    "ti",      // Tigrinya
    "tk",      // Turkmen
    "tn",      // Tswana
    "to",      // Tonga
    "tr",      // Turkish
    "tt",      // Tatar
    "tw",      // Twi
    "ug",      // Uyghur
    "uk",      // Ukrainian
    "ur",      // Urdu
    "uz",      // Uzbek
    "vi",      // Vietnamese
    "wa",      // Walloon
    "wo",      // Wolof
    "xh",      // Xhosa
    "yi",      // Yiddish
    "yo",      // Yoruba
    "zh",      // Chinese
    "zh-CN",   // Chinese (China)
    "zh-HK",   // Chinese (Hong Kong)
    "zh-TW",   // Chinese (Taiwan)
    "zu",      // Zulu
};

// The list of locales that expected on the current platform, generated from the
// |locales| variable in GN (defined in build/config/locales.gni). This is
// equivalently the list of locales that we expect to have translation strings
// for on the current platform. Guaranteed to be in sorted order and guaranteed
// to have no duplicates.
//
// Note that this could have false positives at runtime on Android and iOS:
// - On Android, some locales aren't shipped (|android_apk_omitted_locales| in
//   GN), and some locales files are dynamically shipped in app bundles
//   (|android_bundle_only_locales|). Both of these lists are included in
//   this variable.
// - On iOS, some locales aren't shipped (|ios_unsupported_locales|) as they are
//   not supported by the operating system. These locales are included in this
//   variable.
//
// To avoid false positives on these platforms, use
// ui::ResourceBundle::LocaleDataPakExists() to check whether the locales exist
// on disk instead (requires I/O).
static const char* const kPlatformLocales[] = {
#define PLATFORM_LOCALE(locale) #locale,
// The below is generated by tools/l10n/generate_locales_list.py, which is
// run in the //ui/base:locales_list_gen build rule.
#include "ui/base/l10n/l10n_util_locales_list.inc"
#undef PLATFORM_LOCALE
};

// Returns true if |locale_name| has an alias in the ICU data file.
bool IsDuplicateName(const std::string& locale_name) {
  static const char* const kDuplicateNames[] = {
    "ar_001",
    "en",
    "en_001",
    "en_150",
    "pt",  // pt-BR and pt-PT are used.
    "zh",
    "zh_hans_cn",
    "zh_hant_hk",
    "zh_hant_mo",
    "zh_hans_sg",
    "zh_hant_tw"
  };

  // Skip all the es_Foo other than es_419 for now.
  if (base::StartsWith(locale_name, "es_",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return !base::EndsWith(locale_name, "419", base::CompareCase::SENSITIVE);
  }
  for (const char* duplicate_name : kDuplicateNames) {
    if (base::EqualsCaseInsensitiveASCII(duplicate_name, locale_name))
      return true;
  }
  return false;
}

// We added 30+ minimally populated locales with only a few entries
// (exemplar character set, script, writing direction and its own
// lanaguage name). These locales have to be distinguished from the
// fully populated locales to which Chrome is localized.
bool IsLocalePartiallyPopulated(const std::string& locale_name) {
  // For partially populated locales, even the translation for "English"
  // is not available. A more robust/elegant way to check is to add a special
  // field (say, 'isPartial' to our version of ICU locale files) and
  // check its value, but this hack seems to work well.
  return !l10n_util::IsLocaleNameTranslated("en", locale_name);
}

// If |perform_io| is false, this will not perform any I/O but may return false
// positives on Android and iOS. See the |kPlatformLocales| documentation for
// more information.
bool HasStringsForLocale(const std::string& locale,
                         const bool perform_io = true) {
  if (!perform_io) {
    return std::binary_search(std::begin(kPlatformLocales),
                              std::end(kPlatformLocales), locale);
  }
  // If locale has any illegal characters in it, we don't want to try to
  // load it because it may be pointing outside the locale data file directory.
  if (!base::i18n::IsFilenameLegal(base::ASCIIToUTF16(locale)))
    return false;

  // IsLocalePartiallyPopulated() can be called here for an early return w/o
  // checking the resource availability below. It'd help when Chrome is run
  // under a system locale Chrome is not localized to (e.g. Farsi on Linux),
  // but it'd slow down the start up time a little bit for locales Chrome is
  // localized to. So, we don't call it here.
  return ui::ResourceBundle::LocaleDataPakExists(locale);
}

// On Linux, the text layout engine Pango determines paragraph directionality
// by looking at the first strongly-directional character in the text. This
// means text such as "Google Chrome foo bar..." will be layed out LTR even
// if "foo bar" is RTL. So this function prepends the necessary RLM in such
// cases.
void AdjustParagraphDirectionality(std::u16string* paragraph) {
#if defined(OS_POSIX) && !defined(OS_APPLE) && !defined(OS_ANDROID)
  if (base::i18n::IsRTL() &&
      base::i18n::StringContainsStrongRTLChars(*paragraph)) {
    paragraph->insert(0, 1, char16_t{base::i18n::kRightToLeftMark});
  }
#endif
}

struct AvailableLocalesTraits
    : base::internal::DestructorAtExitLazyInstanceTraits<
          std::vector<std::string>> {
  static std::vector<std::string>* New(void* instance) {
    std::vector<std::string>* locales =
        base::internal::DestructorAtExitLazyInstanceTraits<
            std::vector<std::string>>::New(instance);
    int num_locales = uloc_countAvailable();
    for (int i = 0; i < num_locales; ++i) {
      std::string locale_name = uloc_getAvailable(i);
      // Filter out the names that have aliases.
      if (IsDuplicateName(locale_name))
        continue;
      // Filter out locales for which we have only partially populated data
      // and to which Chrome is not localized.
      if (IsLocalePartiallyPopulated(locale_name))
        continue;
      // Normalize underscores to hyphens because that's what our locale files
      // use.
      std::replace(locale_name.begin(), locale_name.end(), '_', '-');

      // Map the Chinese locale names over to zh-CN and zh-TW.
      if (base::LowerCaseEqualsASCII(locale_name, "zh-hans")) {
        locale_name = "zh-CN";
      } else if (base::LowerCaseEqualsASCII(locale_name, "zh-hant")) {
        locale_name = "zh-TW";
      }
      locales->push_back(locale_name);
    }

    return locales;
  }
};

base::LazyInstance<std::vector<std::string>, AvailableLocalesTraits>
    g_available_locales = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace l10n_util {

std::string GetLanguage(const std::string& locale) {
  const std::string::size_type hyphen_pos = locale.find('-');
  return std::string(locale, 0, hyphen_pos);
}

// TODO(jshin): revamp this function completely to use a more systematic
// and generic locale fallback based on ICU/CLDR.
bool CheckAndResolveLocale(const std::string& locale,
                           std::string* resolved_locale,
                           const bool perform_io) {
  if (HasStringsForLocale(locale, perform_io)) {
    *resolved_locale = locale;
    return true;
  }

  // If there's a variant, skip over it so we can try without the region
  // code.  For example, ca_ES@valencia should cause us to try ca@valencia
  // before ca.
  std::string::size_type variant_pos = locale.find('@');
  if (variant_pos != std::string::npos)
    return false;

  // If the locale matches language but not country, use that instead.
  // TODO(jungshik) : Nothing is done about languages that Chrome
  // does not support but available on Windows. We fall
  // back to en-US in GetApplicationLocale so that it's a not critical,
  // but we can do better.
  const std::string lang(GetLanguage(locale));
  if (lang.size() < locale.size()) {
    std::string region(locale, lang.size() + 1);
    std::string tmp_locale(lang);
    // Map es-RR other than es-ES to es-419 (Chrome's Latin American
    // Spanish locale).
    if (base::LowerCaseEqualsASCII(lang, "es") &&
        !base::LowerCaseEqualsASCII(region, "es")) {
#if defined(OS_IOS)
      // iOS uses a different name for es-419 (es-MX).
      tmp_locale.append("-MX");
#else
      tmp_locale.append("-419");
#endif
    } else if (base::LowerCaseEqualsASCII(lang, "pt") &&
               !base::LowerCaseEqualsASCII(region, "br")) {
      // Map pt-RR other than pt-BR to pt-PT. Note that "pt" by itself maps to
      // pt-BR (logic below), and we need to explicitly check for pt-BR here as
      // it is unavailable on iOS.
      tmp_locale.append("-PT");
    } else if (base::LowerCaseEqualsASCII(lang, "zh")) {
      // Map zh-HK and zh-MO to zh-TW. Otherwise, zh-FOO is mapped to zh-CN.
      if (base::LowerCaseEqualsASCII(region, "hk") ||
          base::LowerCaseEqualsASCII(region, "mo")) {  // Macao
        tmp_locale.append("-TW");
      } else {
        tmp_locale.append("-CN");
      }
    } else if (base::LowerCaseEqualsASCII(lang, "en")) {
      // Map Liberian and Filipino English to US English, and everything
      // else to British English.
      // TODO(jungshik): en-CA may have to change sides once
      // we have OS locale separate from app locale (Chrome's UI language).
      if (base::LowerCaseEqualsASCII(region, "lr") ||
          base::LowerCaseEqualsASCII(region, "ph")) {
        tmp_locale.append("-US");
      } else {
        tmp_locale.append("-GB");
      }
    }
    if (HasStringsForLocale(tmp_locale, perform_io)) {
      resolved_locale->swap(tmp_locale);
      return true;
    }
  }

  // Google updater uses no, tl, iw and en for our nb, fil, he, and en-US.
  // Note that pt-RR is mapped to pt-PT above, but we want pt -> pt-BR here.
  struct {
    const char* source;
    const char* dest;
  } static constexpr kAliasMap[] = {
      {"en", "en-US"}, {"iw", "he"},  {"no", "nb"},
      {"pt", "pt-BR"}, {"tl", "fil"}, {"zh", "zh-CN"},
  };
  for (const auto& alias : kAliasMap) {
    if (base::LowerCaseEqualsASCII(lang, alias.source)) {
      std::string tmp_locale(alias.dest);
      if (HasStringsForLocale(tmp_locale, perform_io)) {
        resolved_locale->swap(tmp_locale);
        return true;
      }
    }
  }

  return false;
}

bool CheckAndResolveLocale(const std::string& locale,
                           std::string* resolved_locale) {
  return CheckAndResolveLocale(locale, resolved_locale, /*perform_io=*/true);
}

#if defined(OS_APPLE)
std::string GetApplicationLocaleInternalMac(const std::string& pref_locale) {
  // Use any override (Cocoa for the browser), otherwise use the preference
  // passed to the function.
  std::string app_locale = l10n_util::GetLocaleOverride();
  if (app_locale.empty())
    app_locale = pref_locale;

  // The above should handle all of the cases Chrome normally hits, but for some
  // unit tests, we need something to fall back too.
  if (app_locale.empty())
    app_locale = "en-US";

  return app_locale;
}
#endif

#if !defined(OS_APPLE)
std::string GetApplicationLocaleInternalNonMac(const std::string& pref_locale) {
  std::string resolved_locale;
  std::vector<std::string> candidates;

  // We only use --lang and the app pref on Windows.  On Linux, we only
  // look at the LC_*/LANG environment variables.  We do, however, pass --lang
  // to renderer and plugin processes so they know what language the parent
  // process decided to use.

#if defined(OS_WIN)
  // First, try the preference value.
  if (!pref_locale.empty())
    candidates.push_back(base::i18n::GetCanonicalLocale(pref_locale));

  // Next, try the overridden locale.
  const std::vector<std::string>& languages = l10n_util::GetLocaleOverrides();
  if (!languages.empty()) {
    candidates.reserve(candidates.size() + languages.size());
    std::transform(languages.begin(), languages.end(),
                   std::back_inserter(candidates),
                   &base::i18n::GetCanonicalLocale);
  } else {
    // If no override was set, defer to ICU
    candidates.push_back(base::i18n::GetConfiguredLocale());
  }
#elif defined(OS_ANDROID)
  // Try pref_locale first.
  if (!pref_locale.empty())
    candidates.push_back(base::i18n::GetCanonicalLocale(pref_locale));

  // On Android, query java.util.Locale for the default locale.
  candidates.push_back(base::android::GetDefaultLocaleString());
#elif defined(USE_GLIB) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // GLib implements correct environment variable parsing with
  // the precedence order: LANGUAGE, LC_ALL, LC_MESSAGES and LANG.
  // We used to use our custom parsing code along with ICU for this purpose.
  // If we have a port that does not depend on GTK, we have to
  // restore our custom code for that port.
  const char* const* languages = g_get_language_names();
  DCHECK(languages);  // A valid pointer is guaranteed.
  DCHECK(*languages);  // At least one entry, "C", is guaranteed.

  for (; *languages; ++languages) {
    candidates.push_back(base::i18n::GetCanonicalLocale(*languages));
  }
#else
  // By default, use the application locale preference. This applies to ChromeOS
  // and linux systems without glib.
  if (!pref_locale.empty())
    candidates.push_back(pref_locale);
#endif  // defined(OS_WIN)

  std::vector<std::string>::const_iterator i = candidates.begin();
  for (; i != candidates.end(); ++i) {
    if (CheckAndResolveLocale(*i, &resolved_locale)) {
      return resolved_locale;
    }
  }

  // Fallback on en-US.
  const std::string fallback_locale("en-US");
  if (HasStringsForLocale(fallback_locale))
    return fallback_locale;

  return std::string();
}
#endif  // !defined(OS_APPLE)

std::string GetApplicationLocaleInternal(const std::string& pref_locale) {
#if defined(OS_APPLE)
  return GetApplicationLocaleInternalMac(pref_locale);
#else
  return GetApplicationLocaleInternalNonMac(pref_locale);
#endif
}

std::string GetApplicationLocale(const std::string& pref_locale,
                                 bool set_icu_locale) {
  const std::string locale = GetApplicationLocaleInternal(pref_locale);
  if (set_icu_locale && !locale.empty())
    base::i18n::SetICUDefaultLocale(locale);
  return locale;
}

std::string GetApplicationLocale(const std::string& pref_locale) {
  return GetApplicationLocale(pref_locale, true /* set_icu_locale */);
}

bool IsLocaleNameTranslated(const char* locale,
                            const std::string& display_locale) {
  std::u16string display_name =
      l10n_util::GetDisplayNameForLocale(locale, display_locale, false);
  // Because ICU sets the error code to U_USING_DEFAULT_WARNING whether or not
  // uloc_getDisplayName returns the actual translation or the default
  // value (locale code), we have to rely on this hack to tell whether
  // the translation is available or not.  If ICU doesn't have a translated
  // name for this locale, GetDisplayNameForLocale will just return the
  // locale code.
  return !base::IsStringASCII(display_name) ||
      base::UTF16ToASCII(display_name) != locale;
}

std::u16string GetDisplayNameForLocale(const std::string& locale,
                                       const std::string& display_locale,
                                       bool is_for_ui,
                                       bool disallow_default) {
  std::string locale_code = locale;
  // Internally, we use the language code of zh-CN and zh-TW, but we want the
  // display names to be Chinese (Simplified) and Chinese (Traditional) instead
  // of Chinese (China) and Chinese (Taiwan).
  // Translate uses "tl" (Tagalog) to mean "fil" (Filipino) until Google
  // translate is changed to understand "fil". Make "tl" alias to "fil".
  if (locale_code == "zh-CN")
    locale_code = "zh-Hans";
  else if (locale_code == "zh-TW")
    locale_code = "zh-Hant";
  else if (locale_code == "tl")
    locale_code = "fil";
  else if (locale_code == "mo")
    locale_code = "ro-MD";

  std::u16string display_name;

#if defined(ENABLE_PSEUDOLOCALES)
  if (locale_code == "en-XA") {
    return u"Long strings pseudolocale (en-XA)";
  } else if (locale_code == "ar-XB") {
    return u"RTL pseudolocale (ar-XB)";
  }
#endif  // defined(ENABLE_PSEUDOLOCALES)

#if defined(OS_IOS)
  // Use the Foundation API to get the localized display name, removing the need
  // for the ICU data file to include this data.
  display_name = GetDisplayNameForLocale(locale_code, display_locale);
#else
#if defined(OS_ANDROID)
  // Use Java API to get locale display name so that we can remove most of
  // the lang data from icu data to reduce binary size, except for zh-Hans and
  // zh-Hant because the current Android Java API doesn't support scripts.
  // TODO(wangxianzhu): remove the special handling of zh-Hans and zh-Hant once
  // Android Java API supports scripts.
  if (!base::StartsWith(locale_code, "zh-Han", base::CompareCase::SENSITIVE)) {
    display_name = GetDisplayNameForLocale(locale_code, display_locale);
  } else
#endif  // defined(OS_ANDROID)
  {
    UErrorCode error = U_ZERO_ERROR;
    const int kBufferSize = 1024;

    int actual_size;
    // For Country code in ICU64 we need to call uloc_getDisplayCountry
    if (locale_code[0] == '-' || locale_code[0] == '_') {
      actual_size = uloc_getDisplayCountry(
          locale_code.c_str(), display_locale.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    } else {
      actual_size = uloc_getDisplayName(
          locale_code.c_str(), display_locale.c_str(),
          base::WriteInto(&display_name, kBufferSize), kBufferSize - 1, &error);
    }
    if (disallow_default && U_USING_DEFAULT_WARNING == error)
      return std::u16string();
    DCHECK(U_SUCCESS(error));
    display_name.resize(actual_size);
  }
#endif  // defined(OS_IOS)

  // Add directional markup so parentheses are properly placed.
  if (is_for_ui && base::i18n::IsRTL())
    base::i18n::AdjustStringForLocaleDirection(&display_name);
  return display_name;
}

std::u16string GetDisplayNameForCountry(const std::string& country_code,
                                        const std::string& display_locale) {
  return GetDisplayNameForLocale("_" + country_code, display_locale, false);
}

std::string NormalizeLocale(const std::string& locale) {
  std::string normalized_locale(locale);
  std::replace(normalized_locale.begin(), normalized_locale.end(), '-', '_');

  return normalized_locale;
}

void GetParentLocales(const std::string& current_locale,
                      std::vector<std::string>* parent_locales) {
  std::string locale(NormalizeLocale(current_locale));

  const int kNameCapacity = 256;
  char parent[kNameCapacity];
  base::strlcpy(parent, locale.c_str(), kNameCapacity);
  parent_locales->push_back(parent);
  UErrorCode err = U_ZERO_ERROR;
  while (uloc_getParent(parent, parent, kNameCapacity, &err) > 0) {
    if (U_FAILURE(err))
      break;
    parent_locales->push_back(parent);
  }
}

bool IsValidLocaleSyntax(const std::string& locale) {
  // Check that the length is plausible.
  if (locale.size() < 2 || locale.size() >= ULOC_FULLNAME_CAPACITY)
    return false;

  // Strip off the part after an '@' sign, which might contain keywords,
  // as in en_IE@currency=IEP or fr@collation=phonebook;calendar=islamic-civil.
  // We don't validate that part much, just check that there's at least one
  // equals sign in a plausible place. Normalize the prefix so that hyphens
  // are changed to underscores.
  std::string prefix = NormalizeLocale(locale);
  size_t split_point = locale.find("@");
  if (split_point != std::string::npos) {
    std::string keywords = locale.substr(split_point + 1);
    prefix = locale.substr(0, split_point);

    size_t equals_loc = keywords.find("=");
    if (equals_loc == 0 || equals_loc == std::string::npos ||
        equals_loc > keywords.size() - 2) {
      return false;
    }
  }

  // Check that all characters before the at-sign are alphanumeric or
  // underscore.
  for (char ch : prefix) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '_')
      return false;
  }

  // Check that the initial token (before the first hyphen/underscore)
  // is 1 - 3 alphabetical characters (a language tag).
  for (size_t i = 0; i < prefix.size(); i++) {
    char ch = prefix[i];
    if (ch == '_') {
      if (i < 1 || i > 3)
        return false;
      break;
    }
    if (!base::IsAsciiAlpha(ch))
      return false;
  }

  // Check that the all tokens after the initial token are 1 - 8 characters.
  // (Tokenize/StringTokenizer don't work here, they collapse multiple
  // delimiters into one.)
  int token_len = 0;
  int token_index = 0;
  for (char ch : prefix) {
    if (ch != '_') {
      token_len++;
      continue;
    }

    if (token_index > 0 && (token_len < 1 || token_len > 8)) {
      return false;
    }
    token_index++;
    token_len = 0;
  }
  if (token_index == 0 && (token_len < 1 || token_len > 3))
    return false;
  if (token_len < 1 || token_len > 8)
    return false;

  return true;
}

std::string GetStringUTF8(int message_id) {
  return base::UTF16ToUTF8(GetStringUTF16(message_id));
}

std::u16string GetStringUTF16(int message_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  std::u16string str = rb.GetLocalizedString(message_id);
  AdjustParagraphDirectionality(&str);

  return str;
}

std::u16string FormatString(const std::u16string& format_string,
                            const std::vector<std::u16string>& replacements,
                            std::vector<size_t>* offsets) {
#if DCHECK_IS_ON()
  // Make sure every replacement string is being used, so we don't just
  // silently fail to insert one. If |offsets| is non-NULL, then don't do this
  // check as the code may simply want to find the placeholders rather than
  // actually replacing them.
  if (!offsets) {
    // $9 is the highest allowed placeholder.
    for (size_t i = 0; i < 9; ++i) {
      bool placeholder_should_exist = replacements.size() > i;

      std::u16string placeholder = u"$";
      placeholder += (L'1' + i);
      size_t pos = format_string.find(placeholder);
      if (placeholder_should_exist) {
        DCHECK_NE(std::string::npos, pos) << " Didn't find a " << placeholder
                                          << " placeholder in "
                                          << format_string;
      } else {
        DCHECK_EQ(std::string::npos, pos) << " Unexpectedly found a "
                                          << placeholder << " placeholder in "
                                          << format_string;
      }
    }
  }
#endif

  std::u16string formatted =
      base::ReplaceStringPlaceholders(format_string, replacements, offsets);
  AdjustParagraphDirectionality(&formatted);

  return formatted;
}

std::u16string GetStringFUTF16(int message_id,
                               const std::vector<std::u16string>& replacements,
                               std::vector<size_t>* offsets) {
  // TODO(tc): We could save a string copy if we got the raw string as
  // a StringPiece and were able to call ReplaceStringPlaceholders with
  // a StringPiece format string and std::u16string substitution strings.  In
  // practice, the strings should be relatively short.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const std::u16string& format_string = rb.GetLocalizedString(message_id);
  return FormatString(format_string, replacements, offsets);
}

std::string GetStringFUTF8(int message_id, const std::u16string& a) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c));
}

std::string GetStringFUTF8(int message_id,
                           const std::u16string& a,
                           const std::u16string& b,
                           const std::u16string& c,
                           const std::u16string& d) {
  return base::UTF16ToUTF8(GetStringFUTF16(message_id, a, b, c, d));
}

std::u16string GetStringFUTF16(int message_id, const std::u16string& a) {
  std::vector<std::u16string> replacements = {a};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b) {
  return GetStringFUTF16(message_id, a, b, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c) {
  std::vector<std::u16string> replacements = {a, b, c};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d) {
  std::vector<std::u16string> replacements = {a, b, c, d};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               const std::u16string& c,
                               const std::u16string& d,
                               const std::u16string& e) {
  std::vector<std::u16string> replacements = {a, b, c, d, e};
  return GetStringFUTF16(message_id, replacements, nullptr);
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               size_t* offset) {
  DCHECK(offset);
  std::vector<size_t> offsets;
  std::vector<std::u16string> replacements = {a};
  std::u16string result = GetStringFUTF16(message_id, replacements, &offsets);
  DCHECK_EQ(1u, offsets.size());
  *offset = offsets[0];
  return result;
}

std::u16string GetStringFUTF16(int message_id,
                               const std::u16string& a,
                               const std::u16string& b,
                               std::vector<size_t>* offsets) {
  std::vector<std::u16string> replacements = {a, b};
  return GetStringFUTF16(message_id, replacements, offsets);
}

std::u16string GetStringFUTF16Int(int message_id, int a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

std::u16string GetStringFUTF16Int(int message_id, int64_t a) {
  return GetStringFUTF16(message_id, base::FormatNumber(a));
}

std::u16string GetPluralStringFUTF16(int message_id, int number) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), number);
}

std::string GetPluralStringFUTF8(int message_id, int number) {
  return base::UTF16ToUTF8(GetPluralStringFUTF16(message_id, number));
}

std::u16string GetSingleOrMultipleStringUTF16(int message_id,
                                              bool is_multiple) {
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      GetStringUTF16(message_id), is_multiple ? "multiple" : "single");
}

void SortStrings16(const std::string& locale,
                   std::vector<std::u16string>* strings) {
  SortVectorWithStringKey(locale, strings, false);
}

const std::vector<std::string>& GetAvailableICULocales() {
  return g_available_locales.Get();
}

const std::vector<std::string>& GetLocalesWithStrings() {
  static base::NoDestructor<std::vector<std::string>> available_locales([] {
    std::vector<std::string> locales;
    for (const char* accept_language : kAcceptLanguageList) {
      std::string locale(accept_language);
      std::string resolved_locale;

      // As there are many callers of GetLocalesWithStrings from threads where
      // I/O is prohibited, we cannot perform I/O here.
      if (!l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                            /*perform_io=*/false))
        continue;

      // We shouldn't show the user any other Chinese locales other than the
      // ones that we have strings for (i.e. when resolved_locale == locale).
      if (resolved_locale != locale &&
          base::LowerCaseEqualsASCII(l10n_util::GetLanguage(locale), "zh"))
        continue;

      locales.push_back(locale);
    }
    return locales;
  }());

  return *available_locales;
}

void GetAcceptLanguagesForLocale(const std::string& display_locale,
                                 std::vector<std::string>* locale_codes) {
  for (const char* accept_language : kAcceptLanguageList) {
    if (!l10n_util::IsLocaleNameTranslated(accept_language, display_locale)) {
      // TODO(jungshik) : Put them at the end of the list with language codes
      // enclosed by brackets instead of skipping.
      continue;
    }
    locale_codes->push_back(accept_language);
  }
}

void GetAcceptLanguages(std::vector<std::string>* locale_codes) {
  for (const char* accept_language : kAcceptLanguageList) {
    locale_codes->push_back(accept_language);
  }
}

bool IsLanguageAccepted(const std::string& display_locale,
                        const std::string& locale) {
  for (const char* accept_language : kAcceptLanguageList) {
    if (accept_language == locale &&
        l10n_util::IsLocaleNameTranslated(locale.c_str(), display_locale)) {
      return true;
    }
  }
  return false;
}

int GetLocalizedContentsWidthInPixels(int pixel_resource_id) {
  int width = 0;
  base::StringToInt(l10n_util::GetStringUTF8(pixel_resource_id), &width);
  DCHECK_GT(width, 0);
  return width;
}

const char* const* GetAcceptLanguageListForTesting() {
  return kAcceptLanguageList;
}

size_t GetAcceptLanguageListSizeForTesting() {
  return base::size(kAcceptLanguageList);
}

const char* const* GetPlatformLocalesForTesting() {
  return kPlatformLocales;
}

size_t GetPlatformLocalesSizeForTesting() {
  return base::size(kPlatformLocales);
}

}  // namespace l10n_util
