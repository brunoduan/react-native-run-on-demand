// Copyright (c) Facebook, Inc. and its affiliates.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include <cxxreact/JSModulesUnbundle.h>

#ifdef XPENG_BUILD_SPLIT_BUNDLE
#include <cxxreact/JSBigString.h>
#endif

#ifndef RN_EXPORT
#define RN_EXPORT __attribute__((visibility("default")))
#endif

namespace facebook {
namespace react {

class RN_EXPORT RAMBundleRegistry {
public:
  constexpr static uint32_t MAIN_BUNDLE_ID = 0;

  static std::unique_ptr<RAMBundleRegistry> singleBundleRegistry(
      std::unique_ptr<JSModulesUnbundle> mainBundle);
  static std::unique_ptr<RAMBundleRegistry> multipleBundlesRegistry(
      std::unique_ptr<JSModulesUnbundle> mainBundle,
      std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> factory);

  explicit RAMBundleRegistry(
      std::unique_ptr<JSModulesUnbundle> mainBundle,
      std::function<
        std::unique_ptr<JSModulesUnbundle>(std::string)> factory = nullptr);

  RAMBundleRegistry(RAMBundleRegistry&&) = default;
  RAMBundleRegistry& operator=(RAMBundleRegistry&&) = default;

  void registerBundle(uint32_t bundleId, std::string bundlePath);
  JSModulesUnbundle::Module getModule(uint32_t bundleId, uint32_t moduleId);
  virtual ~RAMBundleRegistry() {};

#ifdef XPENG_BUILD_SPLIT_BUNDLE
  void registerBundle(const std::string &sourceURL, const std::string &script);
  std::unique_ptr<const JSBigString> getStartupCodeFromStringBundles(
      const std::string &sourcePath);
  bool existsJSModulesUndundle();
#endif

private:
  JSModulesUnbundle* getBundle(uint32_t bundleId) const;

  std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> m_factory;
  std::unordered_map<uint32_t, std::string> m_bundlePaths;
  std::unordered_map<uint32_t, std::unique_ptr<JSModulesUnbundle>> m_bundles;

#ifdef XPENG_BUILD_SPLIT_BUNDLE
  std::unordered_map<std::string, std::unique_ptr<JSModulesUnbundle>> m_fromStringBundles;
#endif

};

}  // namespace react
}  // namespace facebook
