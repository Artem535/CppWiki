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

  // For DocumentKind::kWikiPage (the default), validates the snapshot as BlockNote JSON exactly
  // as before. For the other kinds (nbformat/Excalidraw), full schema validation is out of
  // scope here (see #52/#53) — this only checks the payload is well-formed JSON, so callers get
  // a real ValidationError instead of silently accepting garbage, without this validator having
  // to know the nbformat/Excalidraw schemas.
  [[nodiscard]] static Result ParseAndValidateSnapshot(
      const QByteArray& snapshot_json, DocumentKind kind = DocumentKind::kWikiPage);
};

}  // namespace cppwiki::document

#endif  // CPPWIKI_SRC_DOCUMENT_DOCUMENT_VALIDATOR_H_
