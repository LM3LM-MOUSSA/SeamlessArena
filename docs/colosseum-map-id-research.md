# Colosseum map ID research

## Visual inspection update — 17 October 2026

The user inspected the Colosseum maps in Smithbox 2.2.6 and confirms that **all three IDs ending in `00` are full-detail Colosseum maps**, not LOD views. The supplied screenshot shows `m45_01_00_00: Caelid Colosseum` selected with its detailed interior rendered in the viewport.

| Map ID | Finding |
| --- | --- |
| `m45_00_00_00` | Royal Colosseum — full-detail map, user-confirmed in Smithbox |
| `m45_01_00_00` | Caelid Colosseum — full-detail map; shown in the supplied screenshot |
| `m45_02_00_00` | Limgrave Colosseum — full-detail map, user-confirmed in Smithbox |

The `m45_*_00_99` variants remain a separate open question. Their community listing says “Overworld,” but their actual purpose and relationship to the full-detail maps have not yet been established. The Smithbox inspection confirms map presentation; runtime travel and arena gameplay still need in-game verification.

**Updated:** 17 October 2026  
**Status:** Initial research; map names cross-checked, runtime role still to be verified.

## Findings

The community-maintained Souls Modding Wiki map-name list associates these map IDs with the three Colosseums:

| Map ID | Listed name | Current interpretation |
| --- | --- | --- |
| `m45_00_00_00` | Royal Colosseum | Colosseum map entry |
| `m45_01_00_00` | Caelid Colosseum | Colosseum map entry |
| `m45_02_00_00` | Limgrave Colosseum | Colosseum map entry |
| `m45_00_00_99` | Royal Colosseum: Overworld | Overworld-labeled variant; exact role unverified |
| `m45_01_00_99` | Caelid Colosseum: Overworld | Overworld-labeled variant; exact role unverified |
| `m45_02_00_99` | Limgrave Colosseum: Overworld | Overworld-labeled variant; exact role unverified |

The IDs and labels are listed in the [Souls Modding Wiki map-name list](https://www.soulsmodding.com/doku.php?id=er-refmat:map-name-list). That listing supports the names above, but it does not establish that the `99` entries are LOD maps.

## LOD hypothesis

The `00` and `99` entries form matching pairs by Colosseum. It is reasonable to investigate whether they represent different map variants, but the source calls the `99` entries **Overworld**, not LOD. Treating `99` as LOD is currently a hypothesis, not a confirmed finding. An overworld representation could be a separate map scene or another game-specific representation; the names alone do not tell us which.

For now, use these neutral descriptions in code and notes:

- `m45_*_00_00`: named Colosseum entry.
- `m45_*_00_99`: Overworld-labeled Colosseum variant.

Avoid encoding `99 = LOD` until the game files or runtime behavior confirm it.

## What to verify next

1. In Smithbox, inspect the map files for each `m45_*_00_00` and matching `m45_*_00_99` ID on the same Elden Ring game version. Record whether an MSB exists for each ID and compare its parts, events, regions, and entity data.
2. Compare the actual geometry and asset references. A lower-detail visual representation is evidence for an LOD role; a separate scene or gameplay entities could indicate a different purpose.
3. Observe the transition into each Colosseum in game and record which map ID is active before and after the transition. If available, use safe, read-only map/runtime diagnostics.
4. Record the game version, file names, and screenshots or observations so the conclusion can be reproduced.

Smithbox is a map and game-data inspection tool; the [DSMapStudio map ID reference](https://dsmapstudio.com/editors/map-editor/map-ids) and the [Souls Modding map overview](https://www.soulsmodding.com/doku.php?id=er-refmat:map-overview) provide additional community documentation about map IDs and map data. These are reverse-engineering references, not official FromSoftware documentation.

## Project implication

The arena mode can use the confirmed Colosseum IDs as candidate destinations, but selection should wait until we confirm which map variant supports the intended arena scene and player spawn behavior. Keep the `99` variant available as a research lead rather than hard-coding it as an LOD asset.
