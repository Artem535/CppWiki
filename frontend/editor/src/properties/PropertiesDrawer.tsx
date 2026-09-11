import { useEffect, useMemo, useState } from "react";
import type { EditorBridge, PagePropertyValue, PropertyDefinition, PropertyValueKind } from "../bridge/editorBridge";
import { getPropertySummary } from "./propertyModel";

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

function valuesToText(value: PagePropertyValue | undefined): string {
  return value?.values.join(", ") ?? "";
}

function PropertyTypeMark({ kind }: { kind: PropertyValueKind }) {
  const mark = kind === "checkbox" ? "✓" : kind === "date" ? "◷" : kind === "relation" ? "↗" : "•";
  return <span className="property-chip__mark" aria-hidden="true">{mark}</span>;
}

export function PropertiesSummary({
  bridge,
  workspaceId,
  pageId,
  refreshToken,
  onOpen,
}: {
  bridge: EditorBridge;
  workspaceId: string;
  pageId: string;
  refreshToken: number;
  onOpen: () => void;
}) {
  const [definitions, setDefinitions] = useState<PropertyDefinition[]>([]);
  const [values, setValues] = useState<PagePropertyValue[]>([]);

  useEffect(() => {
    let active = true;
    void Promise.all([
      bridge.listPropertyDefinitions(workspaceId),
      bridge.listPagePropertyValues(workspaceId, pageId),
    ]).then(([definitionResponse, valueResponse]) => {
      if (!active || !definitionResponse.ok || !valueResponse.ok) return;
      setDefinitions(definitionResponse.result);
      setValues(valueResponse.result);
    });
    return () => {
      active = false;
    };
  }, [bridge, pageId, refreshToken, workspaceId]);

  const summary = getPropertySummary(definitions, values);

  return (
    <div className="properties-summary" data-testid="properties-summary">
      <div className="properties-summary__identity">
        <span className="properties-summary__icon" aria-hidden="true">◈</span>
        <div>
          <span className="properties-summary__type">Wiki page</span>
          <span className="properties-summary__hint">Object metadata</span>
        </div>
      </div>
      <div className="properties-summary__chips" aria-label="Page properties">
        {summary.slice(0, 4).map((item) => (
          <span className="property-chip" key={item.id}>
            <PropertyTypeMark kind={item.valueKind} />
            <span className="property-chip__name">{item.name}</span>
            <span className="property-chip__value">{item.value}</span>
          </span>
        ))}
        {summary.length > 4 ? <span className="property-chip property-chip--muted">+{summary.length - 4} more</span> : null}
        <button className="properties-summary__add" onClick={onOpen} type="button">
          {summary.length === 0 ? "+ Add property" : "+ Property"}
        </button>
      </div>
      <button className="properties-summary__manage" onClick={onOpen} type="button" aria-label="Open properties">
        <PropertiesGlyph />
        <span>Properties</span>
      </button>
    </div>
  );
}

