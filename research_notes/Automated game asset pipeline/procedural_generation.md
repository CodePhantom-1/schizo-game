# Headless procedural generation of game assets on Linux (Blender / Material Maker / numpy / UE 5.8 Python)

Scope: unattended (CLI/Python) generation of buildings, props, vegetation, terrain, textures, VFX for a low-poly stylized Bronze-Age Mesopotamia UE 5.8 game. Machine: Linux, 8 cores, 31 GB RAM, AMD RX 5700 XT treated as unreliable, so everything needs a CPU fallback. Blender is **not installed** on this machine (checked `which blender` on 2026-09-26).

Research budget note: about 20 search/fetch calls. Items marked "(prior knowledge, unverified this session)" come from the researcher's background knowledge, not from a fetched source. The report writer should present them as unconfirmed or leave them out.

---

## 1. Blender headless: bmesh, Geometry Nodes, baking, export, versions

### Takeaway
Blender in background mode (`blender -b --factory-startup -P script.py -- args`) is the core of the pipeline. Geometry Nodes node groups can be built and attached from Python (`bpy.data.node_groups.new(..., "GeometryNodeTree")`, `obj.modifiers.new(..., "NODES")`). Cycles baking works from background scripts, but memory is not freed between bakes, so run one Blender process per asset. The current LTS line has moved past 4.5: the live manual is titled "Blender 5.2 LTS".

