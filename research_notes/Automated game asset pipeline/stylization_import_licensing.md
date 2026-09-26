# Automated stylization, UE 5.8 import, scattering and license tracking for a low-poly stylized (Valheim-like) game, built headless on Linux

Scope note: researched 2026-09-26, 17 tool calls. Items marked **[unverified]** come from working API knowledge that I could not confirm against a fetched page this session. Treat them as the starting point for a smoke test, not as documented fact.

## 1. Mesh stylization and LOD generation (decimation, remesh, flat shading, meshoptimizer)

### Takeaway
Use gltfpack/meshoptimizer as the headless CLI workhorse: `-si R` simplification for LODs, quantization and texture compression. Use Blender in background mode for anything stylistic, such as planar/collapse decimate, weld, voxel remesh, flat shading and vertex-color bakes. UE's own `StaticMeshEditorSubsystem.set_lods()` can regenerate LODs after import, so you do not have to ship pre-made LOD files.

### Cited Findings
- gltfpack `-si R` simplifies meshes toward a triangle/point-count ratio R (0 to 1, default 1). Other flags: `-noq` disables the default `KHR_mesh_quantization`; `-vpf` uses floating-point position quantization; `-kn` keeps named nodes; `-km` keeps named materials and turns off material merging; `-ke` keeps extras; `-mi` uses mesh instancing for repeated meshes; `-tc` converts textures to KTX2/BasisU; `-tw` converts them to WebP; `-cc` / `-c` / `-cz` apply meshopt compression. Run `gltfpack -h` for the full list. — [meshoptimizer gltfpack docs](https://meshoptimizer.org/gltf/)
- UE `StaticMeshEditorSubsystem` exposes `set_lods(static_mesh, reduction_options) -> int32`, which removes the existing LODs and builds new ones from reduction settings and applies the changes automatically. It also has `set_lod_reduction_settings(static_mesh, lod_index, reduction_options)`, `set_lod_build_settings(static_mesh, lod_index, build_options)` and `get_lod_reduction_settings(...)`. — [UE Python API: StaticMeshEditorSubsystem](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/StaticMeshEditorSubsystem?application_version=5.6)
- Since Blender 2.92 a bake can target Vertex Colors instead of Image Textures (Render Properties > Bake > Output). The mesh needs an existing color attribute layer first. — [Victor Karp: bake textures to vertex colors](https://victorkarp.com/how-to-bake-textures-to-vertex-colors-in-blender/)

### Inferences
- A suggested headless Blender recipe for "stylize one asset" **[unverified operator/property names; smoke-test on Blender 4.x/5.x]**:
  ```bash
  blender -b -P stylize.py -- in.glb out.glb 0.15
  ```
  ```python
  import bpy, sys
  src, dst, ratio = sys.argv[sys.argv.index('--')+1:]
  bpy.ops.wm.read_factory_settings(use_empty=True)
  bpy.ops.import_scene.gltf(filepath=src)
  for ob in [o for o in bpy.context.scene.objects if o.type == 'MESH']:
      bpy.context.view_layer.objects.active = ob
      ob.modifiers.new('weld', 'WELD').merge_threshold = 0.001
      d = ob.modifiers.new('dec', 'DECIMATE'); d.decimate_type = 'COLLAPSE'; d.ratio = float(ratio)
      p = ob.modifiers.new('plan', 'DECIMATE'); p.decimate_type = 'DISSOLVE'; p.angle_limit = 0.087  # ~5 deg planar
      for m in list(ob.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
      bpy.ops.object.shade_flat()
  bpy.ops.export_scene.gltf(filepath=dst, export_format='GLB')
  ```
  - For AI meshes (triangle soup, broken topology), run a `REMESH` modifier (`mode='VOXEL'`, `voxel_size` of about 2 to 5 cm at game scale) before COLLAPSE. Voxel remesh closes holes and removes the self-intersections that make quadric decimation look bad.
  - For the albedo+AO to vertex color bake, use `bpy.ops.object.bake(type='DIFFUSE', pass_filter={'COLOR'}, target='VERTEX_COLORS')` with Cycles, after `mesh.color_attributes.new('Col', 'BYTE_COLOR', 'CORNER')`. Then drop the texture entirely and use a single shared vertex-color master material in UE. This is the cheapest way to get the Valheim/Townscaper "flat painted" look and it kills per-asset texture memory.
- LOD strategy: generate only LOD0 in Blender/gltfpack (stylized decimate), then call `set_lods` in UE with a reduction for LOD1 to LOD3 (for example 50/25/10 % triangles). Keep one source of truth. gltfpack `-si 0.5`, `-si 0.25` is the alternative if you want LOD files outside the engine, but UE's glTF import would then need them merged into LODs manually.
- Instant Meshes (quad remesher) has a batch CLI mode but I did not verify its flags. Quad topology adds nothing for a flat-shaded triangle look, so skip it.
- trimesh: `mesh.simplify_quadric_decimation(...)` exists **[unverified: the signature changed across versions and it depends on `fast-simplification`]**. Prefer gltfpack for pure-CLI work.

### Gaps
- I could not fetch the exact `bpy.ops.object.bake` / `shade_flat` signature text from docs.blender.org (the page excerpt was truncated), so the Blender snippet above is unverified.
- No verified Instant Meshes CLI flags were found.

## 2. Texture stylization (downscale, palette quantization, color transfer)

### Takeaway
Pillow does most of it in a few lines. Downscale with `Resampling.BOX` (area average), then `quantize()` against a fixed project palette image with `dither=Dither.NONE`. That forces every source (CC0 scans, glTF packs, AI textures) into the same colours. Upscale only with `NEAREST`, and only if you are not using nearest filtering in-engine.

### Cited Findings
- `Image.quantize(colors=256, method=None, kmeans=0, palette=None, dither=Dither.FLOYDSTEINBERG)`. Methods: `Quantize.MEDIANCUT`, `MAXCOVERAGE`, `FASTOCTREE`, `LIBIMAGEQUANT`. Dither: `Dither.NONE`, `Dither.FLOYDSTEINBERG`. Resampling includes `Resampling.NEAREST` and `Resampling.BOX`. — [Pillow Image docs](https://pillow.readthedocs.io/en/stable/reference/Image.html)
- Fortnite's team tried simplifying realistic photo textures in Photoshop and found the results "too noisy", so they switched to hand-painted textures. Sea of Thieves likewise simplified photo references by hand rather than using photos directly. — [Habrador: lessons from Sea of Thieves and Fortnite](https://blog.habrador.com/2018/08/stylized-graphics-fortnite-sea-of-thieves.html)

### Inferences
- Minimal palette-lock script:
  ```python
  from PIL import Image
  pal = Image.open('palette_32.png').convert('P', palette=Image.Palette.ADAPTIVE, colors=32)  # a strip of the project palette
  im = Image.open(src).convert('RGB').resize((128, 128), Image.Resampling.BOX)
  im.quantize(palette=pal, dither=Image.Dither.NONE).convert('RGB').save(dst)
  ```
  Per-texture `kmeans=` or `method=Quantize.LIBIMAGEQUANT` gives nicer per-asset palettes, but it breaks cross-asset consistency. The fixed `palette=` path is the one that unifies mixed sources.
- The Fortnite finding matters: quantizing a photoscan still leaves high-frequency noise. Before quantizing, blur or median-filter first (`ImageFilter.MedianFilter(5)`, or downscale to 32 to 64 px and back) so that each palette region becomes a flat patch. Also drop normal/roughness maps from scans entirely, and use constant roughness in the master material.
- Consistency across sources, in increasing order of cost: (a) fixed-palette quantize; (b) per-channel mean/std colour transfer in LAB (Reinhard-style, about 10 lines of numpy) toward a reference image before quantizing; (c) a gradient map, which takes luminance and looks it up in a 1D ramp per material class (wood, stone, foliage). Option (c) is also available in-engine as a material function, so textures can stay greyscale and be tinted per instance.
- Target texel density: 64 to 256 px per asset, or a shared 256 to 512 px palette atlas (Kenney/"Synty"-style palette UVs). This makes many props share one material, which is the main draw-call win (see section 4).

### Gaps
- No primary source was found for Valheim's actual texture resolutions or pipeline. The community describes them as low-res, point-filtered textures (see section 5).
- `ImageOps.posterize` was not in the fetched excerpt. It exists (bits per channel) but its signature was not verified.

## 3. UE 5.8 Python import in headless/commandlet mode (AssetImportTask / Interchange, LODs, collision, Nanite, textures, MICs)

### Takeaway
Two import paths are scriptable. (1) Legacy `unreal.AssetImportTask` + `AssetToolsHelpers.get_asset_tools().import_asset_tasks([...])` with `automated=True`. (2) Interchange, via `InterchangeManager` + `ImportAssetParameters(is_automated=True)`, which is the path that handles glTF/.glb natively. Post-import fixups (LODs, collision, Nanite, texture filter/mips/group) all go through `StaticMeshEditorSubsystem` and `set_editor_property`.

### Cited Findings
- `AssetImportTask` basics: set `automated=True` (no dialog), `destination_path`, `destination_name`, `filename`. Mesh options go in `static_mesh_import_data`, which includes Nanite build settings. — [Deon Wilson: Python import static meshes](https://deonwilson.artstation.com/blog/bl7N/using-python-in-unreal-to-import-static-meshes); [Epic tutorial: Enable Nanite with Python](https://dev.epicgames.com/community/learning/tutorials/4x7v/unreal-engine-enable-nanite-with-python)
- Interchange: `ImportAssetParameters` has `reimport_asset`, `reimport_source_index`, `is_automated`, `override_pipelines` and progress delegates. The documented script branches on the extension: glb/gltf imports use a dedicated glTF assets pipeline, FBX/USD use the default ones. "Import Into Level" currently works only for glTF and MaterialX. — [UE 5.8 docs: Importing Assets Using Interchange](https://dev.epicgames.com/documentation/unreal-engine/importing-assets-using-interchange-in-unreal-engine?lang=en-US); [ImportAssetParameters API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ImportAssetParameters?application_version=5.3)
- Custom Interchange pipelines can be written in Python by subclassing `unreal.InterchangePythonPipelineBase` with `@unreal.uclass()` and `unreal.uproperty(...)`. They must be registered as Startup Scripts in project settings. The 5.8 doc page contains no commandlet/automation guidance. — [UE 5.8 Interchange docs](https://dev.epicgames.com/documentation/unreal-engine/importing-assets-using-interchange-in-unreal-engine?lang=en-US)
- A forum thread specifically covers running Interchange from Python inside a commandlet, which suggests it is a known friction point. — [Epic forums: Interchange with Python in a commandlet](https://forums.unrealengine.com/t/interchange-with-python-in-an-unreal-commandlet/724404)
- Collision: `add_simple_collisions(static_mesh, shape_type) -> int32` and `set_convex_decomposition_collisions(static_mesh, hull_count, max_hull_verts, hull_precision) -> bool` both apply automatically. `get_collision_complexity(static_mesh)` returns the `CollisionTraceFlag`. Nanite: `set_nanite_settings(static_mesh, nanite_settings, apply_changes=True)` / `get_nanite_settings`. — [StaticMeshEditorSubsystem API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/StaticMeshEditorSubsystem?application_version=5.6)
- Texture2D editor properties: `filter` (TextureFilter), `mip_gen_settings` (TextureMipGenSettings, including `TMGS_NO_MIPMAPS`), `compression_settings`, `lod_group` (TextureGroup, including `TEXTUREGROUP_PIXELS2D`), `srgb`, `max_texture_size`, `never_stream`, `lod_bias`. — [UE Python API: Texture2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Texture2D?application_version=5.6)

### Inferences
- Headless run:
  ```bash
  UnrealEditor-Cmd Game.uproject -run=pythonscript -script="/abs/path/import_all.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput
  ```
  **[unverified exact flag set for 5.8]** `-run=pythonscript -script=` is the long-standing pattern. `-NullRHI` avoids needing a GPU in CI, but Nanite build, texture compression and anything that renders thumbnails may behave differently without an RHI, so test it. If a step fails under the commandlet, fall back to `UnrealEditor -ExecutePythonScript=... -unattended -RenderOffscreen`. On Linux use the `UnrealEditor-Cmd` binary from `Engine/Binaries/Linux/`. Paths must be absolute and forward-slashed.
- Post-import fixup sketch **[enum/struct names from API knowledge; smoke-test]**:
  ```python
  import unreal
  sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
  for path in unreal.EditorAssetLibrary.list_assets('/Game/Props', recursive=True):
      a = unreal.EditorAssetLibrary.load_asset(path)
      if isinstance(a, unreal.StaticMesh):
          ns = sm_sub.get_nanite_settings(a); ns.enabled = False
          sm_sub.set_nanite_settings(a, ns, apply_changes=True)
          opts = unreal.StaticMeshReductionOptions()
          opts.reduction_settings = [unreal.StaticMeshReductionSettings(1.0, 1.0),
                                     unreal.StaticMeshReductionSettings(0.5, 0.5),
                                     unreal.StaticMeshReductionSettings(0.25, 0.25)]
          sm_sub.set_lods(a, opts)
          sm_sub.add_simple_collisions(a, unreal.ScriptingCollisionShapeType.NDOP10_X)  # props; BOX for crates
      elif isinstance(a, unreal.Texture2D):
          a.set_editor_property('filter', unreal.TextureFilter.TF_NEAREST)
          a.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
          a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
          a.set_editor_property('max_texture_size', 256)
      unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
  ```
- Material instances: `unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())`, then `MaterialEditingLibrary.set_material_instance_parent / set_material_instance_texture_parameter_value / ..._vector_parameter_value / ..._scalar_parameter_value`, then assign it with `static_mesh.set_material(i, mic)`. **[unverified in 5.8 docs this session; long-stable API]**
- Nanite decision for this style: Nanite **off** by default. Low-poly props (under ~2k tris) gain nothing from Nanite's cluster LOD. Nanite also adds a base cost, and on an RX 5700 XT (RDNA1) or an Intel iGPU the non-Nanite path with HISM + LODs is the safer baseline. Consider Nanite only for dense rock/cliff kits. UE 5.x Nanite foliage is a newer feature that is irrelevant to flat-shaded foliage cards.
- Mips vs no mips with nearest filtering: `TMGS_NO_MIPMAPS` gives the crisp PS1 look but shimmers badly at distance on scattered foliage. Keep mips with `TF_NEAREST` on the texture (point sampling per mip), or use nearest only on hero/close assets. Valheim players complained about exactly this shimmering and pixelation (see section 5 threads).

### Gaps
- No official 5.8 commandlet + Interchange example was found. The full 85-line Interchange script on the docs page did not render in the fetch.
- Whether `-NullRHI` breaks texture compression or Nanite builds in 5.8 on Linux is unconfirmed.

## 4. Scattering and instancing (ISM vs HISM vs Foliage vs PCG) and performance budgets

### Takeaway
For thousands of static, low-poly, non-Nanite props and plants, HISM (which the Foliage system uses) is Epic's recommended choice. ISM is for moving instances or Nanite-only projects. PCG graphs can be created and parameterised from Python, but authoring node wiring from Python is poorly supported. Build the graph once by hand, feed it data (CSV → DataTable / attribute sets), and drive or regenerate it from Python.

### Cited Findings
- UE 5.8 docs: HISM is optimal for "1000s of instances that don't move", because a static hierarchy accelerates culling and LOD. ISM has no hierarchy and "has to cull and LOD each instance on the GPU, which can be expensive on lower powered platforms." If a project uses only Nanite, ISM is always the choice. HISM suits meshes with few triangles, where per-instance LOD precision matters less. Moving HISM instances can cause errors. — [UE 5.8: Instanced Static Mesh Component](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine)
- A PCG graph asset can be created from Python with `asset_tools.create_asset('PCG_Test', '/Game/PCG_Graphs', unreal.PCGGraph, unreal.PCGGraphFactory())`. Creating and connecting nodes from Python is described as hard and is a recurring forum question. — [Epic forums: Create PCG Graph with Python](https://forums.unrealengine.com/t/create-pcg-graph-with-python/1714891); [Changing PCG graph parameters from Python](https://forums.unrealengine.com/t/how-to-change-pcg-graph-parameters-from-python/2060532)
- The PCG framework runs a procedural node graph in-editor and at runtime. Spatial data flows in from a PCG Component in the level. — [UE 5.8 PCG overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- PCG (UE 5.2+) can consume point data from CSV via a DataTable, with columns such as position, rotation, scale, mesh and material. The Static Mesh Spawner selects the mesh from an attribute. SideFX's Labs PCG Export pushes CSV or Alembic point clouds into it. — [SideFX: How to Export to PCG in Unreal](https://www.sidefx.com/tutorials/how-to-export-to-pcg-in-unreal/); [Epic forums: PCG placement from CSV](https://forums.unrealengine.com/t/pcg-content-placement-based-on-the-points-from-csv/1185081)
- Instancing renders many copies in one draw call. HISM can add a few extra draws because it groups instances by LOD. — [Epic forums: foliage vs static meshes](https://forums.unrealengine.com/t/foliage-system-vs-static-meshes-with-lods-for-performance/377403) (community source)

### Inferences
- Pipeline: an external generator (Python) writes `scatter.csv` (`Name,Mesh,X,Y,Z,Yaw,Scale`). The import script creates/reimports a DataTable from the CSV. A hand-built PCG graph loads it (the "Load Data Table" / "Data Table Row To Attribute Set" nodes, whose exact names should be checked in the 5.8 node reference) and spawns through Static Mesh Spawner. Python then calls `pcg_component.generate(force=True)` **[unverified method signature]** and saves the level. The bypass that needs no PCG: spawn an actor with a `HierarchicalInstancedStaticMeshComponent` per mesh and call `add_instances(transforms, ...)` from Python straight from the CSV. It is simpler, fully scriptable, and good enough for a static world.
- Budgets for RX 5700 XT / Intel iGPU (rule-of-thumb inferences, not sourced): about 1,000 to 2,000 draw calls on the 5700 XT and about 500 on an iGPU, counting each HISM mesh×material×LOD cluster. Scene triangles of a few million on the 5700 XT and about 1M on an iGPU. With 128 to 256 px palette textures, texture memory is trivial: 100 unique 256² BC1 textures are about 3 to 4 MB. The practical limiters are overdraw from alpha-tested foliage cards and shadow cascades (Virtual Shadow Maps are costly without Nanite; consider classic CSM with a short distance plus distance-field shadows). Share one or a few master materials so instances batch.
- Impostors for distant vegetation: UE ships an Impostor Baker plugin/content (octahedral impostors). With aggressive LOD3 of about 20 tris on low-poly trees, plus HISM cull distances, impostors are likely unnecessary. Add them only if profiling shows the far-field tree count dominates.

### Gaps
- No authoritative published numbers for draw call or instance budgets specific to RX 5700 XT or Intel iGPU in UE 5.8. Profile with `stat unit`, `stat rhi` and `stat scenerendering`.
- Exact 5.8 PCG node names for DataTable/CSV input and the Python `generate` API were not verified.

## 5. License and attribution tracking

### Takeaway
Adopt the REUSE spec. Put the license texts in `LICENSES/<SPDX-ID>.txt`, and add either a `<file>.license` sidecar per asset or glob annotations in `REUSE.toml`. Keep your own richer JSON/CSV manifest (source URL, author, license, date, processing steps) as the single source for both REUSE annotations and the in-game credits screen.

### Cited Findings
- REUSE sidecars: for an uncommentable binary `cat.jpg`, add `cat.jpg.license` containing `SPDX-FileCopyrightText: 2019 Jane Doe <jane@example.com>` and `SPDX-License-Identifier: MIT` (UTF-8). — [REUSE spec 3.3](https://reuse.software/spec-3.3/)
- `REUSE.toml` (version = 1) with `[[annotations]]` entries: `path` (globs, `*`/`**`), `precedence` (`closest` / `aggregate` / `override`), `SPDX-FileCopyrightText`, `SPDX-License-Identifier`. — [REUSE spec 3.3](https://reuse.software/spec-3.3/)
- License texts must live in `LICENSES/[SPDX-ID].txt` at the project root, for example `LICENSES/CC0-1.0.txt`. The REUSE tool (codeberg.org/fsfe/reuse-tool) validates compliance. — [REUSE spec 3.3](https://reuse.software/spec-3.3/)

### Inferences
- Manifest (`assets/manifest.csv` or JSON). One row per *source* asset: `id, source_url, author, license_spdx, retrieved_date, sha256_of_download, derived_files_glob, modified(bool), notes`. For AI-generated meshes, record the tool, model version, prompt and the tool's ToS license terms. Treat them as a separate class, because their copyright status is legally unsettled.
- Automation:
  1. The import script refuses any file whose source id is not in the manifest.
  2. A generator writes `REUSE.toml` annotations from the manifest.
  3. `pipx run reuse lint` runs in CI **[subcommands `lint` and `spdx` per the tool's docs, not fetched]**.
  4. A generator emits `Credits.csv`, which is imported as a DataTable for a credits UMG screen, grouped by license. Every CC-BY entry must render "Title by Author (URL), licensed CC BY 4.0, modified".
- Compatibility check: keep an allowlist of `CC0-1.0`, `CC-BY-4.0`, `MIT`, `OFL-1.1` (fonts), and `CC-BY-SA-4.0` only if you accept share-alike on the derived asset. Hard-fail on `CC-BY-NC*` and `CC-BY-ND*` for a commercial game, because NC forbids sale and ND forbids stylization. The rules fit in about 20 lines of Python. SPDX IDs make it a set lookup.
- Git LFS: store the *raw downloads* outside git (an archive or bucket, keyed by sha256 in the manifest), because they are reproducible from source_url. Commit the processed low-res glTF/PNG (small) and the `.uasset` files through LFS. With 128 to 256 px textures and flat-shaded meshes the processed set stays small.

### Gaps
- `reuse` CLI subcommand syntax was not fetched this session.
- No verified tool was found that auto-checks asset-license compatibility. It has to be a custom allowlist.

## 6. How successful stylized games get their look cheaply

### Takeaway
The recurring pattern is to spend on lighting, fog/atmosphere and composition rather than on texture or polygon detail. Simplify forms and hand-control colour through a small rule set, and let low-res or painted textures stay deliberately "unreal" to avoid the uncanny valley.

### Cited Findings
- Valheim blends low-res textures and sparse polygon detail with modern materials, lighting and post effects, inspired by PS1/DOS-era 3D before texture filtering. Iron Gate CEO Richard Svensson said composition and lighting matter more than texture quality and polygon count. He also said the low-res textures keep things slightly unreal and avoid an uncanny valley that would need "20x the work". Low-poly assets match the voxel terrain and let players build larger structures before performance drops. — [Screen Rant: How Valheim looks so good without fancy textures](https://screenrant.com/valheim-good-graphics-lighting-low-resolution-textures/) (secondary source quoting Svensson)
- Valheim players repeatedly ask about texture filtering and pixelation, which indicates point-filtered low-res textures without a player-facing option to smooth them. — [Steam: Texture filtering options?](https://steamcommunity.com/app/892970/discussions/0/3033725780704817455/?ctp=2); [Steam: Why is the game so pixelated](https://steamcommunity.com/app/892970/discussions/0/3104638636526968494/)
- Sea of Thieves (GDC 2018, art director Ryan Stevenson, "Visual Adventures on Sea of Thieves"): a few simple rules drive all visuals. The painterly environment diffuse is hand-painted following a rule set. — [Game Developer: how Rare crafted the look](https://www.gamedeveloper.com/art/video-how-rare-crafted-the-look-and-feel-of-i-sea-of-thieves-i-); [GDC Vault](https://gdcvault.com/play/1025015/Visual-Adventures-on-Sea-of); [80.lv GDC18 summary](https://80.lv/articles/gdc18-visual-adventures-on-sea-of-thieves)
- Sea of Thieves and Fortnite: complex scenes are built from simple combined shapes, "realistically wonky" deformation, and wear/tear/repair details. Fortnite removes parallel lines from real-world references and adds nothing "smaller than a mailbox". — [Habrador summary](https://blog.habrador.com/2018/08/stylized-graphics-fortnite-sea-of-thieves.html)
- Tiny Glade (Anastasia Opara, Tomasz Stachowiak) relies on real-time GI, GPU-driven rendering, custom depth of field and ray marching. It is lighting-heavy rather than texture-heavy, with procedural rules at the core. It started on Bevy/OpenGL and moved to Vulkan. — [Hacker News discussion](https://news.ycombinator.com/item?id=42191767); [Indie-Games.eu](https://www.indie-games.eu/tiny-glade-the-journey-of-a-cozy-castle-building-simulator/)

### Inferences
- Cheap recipe for UE 5.8:
  - One master material: vertex colour or palette-atlas albedo, constant roughness of about 0.8 to 1, no normal maps.
  - Strong directional light with warm/cool split toning.
  - Exponential height fog + volumetric fog + sky atmosphere carry the mood.
  - Lumen GI only if the 5700 XT budget allows. Otherwise use a baked or distance-field AO plus a skylight.
  - A post-process LUT/colour grade to unify palette, the in-engine equivalent of the Pillow palette-lock.
  - Optional nearest-filtered textures for the Valheim PS1 flavour.
- The palette-quantize plus vertex-colour bake route in sections 1 and 2 is the automated stand-in for Sea of Thieves' "hand-paint following a rule set". The rule set becomes the palette file and the gradient ramps.

### Gaps
- No primary sources were found this session for Townscaper, Dorfromantik or Genshin environment art techniques. These were not researched within the tool-call budget.
- No primary Iron Gate or GDC talk on Valheim rendering was found. The Svensson quotes come via Screen Rant.
- Tiny Glade's own technical write-ups (for example talks by Tomasz Stachowiak) were not located.