function PropertiesGlyph() {
  return (
    <svg width="15" height="15" viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <path d="M4 6h16M4 12h16M4 18h16" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" />
      <circle cx="9" cy="6" r="2" fill="currentColor" />
      <circle cx="15" cy="12" r="2" fill="currentColor" />
      <circle cx="10" cy="18" r="2" fill="currentColor" />
    </svg>
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
  const [catalogOpen, setCatalogOpen] = useState(false);
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

  const saveValue = async (definition: PropertyDefinition, text: string, existing?: PagePropertyValue) => {
    const nextValues = text.split(",").map((item) => item.trim()).filter(Boolean);
    if (nextValues.length === 0) {
      setError("Property values cannot be empty.");
      return;
    }
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

  const renderValueEditor = (
    definition: PropertyDefinition,
    existing: PagePropertyValue | undefined,
  ) => {
    const value = existing?.values[0] ?? "";
    const save = (nextValue: string) => void saveValue(definition, nextValue, existing);
    if (definition.valueKind === "checkbox") {
      return (
        <input
          key={`${pageId}-${definition.id}`}
          type="checkbox"
          checked={value === "true"}
          disabled={!editable}
          onChange={(event) => save(event.currentTarget.checked ? "true" : "false")}
          aria-label={definition.name}
        />
      );
    }
    if (definition.valueKind === "select") {
      return (
        <select
          key={`${pageId}-${definition.id}`}
          value={value}
          disabled={!editable}
          onChange={(event) => save(event.currentTarget.value)}
          aria-label={definition.name}
        >
          <option value="">Choose…</option>
          {definition.options.map((option) => <option value={option} key={option}>{option}</option>)}
        </select>
      );
    }
    return (
      <input
        key={`${pageId}-${definition.id}`}
        type={definition.valueKind === "number" ? "number" : definition.valueKind === "date" ? "date" : "text"}
        disabled={!editable}
        defaultValue={valuesToText(existing)}
        placeholder={`Add ${definition.name.toLowerCase()}`}
        onBlur={(event) => {
          if (event.currentTarget.value.trim()) save(event.currentTarget.value);
        }}
        aria-label={definition.name}
      />
    );
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
          <div>
            <p className="properties-drawer__eyebrow">Page metadata</p>
            <h2>Properties</h2>
          </div>
          <button className="properties-drawer__close" onClick={onClose} aria-label="Close properties">×</button>
        </header>
        {error ? <p className="properties-drawer__error" role="alert">{error}</p> : null}
        <section className="properties-drawer__section">
          <div className="properties-drawer__section-title"><h3>This page</h3><span>{activeDefinitions.length} available</span></div>
          {activeDefinitions.map((definition) => {
            const existing = values.find((value) => value.propertyDefinitionId === definition.id);
            return (
              <label className="property-row" key={definition.id}>
                <span className="property-row__name">{definition.name}</span>
                {renderValueEditor(definition, existing)}
                {existing ? <button disabled={!editable} onClick={() => void removeValue(existing)} aria-label={`Remove ${definition.name}`}>×</button> : null}
              </label>
            );
          })}
          {activeDefinitions.length === 0 ? <p className="properties-drawer__empty">Create a property in the catalog below.</p> : null}
        </section>
        <section className="properties-drawer__section">
          <button className="properties-drawer__catalog-toggle" onClick={() => setCatalogOpen((value) => !value)}>
            <span>Workspace catalog</span><span>{catalogOpen ? "−" : "+"}</span>
          </button>
          {catalogOpen ? (
            <div className="properties-catalog">
              <div className="properties-catalog__create">
                <input value={draftName} onChange={(event) => setDraftName(event.target.value)} placeholder="Property name" disabled={!editable} />
                <select value={draftKind} onChange={(event) => setDraftKind(event.target.value as PropertyValueKind)} disabled={!editable}>
                  {valueKinds.map((kind) => <option value={kind.value} key={kind.value}>{kind.label}</option>)}
                </select>
                <input value={draftGroup} onChange={(event) => setDraftGroup(event.target.value)} placeholder="Group (optional)" disabled={!editable} />
                {draftKind === "select" ? <input value={draftOptions} onChange={(event) => setDraftOptions(event.target.value)} placeholder="Options, comma-separated" disabled={!editable} /> : null}
                <button onClick={() => void createDefinition()} disabled={!editable || !draftName.trim()}>Create property</button>
              </div>
              {definitions.map((definition) => (
                <div className="catalog-row" key={definition.id}>
                  <input defaultValue={definition.name} disabled={!editable || definition.state === "retired"} aria-label={`Name for ${definition.name}`} onBlur={(event) => void renameDefinition(definition, event.currentTarget.value, definition.groupName ?? "")} />
                  <span>{definition.valueKind}{definition.groupName ? ` · ${definition.groupName}` : ""}</span>
                  {definition.state === "active" ? <button disabled={!editable} onClick={() => void retireDefinition(definition)}>Retire</button> : <em>Retired</em>}
                </div>
              ))}
            </div>
          ) : null}
        </section>
      </aside>
    </>
  );
}