### Cited Findings
- The Blender manual's current URL is titled "Blender 5.2 LTS Manual", so 5.2 is an LTS release as of 2026. The 4.5 LTS release notes are still published. — [Geometry Nodes Modifier, Blender 5.2 LTS Manual](https://docs.blender.org/manual/en/latest/modeling/modifiers/geometry_nodes.html); [Blender 4.5 LTS GN release notes](https://developer.blender.org/docs/release_notes/4.5/geometry_nodes/)
- Python pattern for Geometry Nodes: `bpy.data.node_groups.new("Name", "GeometryNodeTree")`, then `mod = obj.modifiers.new("MyGeoNodesModifier", "NODES"); mod.node_group = node_tree`. Nodes are parameterised with `node.inputs['Level'].default_value = 3`. Full examples are in github.com/cgwire/blender-scripting-geometry-nodes. — [CGWire blog (2026)](https://blog.cg-wire.com/blender-scripting-geometry-nodes-2/)
- There was an old bug where changing a GN modifier input from Python did not trigger an update. Scripts should force a depsgraph update after setting inputs. — [Blender T87006](https://developer.blender.org/T87006)
- An experimental third-party "Python node" for GN exists for 4.5, but "changing objects/meshes with the Python node may crash Blender". Avoid it in unattended pipelines. — [Snowdaw/blender-python-geometry-node](https://github.com/Snowdaw/blender-python-geometry-node)
- Sequential baking uses `blender --background --factory-startup --python`. Cycles baking "can consume excessive memory… memory is not freed after baking even after saving the image". The workaround is a driver script that restarts Blender for each object. `bpy.ops.object.bake(type='NORMAL')` followed by an image save is the basic call. — [Mateusz-Grzelinski/cycles-bake-workaround](https://github.com/Mateusz-Grzelinski/cycles-bake-workaround); [devtalk: bake progress](https://devtalk.blender.org/t/how-to-get-progres-info-from-ops-object-bake/9628)
- Blender has a built-in "Dirty Vertex Colors" function that darkens vertex colours by curvature (fake AO). It is under `bpy.ops.paint`. The fetched API page did not render the operator signature. — [Paint Operators, Blender Python API](https://docs.blender.org/api/current/bpy.ops.paint.html); [blender_vertex_color_master](https://github.com/andyp123/blender_vertex_color_master)

### Inferences
- Recommended invocation: `blender -b --factory-startup -noaudio -P gen_building.py -- --seed 42 --out out/bldg_042.glb`, with arguments read from `sys.argv[sys.argv.index("--")+1:]`. Use one process per asset, run in parallel with `xargs -P 6` or GNU parallel on the 8 cores. That also avoids the bake-memory leak. (Standard Blender CLI, prior knowledge.)
- To get the GN result as real geometry in background mode without operators, use `obj.evaluated_get(depsgraph).to_mesh()` or `bpy.data.meshes.new_from_object(eval_obj)`. Alternatively, `bpy.ops.object.modifier_apply(modifier=mod.name)` works inside a `bpy.context.temp_override(object=obj)` block. (Prior knowledge, unverified this session.)
- In recent Blender versions, GN modifier inputs are set by socket identifier (`mod["Socket_2"] = 5.0`), not by display name. Look up identifiers through `node_group.interface.items_tree`. (Prior knowledge, 4.0+ interface API, unverified this session.)
- For CPU-only Cycles baking, set `scene.cycles.device = 'CPU'` and use low samples (1–16). AO and curvature bakes on low-poly assets at 512–1024 px take seconds per asset. Vertex-colour AO and dirt avoid baking entirely and suit a Valheim-like look better than textures.
- Export: `bpy.ops.export_scene.gltf(filepath=..., export_format='GLB', use_selection=True)` or `bpy.ops.export_scene.fbx(..., mesh_smooth_type='FACE', add_leaf_bones=False)`. Both run headless. UE 5.8 imports glTF natively through Interchange, and FBX remains the safest option for foliage with vertex colours. (Prior knowledge.)

### Gaps
- No verified signature for `bpy.ops.paint.vertex_color_dirt` in Blender 5.x (page did not render). The legacy parameters were `blur_strength, blur_iterations, clean_angle, dirt_angle, dirt_only, normalize` (prior knowledge). It needs a vertex-paint context. Test under `temp_override` or use a numpy reimplementation (curvature from vertex normals vs. neighbour positions).
- Did not confirm whether any 5.x release changed operator-context rules for `modifier_apply` in background mode.
- Blender 5.x Asset Library scripting (`bpy.types.AssetMetaData`, `asset_mark()`) was not researched.

---

## 2. Procedural buildings (mudbrick, reed huts)

### Takeaway
No well-maintained headless CGA-style Python library turned up in this session. For this art style, the simplest path is custom bmesh/Geometry Nodes scripts: box massing, parapets, a flat roof with beam ends, doorway cutouts, and bevelled "soft" edges. Wave function collapse is only worth it for settlement layouts.

### Cited Findings
- No sources were fetched in this session for Building Tools, Buildify, CGA-in-Python or WFC. See Gaps.

### Inferences
- Mesopotamian mudbrick houses are geometrically simple: rectilinear volumes, flat roofs, few openings, courtyards. A split-grammar-lite in about 200 lines of Python (footprint → extrude → inset parapet → boolean doors, then randomised vertex jitter for a hand-made look) is cheaper than adopting a framework. This is an opinion based on the style constraints.
- Reed huts (mudhif-style arched reed bundles): generate arches as curves → `bpy.types.Curve` with bevel depth, or as bmesh ring sweeps along a parabola. Repeat along the hut length and add a mat skin.
- Modular kit route: model or generate 10–20 wall, corner and parapet pieces on a 50 cm or 1 m grid. Assemble them in UE with PCG (see §6) rather than in Blender. That keeps the geometry instanced (HISM / ISM).

### Gaps
- Building Tools (ranjian0/building_tools) headless operability and license, and Buildify (paid GN kit, Gumroad) license terms: not verified.
- CGA/split-grammar Python libraries (e.g. `pycga`, `procedural_city_generation`): not verified.
- WFC Python implementations and Townscaper/Tiny Glade GDC-talk details: not fetched.

---

## 3. Vegetation (date palms, tamarisk, reeds, crops), wind data, impostors

### Takeaway
`friggog/tree-gen` (GPL-3.0, Blender add-on) generates trees, and the author allows free use of generated models except for direct sale as assets. It is not documented as headless. Palms and reeds are simple enough to generate directly with bmesh: a trunk sweep plus frond cards along an L-system-like spiral. For impostors, the free Blender Impostor Baker add-on and its UE shaders exist but target Blender 3.1.

### Cited Findings
- tree-gen: "The code is under GPL-3.0 License, any models generated using the tool are free for use without restriction in any context apart from direct sale as assets." It installs as a Blender add-on. The README excerpt did not document headless or script usage. It includes `leaf.py` and `leaf_shapes.py`. — [friggog/tree-gen](https://github.com/friggog/tree-gen)
- The Blender Impostor Baker add-on creates impostor meshes and textures "with one click". It supports Blender 3.1.0 and ships UE 4.27/5 shader versions. — [Pandrodor/Blender-Impostor-Baker](https://github.com/Pandrodor/Blender-Impostor-Baker)
- Background on octahedral impostors (Ryan Brucks): [shaderbits.com](https://shaderbits.com/blog/octahedral-impostors). An open-source Godot implementation exists for reference: [wojtekpil/Godot-Octahedral-Impostors](https://github.com/wojtekpil/Godot-Octahedral-Impostors)

### Inferences
- tree-gen is a Blender add-on, so it can probably be driven headless: enable it with `addon_utils.enable("tree-gen")`, then call its operator or its internal `gen` function with a parameter preset. This needs testing. Its GPL code does not infect the generated meshes, per the author's statement above.
- Date palm recipe (bmesh): a slightly curved trunk (8–10 sided tube along a Bezier curve, with ring bumps for leaf scars), a crown of 20–30 fronds on a golden-angle spiral (137.5°), each frond a bent strip or two crossed cards with an alpha-tested leaflet texture, and date clusters as small spheres. This is low-poly and deterministic from a seed.
- Reeds, grass and crops: 1–3 quad cards per blade clump, instanced in UE. Generate an atlas of 8–16 blade silhouettes as a 2D numpy/PIL image.
- Wind vertex colours for UE foliage: bake a per-vertex gradient (R = height from pivot 0→1, G = branch/frond ID random, B = distance along frond) into a colour attribute in Blender and read it in a UE material with World Position Offset. UE's Pivot Painter 2 is the heavier alternative. (Prior knowledge, convention not verified this session.)
- Impostors in UE: UE 5.x has a built-in "Impostor Baker" plugin (Ryan Brucks) and Nanite foliage. For a low-poly game, plain LODs (LOD2 = 2 crossed cards) are likely enough. The Blender add-on's 3.1 target means expect breakage on 5.x.

### Gaps
- Sapling Tree Gen headless scripting: not verified (it is an operator with many parameters, so it probably works through `bpy.ops.curve.tree_add(...)`, but this is untested).
- Python space-colonization implementations: not fetched.
- UE 5.8 Impostor Baker plugin status and scriptability: not verified.

---

## 4. Terrain: heightmaps, erosion, canals and levees, UE import

### Takeaway
On Linux, plain numpy is the best-supported route. `dandrino/terrain-erosion-3-ways` (FBM, river networks and simulation-based erosion) is tested on Linux. Gaea's CLI "Build Swarm" is Windows-only (`Gaea.Build.exe`) and, per QuadSpinner, command-line use needs a paid Professional/Enterprise edition, so it doesn't fit the constraints. UE 5.8 heightmap import from Python works through `LandscapeProxy` methods: import from a render target, or the `landscape_import` method with raw uint16 data.

### Cited Findings
- terrain-erosion-3-ways uses Python and numpy and is "tested on OSX and Linux, but not on Windows". Methods: plain FBM, ridge noise, river networks (O(N² log N) for an N×N map) and simulation. — [dandrino/terrain-erosion-3-ways](https://github.com/dandrino/terrain-erosion-3-ways)
- The Gaea Build Swarm is `Gaea.Build.exe "file.tor"` with `--` switches: `--silent`, `--resolution####`, `--cpulimit##`, `--mutate##`, `--nodemap`, and variables as `var:value` after the switches. The executable is a Windows .exe. — [Gaea Build Swarm docs](https://docs.quadspinner.com/Guide/Build/Swarm.html)
- "Gaea can be used as a silent processor from other applications from the command line. The end-user must have a Professional or Enterprise edition of Gaea." The CLI can also enforce CPU-only mode. — [search summary of docs.gaea.app / quadspinner licensing](https://docs.gaea.app/getting-started/command-line-interface). The direct fetch returned 404, so this is from the search snippet only.
- UE heightmaps are unsigned 16-bit arrays (0–65535, 32768 is sea level). Landscapes can be filled from Python byte arrays. — [20tab UnrealEnginePython Landscape API](https://github.com/20tab/UnrealEnginePython/blob/master/docs/Landscape_API.md). Note: this is the legacy third-party plugin, not Epic's Python.
- Epic's `unreal.LandscapeProxy` exposes `landscape_import_heightmap_from_render_target()` and `landscape_export_heightmap_to_render_target()`. Supported render target formats are RTF_RGBA16f, RTF_RGBA32f and RTF_RGBA8, with an option to export height into the RG channels. — [unreal.LandscapeProxy (5.4 Python docs)](https://docs.unrealengine.com/5.4/en-US/PythonAPI/class/LandscapeProxy.html); [BP node doc 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Rendering/LandscapeImportHeightmapfromRend-)

### Inferences
- A floodplain like Mesopotamia is mostly flat. Large-scale erosion matters less than hand-authored-by-rule features. In numpy:
  - Base: low-amplitude FBM, then a gentle slope toward the river.
  - River: a spline polyline. Compute a distance field (`scipy.ndimage.distance_transform_edt` on a rasterised centreline), carve the channel with a smooth profile, and raise levees as a Gaussian bump at channel_width + 5–20 m.
  - Canals: straight or piecewise polylines off the river with a narrow trapezoid profile and small spoil banks both sides (same distance-field trick).
  - Dunes: anisotropic ridge noise (`abs(sin)` along a wind direction warped by FBM) masked to the desert edge.
  - Optional: a thermal-erosion pass (a few lines of numpy: move material where slope > talus angle) to soften edges.
- Splat maps by rule from the same fields: wet silt near water (distance < d1), farmland in canal-irrigated zones, sand where the dune mask is high, rocky ground on slope > θ. Write each as an 8-bit PNG weight map for UE Landscape layer import.
- Export heightmaps as 16-bit PNG or r16 at UE-friendly sizes (e.g. 1009, 2017, 4033 per side, following UE's recommended landscape sizes). Import the heightmap and layers either through the Landscape import UI once, or through Python with a render target or `landscape_import`. Verify method availability in 5.8.

### Gaps
- Exact UE 5.8 Python signature for creating a new Landscape actor with a heightmap from file: not verified. Forum threads suggest this is awkward. A fallback is a small C++ or Blueprint utility, or a one-time manual import followed by scripted updates.
- World Machine: Windows-only (prior knowledge), not researched.
- Gaea Community edition CLI terms: only the search snippet was available (docs page 404).

---

## 5. Procedural textures: Material Maker, baked Blender nodes, trim sheets, decals, vertex colour

### Takeaway
Material Maker (MIT, Godot-based) 1.5 brought back a batch CLI: `material_maker --export-material --target <Blender|Godot|Unity|Unreal> -o <out_dir> <files.ptex>`. It needs a real rendering context. Do not pass `--headless`. On a headless Linux box, that means running it under Xvfb or a virtual display with Mesa llvmpipe (software GL), which is inferred and untested. For stylized low-res textures, numpy/PIL and Blender procedural nodes baked on CPU are the dependable fallbacks.

### Cited Findings
- Material Maker CLI: "material_maker --export-material --target <engine> -o <output_path> <input_files>". Targets are Blender, Godot, Unity or Unreal. Input files are .ptex and wildcards are supported. — [Material Maker docs: Command line](https://rodzill4.github.io/material-maker/doc/command_line.html)
- Material Maker 1.5 (January 2026) "adds DDS, FBX, CLI". The CLI was reintroduced for scripted batch export. — [Digital Production](https://digitalproduction.com/2026/01/26/material-maker-1-5-adds-dds-fbx-cli-10-new-nodes/); [itch devlog 1.5](https://rodzilla.itch.io/material-maker/devlog/1312764/material-maker-15)
- Practitioner notes: "Use `--export-material`, not `--export`" (Godot 4 reserves `--export`). "Do not pass `--headless`; texture rendering needs a real rendering context." The checkout needs `steam_appid.txt` (4110830) or it self-relaunches and exits. The Linux code path is "untested". — [graysonchalmers/Tool-MaterialMaker-MCP](https://github.com/graysonchalmers/Tool-MaterialMaker-MCP)

### Inferences
- On Linux, try `xvfb-run -a ./material_maker --export-material --target Unreal -o out/ mats/*.ptex`. If the AMD GPU is unstable, force software GL with `LIBGL_ALWAYS_SOFTWARE=1` (Mesa llvmpipe). Slow, but CPU-only. Test this before building on it.
- Blender procedural textures (Noise, Voronoi, Wave, Brick nodes) baked with CPU Cycles, `bake_type='EMIT'` on an emission-wired material, give headless procedural textures without Material Maker. Brick and Voronoi nodes suit mudbrick and cracked mud.
- Stylized low-res look: generate at 256–512 px, quantise to a fixed palette (PIL `Image.quantize(palette=...)`), and set UE textures to `TF_Nearest` filter or keep low mips for pixel-crisp. For a Valheim-like look, prefer vertex colour plus small tiling textures over unique UVs.
- Trim sheets: one 1024×1024 texture split into horizontal bands (mudbrick course, plaster, reed-mat, wood beam, rope). Compose it in numpy/PIL from per-band generators. In Blender, UV-snap faces to band V ranges by face normal or material index during generation.
- Decal atlases: compose alpha-masked PNGs (cracks, water stains, soot, handprints) in a grid. In UE, one decal material with a SubUV index parameter per instance.
- Vertex-colour weathering without the operator: numpy over mesh attributes. Grime = f(1 − normal.z, height near ground, concavity). Write to `mesh.color_attributes.new("Col", 'BYTE_COLOR', 'POINT')`.

### Gaps
- No verified report of Material Maker CLI working on Linux under Xvfb or llvmpipe.
- Substance alternatives beyond Material Maker (e.g. ArmorLab, Blender "Principled" bake kits) were not researched.

---

## 6. UE 5.8 side automation (import, material instances, PCG, foliage, Niagara, commandlets)

### Takeaway
UE 5.8 runs Python two ways: the full editor with `-ExecutePythonScript=<file>`, or the headless commandlet `UnrealEditor-Cmd <proj> -run=pythonscript -script=<file_or_code>`. The commandlet does not auto-load levels. The Python plugin is still labelled Experimental in 5.8. PCG graphs can be created as assets from Python, and `PCGComponent.generate_local()` / `cleanup_local()` can be triggered. Wiring PCG nodes from Python is poorly documented, so author graphs by hand and drive them with parameters or data.

### Cited Findings
- 5.8 docs: `-ExecutePythonScript` needs the Editor Scripting Utilities plugin and loads the default startup level first. The commandlet form is `-run=pythonscript -script=<script_file_or_code>`. "This commandlet does not automatically load levels", so load one through LevelEditorSubsystem first. The feature is labelled "Experimental". No Linux-specific notes. — [Scripting the Unreal Editor Using Python, UE 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python)
- The commandlet runs "without editor opened… no asset and level loading (but you have access to the unreal python module)" and shuts down after the script. — [xingyulei: Unreal Python Command Line](https://www.xingyulei.com/post/ue-commandline-python/index.html); [Epic snippet: headless editor with Python](https://dev.epicgames.com/community/snippets/J5R1/unreal-engine-run-headless-unreal-editor-with-python-script)
- PCG graph asset creation: `asset_tools.create_asset('PCG_Test', "/Game/PCG_Graphs", unreal.PCGGraph, unreal.PCGGraphFactory())`. Connecting nodes such as Get Landscape Data → Surface Sampler from Python is "not well-documented". — [Epic forum: Create PCG Graph with Python](https://forums.unrealengine.com/t/create-pcg-graph-with-python/1714891); [unreal.PCGGraph](https://docs.unrealengine.com/5.2/en-US/PythonAPI/class/PCGGraph.html)
- `unreal.PCGComponent` exposes `generate()`, `generate_local()`, `cleanup()` and `cleanup_local()`. — [unreal.PCGComponent 5.4](https://docs.unrealengine.com/5.4/en-US/PythonAPI/class/PCGComponent.html); [UPCGComponent::Cleanup 5.8](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PCG/UPCGComponent/Cleanup)
- UE 5.8 has PCG overview and generation-mode docs. — [PCG Overview 5.8](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview); [PCG Generation Modes 5.8](https://dev.epicgames.com/documentation/unreal-engine/using-pcg-generation-modes-in-unreal-engine)

### Inferences
- On Linux the binary is `Engine/Binaries/Linux/UnrealEditor-Cmd`. Suggested command: `UnrealEditor-Cmd /path/Game.uproject -run=pythonscript -script=/abs/import_assets.py -unattended -nosplash -nullrhi -stdout`. `-nullrhi` avoids touching the flaky GPU for import-only jobs; do not use it for anything that renders (thumbnails, render targets, landscape RT import). (Standard UE flags, prior knowledge.)
- Import: `unreal.AssetImportTask` (filename, destination_path, automated=True, replace_existing=True) passed to `unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([...])`. Material instances: `create_asset(name, path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())`, then `unreal.MaterialEditingLibrary.set_material_instance_parent / ..._scalar_parameter_value`. (Prior knowledge, API names stable since 4.2x.)
- PCG strategy: build 3–5 master graphs by hand (scatter palms along the river-distance band, reeds at the water edge, fields in canal zones, building kit on plot footprints). Feed them from Python through PCG DataTable/Attribute Set inputs or spline actors placed by script, then call `generate_local`. Generation that needs the landscape loaded requires the full editor (`-ExecutePythonScript`), not a bare commandlet.
- Niagara from Python: creating systems from scratch is impractical. The feasible pattern is duplicating a template system asset (`EditorAssetLibrary.duplicate_asset`) and swapping user parameters or the flipbook texture. (Inference, not verified.)

### Gaps
- No Linux-specific confirmation of Python commandlet behaviour in 5.8, or of whether PCG generation works under `-nullrhi`.
- Scripted foliage painting (`InstancedFoliageActor` from Python) was not researched. PCG plus HISM is the modern replacement.

---

## 7. Particles/VFX flipbooks

### Takeaway
No sources fetched. Stylized smoke, dust and fire flipbooks can be generated CPU-only in numpy (animated FBM noise × radial falloff, thresholded to a few tones for a toon look) or in Blender (Mantaflow smoke, CPU, rendered orthographically into a grid with EEVEE/Cycles).

### Cited Findings
- None fetched this session.

### Inferences
- Numpy path: for frame t in 0..N-1, compute `density = fbm(x, y + t·v, seed) · radial_falloff(r·(1 + t·k))`, quantise to 3–4 steps, tile into an 8×8 atlas PNG, and use it in UE's SubUV / Niagara "Sub UV Animation" module. This runs in seconds, needs no GPU, and gives a pixel-crisp look.
- Blender Mantaflow bakes on CPU in background mode, but it is slow and produces realistic results that then need stylising. Probably overkill for this style.

### Gaps
- Free CC0 VFX flipbook sources (e.g. Kenney particle pack, the Epic Content Examples project) are not verified this session.
- GDC talks (Valheim, Townscaper, Tiny Glade) on procedural stylized content were not fetched.
