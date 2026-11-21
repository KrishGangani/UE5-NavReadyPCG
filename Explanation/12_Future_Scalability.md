# 12 · Future Scalability: NavReadyPCG

What it would take to grow NavReady beyond this project's 16 non-partitioned cells (the growth paths of [02](02_System_Architecture.md) §11), and what would have to be re-verified on a newer engine.

| Direction | What changes | Cost / risk |
|---|---|---|
| **More cells** | The registry lookup (`IndexOfByKey`) is linear: store the slot index on the component. `HasDirtyTiles(FBox)` walks the pending and running tile lists per cell per frame: with hundreds of cells, bucket the dirty tiles by tile coordinate once per frame | Low; the per-frame cost is already its own CSV stat |
| **Partitioned / hierarchical PCG** | One NavReady cell per generated partition cell, keyed by grid coordinates. The runtime delegates exist per component, not per local component (`01b #9`), so the cell would follow the partition actor | Medium; re-check the delegate behaviour on the target version |
| **Edge props and ready neighbours** | A later cell's edge props can re-dirty a ready neighbour's tile. S3 counts these episodes (`TimesDirtiedWhileReady`); a production version would move that neighbour back to "committed" (pausing its AI) or keep props away from cell edges | Low to implement; the policy needs the measurement first |
| **Several agent sizes** | One probe per navigation data (one Recast navmesh per agent size); AI-ready per agent size | Low |
| **World Partition / streaming** | Cells register and unregister with streaming; a streamed-in cell whose content already exists commits on registration | Medium |
| **Dedicated server** | NavReady runs where navigation runs (the server) | Low |
| **Persistence** | The content hash identifies a cell's content, so cached navigation or a cached "ready" state can be keyed by it | Medium |
| **StateTree / AI frameworks** | A StateTree condition or a Smart Object availability tied to `IsAIReady()` / `OnCellStateChanged` | Low |
| **Engine upgrades** | Re-verify the PCG component delegates, `SetCanEverAffectNavigation` on generated ISMs, `HasDirtyTiles(FBox)` and `GrowBoundingBox`, `RECAST_ASYNC_REBUILDING`, and the tick order (navigation before the tick groups) | The plugin's `static_assert`s and unit tests catch part of it; the rest is the `09` checks |
