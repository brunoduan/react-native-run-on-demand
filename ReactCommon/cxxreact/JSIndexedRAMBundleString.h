#pragma once

#include <iostream>
#include <memory>

#include <cxxreact/JSBigString.h>
#include <cxxreact/JSModulesUnbundle.h>

#ifndef RN_EXPORT
#define RN_EXPORT __attribute__((visibility("default")))
#endif

namespace facebook {
namespace react {

class RN_EXPORT JSIndexedRAMBundleString : public JSModulesUnbundle {
public:
  static std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> buildFactory();

  // Throws std::runtime_error on failure.
  JSIndexedRAMBundleString(const char *sourceURL, size_t length);
  JSIndexedRAMBundleString(const std::string str);

  // Throws std::runtime_error on failure.
  std::unique_ptr<const JSBigString> getStartupCode() override;
  // Throws std::runtime_error on failure.
  Module getModule(uint32_t moduleId) const override;
  bool exists(uint32_t moduleId) const override;

private:
  struct ModuleData {
    uint32_t offset;
    uint32_t length;
  };
  static_assert(
    sizeof(ModuleData) == 8,
    "ModuleData must not have any padding and use sizes matching input files");

  struct ModuleTable {
    size_t numEntries;
    std::unique_ptr<ModuleData[]> data;
    ModuleTable() : numEntries(0) {};
    ModuleTable(size_t entries) :
      numEntries(entries),
      data(std::unique_ptr<ModuleData[]>(new ModuleData[numEntries])) {};
    size_t byteLength() const {
      return numEntries * sizeof(ModuleData);
    }
  };

  void init();
  std::string getModuleCode(const uint32_t id) const;
  void readBundle(char *buffer, const std::streamsize bytes) const;
  void readBundle(
    char *buffer, const
    std::streamsize bytes,
    const std::istream::pos_type position) const;

  mutable std::istringstream m_bundle;
  ModuleTable m_table;
  size_t m_baseOffset;
  std::unique_ptr<JSBigBufferString> m_startupCode;
  uint32_t m_minModuleId;
  uint32_t m_maxModuleId;
};

}  // namespace react
}  // namespace facebook
