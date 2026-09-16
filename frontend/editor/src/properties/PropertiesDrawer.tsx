import { useEffect, useMemo, useState } from "react";
import type { EditorBridge, PagePropertyValue, PropertyDefinition, PropertyValueKind } from "../bridge/editorBridge";
import { colorForTagValue } from "./propertyModel";

type PropertiesDrawerProps = {
  bridge: EditorBridge;
  workspaceId: string;
  pageId: string;
  onChange?: () => void;
  editable: boolean;
  open: boolean;
  onClose: () => void;
};

const valueKinds: Array<{ value: PropertyValueKind; label: string }> = [
  { value: "text", label: "Text" },
  { value: "number", label: "Number" },
  { value: "date", label: "Date" },
  { value: "checkbox", label: "Checkbox" },
  { value: "select", label: "Select" },
  { value: "multiSelect", label: "Multi-select" },
  { value: "tags", label: "Tags" },
  { value: "relation", label: "Relation" },
];

const optionKinds: PropertyValueKind[] = ["select", "multiSelect"];
const multiValueKinds: PropertyValueKind[] = ["multiSelect", "tags", "relation"];

function valuesToText(value: PagePropertyValue | undefined): string {
  return value?.values.join(", ") ?? "";
}

// One small line icon per relation kind, muted to the same color as the label so the row reads
// as "icon + label" first and "value" second -- the same visual hierarchy the drawer's row
// layout depends on (see .property-row__label in styles.css).
function PropertyKindIcon({ kind }: { kind: PropertyValueKind }) {
  const common = { width: 14, height: 14, viewBox: "0 0 14 14", fill: "none", "aria-hidden": true } as const;
  switch (kind) {
    case "checkbox":
      return (
        <svg {...common}>
          <rect x="1.5" y="1.5" width="11" height="11" rx="2.5" stroke="currentColor" strokeWidth="1.3" />
          <path d="M4 7.2l1.8 1.8L10 5" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round" strokeLinejoin="round" />
        </svg>
      );
    case "date":
      return (
        <svg {...common}>
          <rect x="1.5" y="2.5" width="11" height="10" rx="1.5" stroke="currentColor" strokeWidth="1.3" />
          <path d="M1.5 5.5h11M4 1.2v2M10 1.2v2" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round" />
        </svg>
      );
    case "number":
      return (
        <svg {...common}>
          <path
            d="M5 1.5L3.6 12.5M10.4 1.5L9 12.5M2 5h10M1.6 9h10"
            stroke="currentColor"
            strokeWidth="1.2"
            strokeLinecap="round"
          />
        </svg>
      );
    case "select":
      return (
        <svg {...common}>
          <circle cx="7" cy="7" r="5.2" stroke="currentColor" strokeWidth="1.3" />
          <circle cx="7" cy="7" r="2" fill="currentColor" />
        </svg>
      );
    case "multiSelect":
      return (
        <svg {...common}>
          <circle cx="5" cy="5.5" r="3" stroke="currentColor" strokeWidth="1.2" />
          <circle cx="9.5" cy="9" r="3" stroke="currentColor" strokeWidth="1.2" />
        </svg>
      );
    case "tags":
      return (
        <svg {...common}>
          <path
            d="M2 2h4.6L12 7.4a1.4 1.4 0 0 1 0 2L8.4 13a1.4 1.4 0 0 1-2 0L2 8.6V2z"
            stroke="currentColor"
            strokeWidth="1.2"
            strokeLinejoin="round"
          />
          <circle cx="4.6" cy="4.6" r="1" fill="currentColor" />
        </svg>
      );
    case "relation":
      return (
        <svg {...common}>
          <path
            d="M5.6 8.4L8.4 5.6M6 3.2l.7-.7a2.5 2.5 0 0 1 3.5 3.5l-.7.7M8 10.8l-.7.7a2.5 2.5 0 0 1-3.5-3.5l.7-.7"
            stroke="currentColor"
            strokeWidth="1.3"
            strokeLinecap="round"
          />
        </svg>
      );
    case "text":
    default:
      return (
        <svg {...common}>
          <path d="M2 3.5h10M2 7h7M2 10.5h9" stroke="currentColor" strokeWidth="1.3" strokeLinecap="round" />
        </svg>
      );
  }
}

