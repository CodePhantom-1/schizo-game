# Art pipeline research — automating the art

**Status:** research only (2026-09-25). Nothing installed or downloaded. Needs designer choice before any work starts.

**Bottom line:** the most automatable path is **art built by code plus free materials**, not AI-generated models. The dev machine's GPU (RX 5700 XT) rules out most AI 3D generators anyway.

## 1. The hardware constraint

- The main AI 3D generators (TRELLIS, Hunyuan3D) are built for NVIDIA cards. Microsoft lists [TRELLIS.2](https://github.com/microsoft/TRELLIS.2) as needing an NVIDIA card with 24 GB.
- The RX 5700 XT is AMD with 8 GB (chip `gfx1010`), and AMD's ROCm toolkit dropped official support for it years ago ([ROCm discussion](https://github.com/ROCm/ROCm/discussions/4030)).
- **What does work locally:** [comfyui-rocm-gfx1010](https://github.com/KenshiN28rus/comfyui-rocm-gfx1010) runs ComfyUI (PyTorch 2.8, ROCm 6.2) on exactly this card, at about 1.5–2 minutes per SDXL 1024² image. That's enough for textures, concept art and tablet images.
- **Running a 3D generator locally** is an unproven experiment on this card. The best candidate is [Hunyuan3D-2mini](https://github.com/Tencent-Hunyuan/Hunyuan3D-2), which needs about 5 GB for shapes. The [low-memory fork Hunyuan3D-2GP](https://github.com/deepbeepmeep/Hunyuan3D-2GP) runs in under 6 GB.
- The card also runs the local LLM, so it can't do both at once.

## 2. Recommended pipeline

| Layer | Approach | Why it suits the game |
|---|---|---|
| **Buildings** (the biggest art risk) | Scripts that generate them: Blender run from the command line (`blender -b -P`), or Unreal's procedural tools ([PCG](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview), [PCG + Geometry Script walls](https://forums.unrealengine.com/t/community-tutorial-pcg-geometry-script-in-ue-5-5-wall-generator/2118674), [5.7 procedural buildings](https://forums.unrealengine.com/t/community-tutorial-pcg-generacion-de-edificios-procedurales-en-unreal-engine-5-7/2680372), [PCG 5.7 foundation](https://dev.epicgames.com/community/learning/tutorials/bEZn/procedural-content-generation-in-unreal-engine-5-7-foundation)) | Mudbrick architecture is rule-shaped: boxes, flat roofs, buttresses, reed layers. An agent can write a "house kit" from `db/canon/buildings.csv`, and the code is reviewable, easy to redo, and never has messy AI geometry. |
| **Materials** | Free CC0 packs: [ambientCG](https://ambientcg.com/) (2,000+ PBR materials), Poly Haven, and Fab's free Megascans starter pack, 1,500+ assets under the Fab Standard licence ([overview](https://app.cinevva.com/guides/free-3d-model-sites)) | Mud plaster, reed, stone, cloth. CC0 needs no attribution and carries no legal risk. |
| **Unique materials and textures** | ComfyUI on the local card, plus [StableGen](https://github.com/sakalond/StableGen), a Blender add-on that paints AI textures onto models through a ComfyUI backend (supports Linux) | For carved reliefs, painted walls, and seals made from the canon. |
| **Real artifacts** | [British Museum on Sketchfab](https://sketchfab.com/britishmuseum/models) (Assyrian and Ashurbanipal scans), [Smithsonian 3D](https://3d.si.edu/object/3d/cuneiform-tablet-iraq:761f8e4b-f943-44c4-bcc9-43795999566d) (cuneiform tablets), [Mesopotamia-tagged models](https://sketchfab.com/tags/mesopotamia), [Sumerian architecture models](https://sketchfab.com/tags/sumerian-architecture) | Licences vary per item. CC-BY needs a credit, NC means no commercial use, and Smithsonian differs object by object. Each download needs a licence check. |
| **One-off props** (pots, tools) | AI image-to-3D on a free cloud GPU (Colab or Kaggle), or code-built in Blender | TRELLIS.2 is MIT-licensed. Hunyuan3D's licence excludes the UK, EU and South Korea and needs a separate licence above 1M monthly users ([details](https://www.cmarix.com/blog/top-open-source-ai-models-for-3d-image-generation/)), which is a problem for a game sold there. |
| **Characters** | MetaHuman, free with Unreal. The creator [runs on Linux since 5.7](https://www.metahuman.com/releases/metahuman-5-7-is-now-available), so it works on the 5.8.3 source build. The [licence](https://www.metahuman.com/license) is free under $1M revenue and allows use in any engine. | The gap is period clothing. That needs Blender work or StableGen. |
| **Animation** | Mixamo plus Epic's free Game Animation Sample for movement. For rites and gestures: [Hunyuan Motion 1.0](https://www.i-scoop.eu/hunyuan-motion-1-0-a-text-to-human-motion-model/) (text to motion, SMPL-H skeleton, retargetable) or [Mocara-Unreal](https://github.com/Conalh/Mocara-Unreal) (text to motion inside UE 5.8) | |

## 3. How agents drive it

- **Tools that let an agent control Blender or Unreal** (MCP servers):
  - Blender: [blender-mcp](https://github.com/ahujasid/blender-mcp), [mcp-blender](https://github.com/RFingAdam/mcp-blender) (218 tools), [blender-mcp-bridge](https://github.com/seehiong/blender-mcp-bridge)
  - Unreal: [mcp-unreal](https://github.com/remiphilippe/mcp-unreal) (headless builds, procedural meshes), [GenOrca/unreal-mcp](https://github.com/GenOrca/unreal-mcp) (253 actions plus Python), [ChiR24/Unreal_mcp](https://github.com/ChiR24/Unreal_mcp), [aadeshrao123/Unreal-MCP](https://github.com/aadeshrao123/Unreal-MCP) (UE 5.6–5.8)
- **Preferred for the pipeline itself:** plain scripts run from the command line (`blender -b -P build_house.py`, then an Unreal import script) over driving the editor through MCP. They repeat exactly, can be checked automatically, and fit the repo's "code + data + docs + tests" rule.
- **Verification check for art:**
  1. **Automatic checks:** polygon count, lightmap UVs, scale in centimetres, and the material slots are set.
  2. **Thumbnails:** the script renders the model from fixed angles.
  3. **Visual review:** a reviewer agent looks at the images and compares them against the canon row.
  4. **Asset log:** every asset gets a row in a new `db/assets.csv` with `license` and `source_ref` columns, following the same tag rules as `canon_lint.py`.

## 4. Recommendation

1. Start with code-built buildings, free materials and MetaHuman. That covers the City of the Moon building kit, one of the two art risks named in `plan.md` §7, without new hardware.
2. Use AI 3D generation only for props, on cloud GPUs, with MIT-licensed models (TRELLIS.2).
3. Clothing is the other named art risk, and it's where automation is weakest. Expect that to need the designer's eye at the design checks.

**First test when approved:** one agent builds a mudbrick house kit in Blender from `buildings.csv` and renders thumbnails for the designer to judge.
