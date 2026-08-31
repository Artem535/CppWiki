import type { BlockNoteEditor, PropSchema } from "@blocknote/core";
import { insertOrUpdateBlock } from "@blocknote/core";
import type { DefaultReactSuggestionItem } from "@blocknote/react";
import { createReactBlockSpec, type ReactCustomBlockRenderProps } from "@blocknote/react";
import { useRef, useState } from "react";

import { getAttachmentBridge } from "./attachmentBridgeContext";
import { uploadAttachment } from "./attachmentUpload";

export const attachmentBlockType = "attachment";

const attachmentPropSchema = {
  attachmentId: { default: "" },
  uri: { default: "" },
  filename: { default: "" },
  mimeType: { default: "application/octet-stream" },
  sizeBytes: { default: 0 },
} satisfies PropSchema;

function formatFileSize(sizeBytes: number): string {
  if (sizeBytes < 1024) {
    return `${sizeBytes} B`;
  }
  if (sizeBytes < 1024 * 1024) {
    return `${Math.ceil(sizeBytes / 1024)} KiB`;
  }
  return `${(sizeBytes / (1024 * 1024)).toFixed(1)} MiB`;
}

function AttachmentIcon() {
  return (
    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <path
        d="M8 12.5l6.6-6.6a3.5 3.5 0 115 5L11.1 20.4a5 5 0 11-7.1-7.1l8.1-8.1"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
      />
    </svg>
  );
}

function AttachmentBlockContent(
  props: ReactCustomBlockRenderProps<
    typeof attachmentBlockType,
    typeof attachmentPropSchema,
    "none"
  >,
) {
  const inputRef = useRef<HTMLInputElement>(null);
  const [isUploading, setIsUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const attachmentId = props.block.props.attachmentId;
  const isImage = props.block.props.mimeType.startsWith("image/");

  const chooseFile = () => {
    inputRef.current?.click();
  };

  const upload = async (file: File) => {
    const bridge = getAttachmentBridge();
    if (bridge === null) {
      setError("Attachments are only available in the desktop app.");
      return;
    }

    setError(null);
    setIsUploading(true);
    try {
      const result = await uploadAttachment(bridge, file);
      if (!result.ok) {
        setError(result.error.message);
        return;
      }
      props.editor.updateBlock(props.block, {
        props: {
          attachmentId: result.result.attachmentId,
          uri: result.result.uri,
          filename: file.name,
          mimeType: file.type || "application/octet-stream",
          sizeBytes: file.size,
        },
      });
    } finally {
      setIsUploading(false);
    }
  };

  const saveToFile = async () => {
    const bridge = getAttachmentBridge();
    if (bridge === null || !attachmentId) {
      setError("Attachment is not available.");
      return;
    }
    const result = await bridge.saveAttachmentToFile(attachmentId);
    if (!result.ok && result.error.code !== "cancelled") {
      setError(result.error.message);
    }
  };

  if (!attachmentId) {
    return (
      <div className="attachment-block attachment-block--empty" contentEditable={false}>
        <AttachmentIcon />
        <div className="attachment-block-copy">
          <strong>Attach a file</strong>
          <span>Images and files up to 25 MiB sync with this workspace.</span>
        </div>
        <button type="button" className="attachment-block-action" onClick={chooseFile} disabled={isUploading}>
          {isUploading ? "Uploading…" : "Choose file"}
        </button>
        <input
          ref={inputRef}
          className="attachment-block-file-input"
          type="file"
          onChange={(event) => {
            const file = event.target.files?.[0];
            if (file) {
              void upload(file);
            }
            event.target.value = "";
          }}
        />
        {error ? <p className="attachment-block-error">{error}</p> : null}
      </div>
    );
  }

  return (
    <div className="attachment-block" contentEditable={false}>
      {isImage ? (
        <img className="attachment-block-image" src={props.block.props.uri} alt={props.block.props.filename} />
      ) : (
        <AttachmentIcon />
      )}
      <div className="attachment-block-copy">
        <strong>{props.block.props.filename}</strong>
        <span>{formatFileSize(props.block.props.sizeBytes)} · {props.block.props.mimeType}</span>
      </div>
      <button type="button" className="attachment-block-action" onClick={() => void saveToFile()}>
        Save as…
      </button>
      {error ? <p className="attachment-block-error">{error}</p> : null}
    </div>
  );
}

export const AttachmentBlock = createReactBlockSpec(
  {
    type: attachmentBlockType,
    propSchema: attachmentPropSchema,
    content: "none",
  } as const,
  { render: AttachmentBlockContent },
);

export function getAttachmentSlashMenuItem(
  // eslint-disable-next-line @typescript-eslint/no-explicit-any -- BlockNote block schemas are recursive.
  editor: BlockNoteEditor<any, any, any>,
): DefaultReactSuggestionItem {
  return {
    title: "File attachment",
    subtext: "Attach a synchronized image or file (up to 25 MiB)",
    aliases: ["attachment", "file", "image", "upload"],
    group: "Media",
    icon: <AttachmentIcon />,
    onItemClick: () => {
      insertOrUpdateBlock(editor, { type: attachmentBlockType });
    },
  };
}
