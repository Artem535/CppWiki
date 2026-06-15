#ifndef CPPWIKI_SRC_DOCUMENT_DOCUMENT_VALIDATOR_H_
#define CPPWIKI_SRC_DOCUMENT_DOCUMENT_VALIDATOR_H_

#include <QByteArray>
#include <cstdint>
#include <optional>
#include <string>

#include "document/block_note_snapshot.h"
#include "document/document.h"

namespace cppwiki::document {

enum class ValidationErrorCode : std::uint8_t {
  kInvalidJson,
  kInvalidRoot,
  kMissingSchemaVersion,
  kMissingPageId,
  kMissingBlockId,
  kEmptyBlockId,
  kDuplicateBlockId,
  kUnsupportedBlockType,
  kInvalidHeadingLevel,
  kInvalidChildren,
};

[[nodiscard]] auto ToString(ValidationErrorCode code) -> std::string;

struct ValidationError {
  ValidationErrorCode code;
  std::string message;
};

class DocumentValidator {
 public:
  struct Result {
    std::optional<Document> document;
    std::optional<BlockNoteDocumentSnapshot> snapshot;
    std::string raw_snapshot_json;
    std::optional<ValidationError> error;
  };

  [[nodiscard]] static Result ParseAndValidateSnapshot(const QByteArray& snapshot_json);
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_VALIDATOR_H_