function TagPill({ label, onRemove, editable }: { label: string; onRemove?: () => void; editable?: boolean }) {
  const color = colorForTagValue(label);
  return (
    <span className={`tag-pill tag-pill--${color}`}>
      <span className="tag-pill__label">{label}</span>
      {onRemove ? (
        <button
          type="button"
          className="tag-pill__remove"
          onClick={onRemove}
          disabled={!editable}
          aria-label={`Remove ${label}`}
        >
          ×
        </button>
      ) : null}
    </span>
  );
}

// One property row's value cell. Owns its own "click to edit" state so the parent drawer doesn't
// need to track which of N rows is currently being edited -- each cell is a self-contained unit
// that renders itself as plain text/pills until the person clicks in, then becomes an input.
function PropertyValueCell({
  definition,
  existing,
  editable,
  onSave,
}: {
  definition: PropertyDefinition;
  existing: PagePropertyValue | undefined;
  editable: boolean;
  onSave: (nextValues: string[]) => void;
}) {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState("");
  const values = existing?.values ?? [];

  if (definition.valueKind === "checkbox") {
    const checked = values[0] === "true";
    return (
      <label className="property-checkbox">
        <input
          type="checkbox"
          checked={checked}
          disabled={!editable}
          onChange={(event) => onSave([event.currentTarget.checked ? "true" : "false"])}
          aria-label={definition.name}
        />
        <span className="property-checkbox__box" aria-hidden="true" />
      </label>
    );
  }

  if (definition.valueKind === "select") {
    return (
      <span className="property-value property-value--overlaySelect">
        {values[0] ? <TagPill label={values[0]} /> : <span className="property-value__empty">Empty</span>}
        <select
          className="property-value__overlay"
          value={values[0] ?? ""}
          disabled={!editable}
          onChange={(event) => onSave(event.currentTarget.value ? [event.currentTarget.value] : [])}
          aria-label={definition.name}
        >
          <option value="">Empty</option>
          {definition.options.map((option) => (
            <option value={option} key={option}>
              {option}
            </option>
          ))}
        </select>
      </span>
    );
  }

  if (multiValueKinds.includes(definition.valueKind) && definition.valueKind !== "relation") {
    const addFromDraft = () => {
      const additions = draft.split(",").map((item) => item.trim()).filter(Boolean);
      if (additions.length > 0) onSave([...values, ...additions]);
      setDraft("");
      setEditing(false);
    };
    return (
      <span className="property-value property-value--pills">
        {values.map((one) => (
          <TagPill
            key={one}
            label={one}
            editable={editable}
            onRemove={() => onSave(values.filter((existingValue) => existingValue !== one))}
          />
        ))}
        {editable && editing ? (
          <input
            autoFocus
            className="property-value__pill-input"
            value={draft}
            placeholder={definition.valueKind === "tags" ? "Add tag" : "Add option"}
            onChange={(event) => setDraft(event.currentTarget.value)}
            onBlur={addFromDraft}
            onKeyDown={(event) => {
              if (event.key === "Enter") addFromDraft();
              if (event.key === "Escape") {
                setDraft("");
                setEditing(false);
              }
            }}
          />
        ) : editable ? (
          <button type="button" className="property-value__pill-add" onClick={() => setEditing(true)}>
            +
          </button>
        ) : values.length === 0 ? (
          <span className="property-value__empty">Empty</span>
        ) : null}
      </span>
    );
  }

  // text, number, date, relation: plain scalar rendered as text until clicked into.
  if (!editing) {
    return (
      <button
        type="button"
        className="property-value property-value--text"
        disabled={!editable}
        onClick={() => {
          setDraft(valuesToText(existing));
          setEditing(true);
        }}
      >
        {values.length > 0 ? valuesToText(existing) : <span className="property-value__empty">Empty</span>}
      </button>
    );
  }
  return (
    <input
      autoFocus
      className="property-value__input"
      type={definition.valueKind === "number" ? "number" : definition.valueKind === "date" ? "date" : "text"}
      value={draft}
      disabled={!editable}
      onChange={(event) => setDraft(event.currentTarget.value)}
      onBlur={() => {
        setEditing(false);
        const next = draft.split(",").map((item) => item.trim()).filter(Boolean);
        if (next.length > 0) onSave(next);
      }}
      onKeyDown={(event) => {
        if (event.key === "Enter") event.currentTarget.blur();
        if (event.key === "Escape") setEditing(false);
      }}
      aria-label={definition.name}
    />
  );
}

