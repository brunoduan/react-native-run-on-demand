#include "JSIndexedRAMBundleString.h"

#include <folly/Memory.h>

namespace facebook {
namespace react {

std::function<std::unique_ptr<JSModulesUnbundle>(std::string)> JSIndexedRAMBundleString::buildFactory() {
  return [](const std::string& bundleScript){
    return folly::make_unique<JSIndexedRAMBundleString>(bundleScript);
  };
}

JSIndexedRAMBundleString::JSIndexedRAMBundleString(const std::string str) :
    m_bundle(str) {
  init();
}

JSIndexedRAMBundleString::JSIndexedRAMBundleString(const char *sourcePath, size_t length) :
    m_bundle(std::string(sourcePath, length)) {
  init();
}

void JSIndexedRAMBundleString::init() {
  if (!m_bundle) {
    throw std::ios_base::failure(
      folly::to<std::string>("Failed to construct JSIndexedRAMBundleString"));
  }

  // read in magic header, minimal module id, number of entries, and length of the startup section
  uint32_t header[4];
  static_assert(
    sizeof(header) == 16,
    "header size must exactly match the input file format");

  readBundle(reinterpret_cast<char *>(header), sizeof(header));
  const size_t numTableEntries = folly::Endian::little(header[2]);
  const size_t startupCodeSize = folly::Endian::little(header[3]);
  // allocate memory for meta data and lookup table.
  m_minModuleId = folly::Endian::little(header[1]);
  m_maxModuleId = m_minModuleId + numTableEntries;
  m_table = ModuleTable(numTableEntries);
  m_baseOffset = sizeof(header) + m_table.byteLength();

  // read the lookup table from the file
  readBundle(
    reinterpret_cast<char *>(m_table.data.get()), m_table.byteLength());

  // read the startup code
  m_startupCode = std::unique_ptr<JSBigBufferString>(new JSBigBufferString{startupCodeSize - 1});
  readBundle(m_startupCode->data(), startupCodeSize - 1);
 }

JSIndexedRAMBundleString::Module JSIndexedRAMBundleString::getModule(uint32_t moduleId) const {
  Module ret;
  ret.name = folly::to<std::string>(moduleId, ".js");
  ret.code = getModuleCode(moduleId);

  return ret;
}

bool JSIndexedRAMBundleString::exists(uint32_t moduleId) const {
  return moduleId >= m_minModuleId && moduleId <= m_maxModuleId;
}

std::unique_ptr<const JSBigString> JSIndexedRAMBundleString::getStartupCode() {
  return std::move(m_startupCode);
}

std::string JSIndexedRAMBundleString::getModuleCode(const uint32_t moduleId) const {
  int32_t id = moduleId - m_minModuleId;
  const auto moduleData = (id >= 0 && id < m_table.numEntries) ? &m_table.data[id] : nullptr;

  // entries without associated code have offset = 0 and length = 0
  const uint32_t length = moduleData ? folly::Endian::little(moduleData->length) : 0;
  if (length == 0) {
    throw std::ios_base::failure(
      folly::to<std::string>("Error loading module", id, "from RAM Bundle"));
  }

  std::string ret(length - 1, '\0');
  readBundle(&ret.front(), length - 1, m_baseOffset + folly::Endian::little(moduleData->offset));
  return ret;
}

void JSIndexedRAMBundleString::readBundle(char *buffer, const std::streamsize bytes) const {
  if (!m_bundle.read(buffer, bytes)) {
    if (m_bundle.rdstate() & std::ios::eofbit) {
      throw std::ios_base::failure("Unexpected end of RAM Bundle file");
    }
    throw std::ios_base::failure(
      folly::to<std::string>("Error reading RAM Bundle: ", m_bundle.rdstate()));
  }
}

void JSIndexedRAMBundleString::readBundle(
    char *buffer,
    const std::streamsize bytes,
    const std::istream::pos_type position) const {

  if (!m_bundle.seekg(position)) {
    throw std::ios_base::failure(
      folly::to<std::string>("Error reading RAM Bundle: ", m_bundle.rdstate()));
  }
  readBundle(buffer, bytes);
}

}  // namespace react
}  // namespace facebook
