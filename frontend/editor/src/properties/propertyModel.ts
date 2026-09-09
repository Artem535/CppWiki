export const propertyValueKinds = ["text", "number", "date", "checkbox", "select", "multiSelect", "tags", "relation"] as const;
export type PropertyValueKind = (typeof propertyValueKinds)[number];
export type PropertyState = "active" | "retired";

export type PropertyDefinition = {
  id: string;
  workspaceId: string;
  name: string;
  groupName?: string | null;
  valueKind: PropertyValueKind;
  options: string[];
  state: PropertyState;
};

export type PagePropertyValue = {
  id: string;
  workspaceId: string;
  pageId: string;
  propertyDefinitionId: string;
  values: string[];
};

export type PropertySummaryItem = {
  id: string;
  name: string;
  value: string;
  valueKind: PropertyValueKind;
};

export function formatPropertyValue(_definition: PropertyDefinition, value: PagePropertyValue): string {
  return value.values.join(", ");
}

export function getPropertySummary(
  definitions: PropertyDefinition[],
  values: PagePropertyValue[],
): PropertySummaryItem[] {
  return definitions
    .filter((definition) => definition.state === "active")
    .map((definition) => {
      const value = values.find((item) => item.propertyDefinitionId === definition.id);
      return value
        ? { id: definition.id, name: definition.name, value: formatPropertyValue(definition, value), valueKind: definition.valueKind }
        : null;
    })
    .filter((item): item is PropertySummaryItem => item !== null);
}

export function upsertPagePropertyValue(values: PagePropertyValue[], nextValue: PagePropertyValue): PagePropertyValue[] {
  const index = values.findIndex((value) => value.id === nextValue.id);
  if (index < 0) return [...values, nextValue];
  return values.map((value, valueIndex) => (valueIndex === index ? nextValue : value));
}

export function removePagePropertyValue(values: PagePropertyValue[], valueId: string): PagePropertyValue[] {
  return values.filter((value) => value.id !== valueId);
}