export function PropertiesDrawer({
  bridge,
  workspaceId,
  pageId,
  onChange,
  editable,
  open,
  onClose,
}: PropertiesDrawerProps) {
  const [definitions, setDefinitions] = useState<PropertyDefinition[]>([]);
  const [values, setValues] = useState<PagePropertyValue[]>([]);
  const [manageOpen, setManageOpen] = useState(false);
  const [creating, setCreating] = useState(false);
  const [draftName, setDraftName] = useState("");
  const [draftKind, setDraftKind] = useState<PropertyValueKind>("text");
  const [draftOptions, setDraftOptions] = useState("");
  const [draftGroup, setDraftGroup] = useState("");
  const [error, setError] = useState<string | null>(null);

  const reload = async () => {
    const [definitionResponse, valueResponse] = await Promise.all([
      bridge.listPropertyDefinitions(workspaceId),
      bridge.listPagePropertyValues(workspaceId, pageId),
    ]);
    if (!definitionResponse.ok) {
      setError(definitionResponse.error.message);
      return;
    }
    if (!valueResponse.ok) {
      setError(valueResponse.error.message);
      return;
    }
    setDefinitions(definitionResponse.result);
    setValues(valueResponse.result);
    setError(null);
  };

  useEffect(() => {
    if (open) void reload();
  }, [bridge, workspaceId, pageId, open]);

  const activeDefinitions = useMemo(
    () => definitions.filter((definition) => definition.state === "active"),
    [definitions],
  );

  const saveValue = async (definition: PropertyDefinition, nextValues: string[], existing?: PagePropertyValue) => {
    if (nextValues.length === 0 && existing) {
      await removeValue(existing);
      return;
    }
    if (nextValues.length === 0) return;
    const response = await bridge.savePagePropertyValue({
      id: existing?.id,
      workspaceId,
      pageId,
      propertyDefinitionId: definition.id,
      values: nextValues,
    });
    if (!response.ok) {
      setError(response.error.message);
      return;
    }
    setValues((previous) => {
      const index = previous.findIndex((value) => value.id === response.result.id);
      return index < 0
        ? [...previous, response.result]
        : previous.map((value, valueIndex) => (valueIndex === index ? response.result : value));
    });
    onChange?.();
    setError(null);
  };

  const removeValue = async (value: PagePropertyValue) => {
    const response = await bridge.deletePagePropertyValue(value.id);
    if (!response.ok) {
      setError(response.error.message);
      return;
    }
    setValues((previous) => previous.filter((item) => item.id !== value.id));
    onChange?.();
  };

  const createDefinition = async () => {
    if (!draftName.trim()) return;
    const response = await bridge.savePropertyDefinition({
      workspaceId,
      name: draftName.trim(),
      groupName: draftGroup.trim() || null,
      valueKind: draftKind,
      options: draftOptions.split(",").map((item) => item.trim()).filter(Boolean),
      state: "active",
    });
    if (!response.ok) {
      setError(response.error.message);
      return;
    }
    setDefinitions((previous) => [...previous.filter((item) => item.id !== response.result.id), response.result]);
    onChange?.();
    setDraftName("");
    setDraftOptions("");
    setDraftGroup("");
    setCreating(false);
    setError(null);
  };

  const renameDefinition = async (definition: PropertyDefinition, name: string, groupName: string) => {
    const response = await bridge.savePropertyDefinition({ ...definition, name: name.trim(), groupName: groupName.trim() || null });
    if (!response.ok) {
      setError(response.error.message);
      return;
    }
    setDefinitions((previous) => previous.map((item) => (item.id === response.result.id ? response.result : item)));
    onChange?.();
  };

  const retireDefinition = async (definition: PropertyDefinition) => {
    const response = await bridge.retirePropertyDefinition(definition.id);
    if (!response.ok) {
      setError(response.error.message);
      return;
    }
    setDefinitions((previous) => previous.map((item) => (item.id === response.result.id ? response.result : item)));
    onChange?.();
  };

  if (!open) return null;

  return (
    <>
      <button className="properties-drawer-backdrop" aria-label="Close properties" onClick={onClose} />
      <aside className="properties-drawer" aria-label="Page properties" data-testid="properties-drawer">
        <header className="properties-drawer__header">
          <h2>Properties</h2>
          <button className="properties-drawer__close" onClick={onClose} aria-label="Close properties">×</button>
        </header>
        {error ? <p className="properties-drawer__error" role="alert">{error}</p> : null}
        <section className="properties-drawer__list">
          {activeDefinitions.map((definition) => {
            const existing = values.find((value) => value.propertyDefinitionId === definition.id);
            return (
              <div className="property-row" key={definition.id}>
                <span className="property-row__label">
                  <span className="property-row__icon"><PropertyKindIcon kind={definition.valueKind} /></span>
                  <span className="property-row__name">{definition.name}</span>
                </span>
                <PropertyValueCell
                  definition={definition}
                  existing={existing}
                  editable={editable}
                  onSave={(nextValues) => void saveValue(definition, nextValues, existing)}
                />
                {existing ? (
                  <button
                    className="property-row__remove"
                    disabled={!editable}
                    onClick={() => void removeValue(existing)}
                    aria-label={`Remove ${definition.name}`}
                  >
                    ×
                  </button>
                ) : (
                  <span className="property-row__remove property-row__remove--spacer" aria-hidden="true" />
                )}
              </div>
            );
          })}
          {activeDefinitions.length === 0 ? <p className="properties-drawer__empty">No properties yet.</p> : null}
          {editable ? (
            creating ? (
              <div className="property-create">
                <input
                  autoFocus
                  value={draftName}
                  onChange={(event) => setDraftName(event.target.value)}
                  placeholder="Property name"
                  onKeyDown={(event) => event.key === "Enter" && void createDefinition()}
                />
                <select value={draftKind} onChange={(event) => setDraftKind(event.target.value as PropertyValueKind)}>
                  {valueKinds.map((kind) => (
                    <option value={kind.value} key={kind.value}>
                      {kind.label}
                    </option>
                  ))}
                </select>
                {optionKinds.includes(draftKind) ? (
                  <input
                    value={draftOptions}
                    onChange={(event) => setDraftOptions(event.target.value)}
                    placeholder="Options, comma-separated"
                  />
                ) : null}
                <div className="property-create__actions">
                  <button type="button" onClick={() => void createDefinition()} disabled={!draftName.trim()}>
                    Add
                  </button>
                  <button type="button" className="property-create__cancel" onClick={() => setCreating(false)}>
                    Cancel
                  </button>
                </div>
              </div>
            ) : (
              <button type="button" className="property-row property-row--ghost" onClick={() => setCreating(true)}>
                <span className="property-row__label">
                  <span className="property-row__icon property-row__icon--add">+</span>
                  <span className="property-row__name">New property</span>
                </span>
              </button>
            )
          ) : null}
        </section>
        <section className="properties-drawer__manage">
          <button className="properties-drawer__manage-toggle" onClick={() => setManageOpen((value) => !value)} type="button">
            <span>Manage properties</span>
            <span>{manageOpen ? "−" : "+"}</span>
          </button>
          {manageOpen ? (
            <div className="properties-catalog">
              {definitions.map((definition) => (
                <div className="catalog-row" key={definition.id}>
                  <input
                    defaultValue={definition.name}
                    disabled={!editable || definition.state === "retired"}
                    aria-label={`Name for ${definition.name}`}
                    onBlur={(event) => void renameDefinition(definition, event.currentTarget.value, definition.groupName ?? "")}
                  />
                  <span className="catalog-row__kind">{definition.valueKind}{definition.groupName ? ` · ${definition.groupName}` : ""}</span>
                  {definition.state === "active" ? (
                    <button disabled={!editable} onClick={() => void retireDefinition(definition)}>
                      Retire
                    </button>
                  ) : (
                    <em>Retired</em>
                  )}
                </div>
              ))}
              {definitions.length === 0 ? <p className="properties-drawer__empty">No properties in this workspace yet.</p> : null}
            </div>
          ) : null}
        </section>
      </aside>
    </>
  );
}
