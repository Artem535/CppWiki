# Wiki Platform v9 Documentation

Generated documentation set for Wiki Platform v9 — Block Document Edition.

## Delivery Discovery Alignment

Документация должна начинаться с delivery discovery: сначала фиксируются problem statement, users/workflows, scope/non-goals, measurable success criteria, data/integration assumptions, risks, pilot gates and release gates. Архитектурные документы и ADR должны ссылаться на эти discovery decisions, а не заменять их.

Для инициатив с AI/LLM/RAG используйте шаблоны из `ML LLM Delivery Pipeline/`:

- `01 PRD Template.md` для service PRD / PRD-lite.
- `05 Evaluation Plan Template.md` для offline eval, release gates and regression policy.
- `06 Delivery Plan Template.md` для workstreams, owners, dependencies, rollout and delivery gates.

## Files

- `PRD_v9_Block_Document_Edition.md`
- `roadmap/Current_Roadmap.md`
- `architecture/Architecture_Baseline_Libraries_and_Approaches.md`
- `architecture/Desktop_Backend_Sync_Interaction.md`
- `architecture/Server_and_Realtime_Editing_Architecture.md`
- `architecture/Project_Structure_and_Style.md`
- `architecture/adr/ADR-001-editor-choice.md`
- `architecture/adr/ADR-002-document-model.md`
- `architecture/adr/ADR-003-collaboration-strategy.md`
- `architecture/adr/ADR-004-plugin-system.md`
- `architecture/adr/ADR-005-rendering-pipeline.md`
- `architecture/adr/ADR-006-confluence-integration.md`
- `architecture/adr/ADR-007-rejection-qtextedit-djot-first.md`
- `architecture/adr/ADR-008-authentik-auth-model.md`
- `architecture/spikes/Couchbase_Sync_Gateway_OIDC_Example.md`
- `backlog/Desktop_Collaboration_Backlog.md`
