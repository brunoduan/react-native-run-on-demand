// Copyright (c) Facebook, Inc. and its affiliates.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "RAMBundleRegistry.h"

#include <folly/Memory.h>
#include <folly/String.h>

#ifdef XPENG_BUILD_SPLIT_BUNDLE
#include <cxxreact/JSIndexedRAMBundleString.h>
#endif

namespace facebook {
namespace react {

constexpr uint32_t RAMBundleRegistry::MAIN_BUNDLE_ID;

std::unique_ptr<RAMBundleRegistry> RAMBundleRegistry::singleBundleRegistry(
    std::unique_ptr<JSModulesUnbundle> mainBundle) {
  return folly::make_unique<RAMBundleRegistry>(std::move(mainBundle));
}

std::unique_ptr<RAMBundleRegistry> RAMBundleRegistry::multipleBundlesRegistry(
    std::unique_ptr<JSModulesUnbundle> mainBundle,
    std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> factory) {
  return folly::make_unique<RAMBundleRegistry>(
      std::move(mainBundle), std::move(factory));
}

RAMBundleRegistry::RAMBundleRegistry(
    std::unique_ptr<JSModulesUnbundle> mainBundle,
    std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> factory):
      m_factory(std::move(factory)) {
  m_bundles.emplace(MAIN_BUNDLE_ID, std::move(mainBundle));
}

void RAMBundleRegistry::registerBundle(
    uint32_t bundleId, std::string bundlePath) {
  m_bundlePaths.emplace(bundleId, std::move(bundlePath));
}

#ifdef XPENG_BUILD_SPLIT_BUNDLE
bool RAMBundleRegistry::existsJSModulesUndundle() {
  return m_fromStringBundles.size() > 0;
}

void RAMBundleRegistry::registerBundle(
    const std::string &sourcePath,
    const std::string& script) {
  m_fromStringBundles.emplace(sourcePath, m_factory(script));
}

std::unique_ptr<const JSBigString> RAMBundleRegistry::getStartupCodeFromStringBundles(
    const std::string &sourcePath) {
  std::unordered_map<std::string, std::unique_ptr<JSModulesUnbundle>>::iterator it =
    m_fromStringBundles.find(sourcePath);
  if (it != m_fromStringBundles.end()) {
    if (it->second) {
      return it->second->getStartupCode();
    }
  }

  return nullptr;
}
#endif

JSModulesUnbundle::Module RAMBundleRegistry::getModule(
    uint32_t bundleId, uint32_t moduleId) {
  if (m_bundles.find(bundleId) == m_bundles.end()) {
    if (!m_factory) {
      throw std::runtime_error(
        "You need to register factory function in order to "
        "support multiple RAM bundles."
      );
    }

    auto bundlePath = m_bundlePaths.find(bundleId);
    if (bundlePath == m_bundlePaths.end()) {
      throw std::runtime_error(
        "In order to fetch RAM bundle from the registry, its file "
        "path needs to be registered first."
      );
    }
    m_bundles.emplace(bundleId, m_factory(bundlePath->second));
  }

#ifdef XPENG_BUILD_SPLIT_BUNDLE
  std::unordered_map<std::string, std::unique_ptr<JSModulesUnbundle>>::iterator it = m_fromStringBundles.begin();
  for (; it != m_fromStringBundles.end(); it++) {
    if (it->second) {
      JSModulesUnbundle *bundle = it->second.get();
      if (bundle && bundle->exists(moduleId)) {
        return bundleGetModule(bundleId, bundle, moduleId);
      }
    }
  }
#endif
  
  auto module = getBundle(bundleId)->getModule(moduleId);
  if (bundleId == MAIN_BUNDLE_ID) {
    return module;
  }
  return {
    folly::to<std::string>("seg-", bundleId, '_', std::move(module.name)),
    std::move(module.code),
  };
}

#ifdef XPENG_BUILD_SPLIT_BUNDLE
JSModulesUnbundle::Module RAMBundleRegistry::bundleGetModule(
    uint32_t bundleId,
    JSModulesUnbundle *bundle,
    uint32_t moduleId) const {
  auto module = bundle->getModule(moduleId);
  if (bundleId == MAIN_BUNDLE_ID) {
    return module;
  }
  return {
    folly::to<std::string>("seg-", bundleId, '_', std::move(module.name)),
    std::move(module.code),
  };
}
#endif

JSModulesUnbundle* RAMBundleRegistry::getBundle(uint32_t bundleId) const {
  return m_bundles.at(bundleId).get();
}

}  // namespace react
}  // namespace facebook
