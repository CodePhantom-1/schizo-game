# Local AI Generation of Game Assets on RX 5700 XT (gfx1010, 8 GB) / CPU — 2026

Scope: what can run locally on Ubuntu 24.04 + RX 5700 XT (RDNA1) + i7-7700K for a low-poly stylized UE game; licenses of models and outputs; recommendation. Research date 2026-09-26. Local-box constraint: the 5700 XT has shown GPU hangs under heavy Vulkan loads (UE 5.8) and an unresolved gfx command-processor hang (see memory `project-rx5700xt-gfxoff-fix`, `project-local-llm-rx5700xt`), so long GPU jobs should run headless with the display on the Intel iGPU.

## 1. ROCm status for gfx1010 and the non-ROCm alternatives (Vulkan, CPU)

### Takeaway
RDNA1/gfx1010 is still not in AMD's official ROCm support list, but in 2026 AMD's TheRock builds publish gfx1010 PyTorch wheels (a `device-gfx1010` extra) and community wheels exist. SD 1.5 works at roughly 1.5 it/s at 512x512. It is a second-class target: rocBLAS-only GEMM, no hipBLASLt, no composable_kernel, no fused attention, and newer models (Flux, Z-Image) are broken or need workarounds. stable-diffusion.cpp on Vulkan (MIT) avoids ROCm entirely and is the lower-friction path.

### Cited Findings
- TheRock (AMD's open ROCm build system) has an open tracking issue for "RDNA1 (gfx1010) and Vega iGPU (gfx90c) current issues and status", tested with ComfyUI and nightly PyTorch wheels — [TheRock #7609](https://github.com/ROCm/TheRock/issues/7609)
- In that testing on an RX 5600M (gfx1010): SD 1.5 works on both the Triton and Eager backends at 512x512, ~1.5 it/s. Z-Image Turbo works only on Eager. Flux/Flux2 give "gray garbled images" on Triton and work on Eager. fp16 is fully accelerated; bf16 upcasts to fp32; GGUF Q4/Q8 dequantize to fp16/fp32; fp8 upcasts to fp16 — [TheRock #7609](https://github.com/ROCm/TheRock/issues/7609) (the fetched page also reported "ROCm Version 10.1", which looks like a mis-extraction. Treat it as unverified.)
- int8 paths fail: hipBLASLt has no Tensile library for gfx1010 ("Cannot read TensileLibrary_lazy_gfx1010.dat"), and ROCBLAS_TENSILE_LIBPATH/HIPBLASLT_TENSILE_LIBPATH workarounds don't help — [TheRock #7609](https://github.com/ROCm/TheRock/issues/7609)
- TheRock leaves composable_kernel, hipBLASLt and rocWMMA out of gfx101X builds, so "even a working gfx1010 torch has rocBLAS-only GEMM and no fused attention" — [unsloth PR #9138](https://github.com/unslothai/unsloth/pull/9138) (search snippet)
- TheRock's multi-arch PyTorch index has `[device-*]` extras, including device-gfx1010, with a torch 2.11–2.14 compatibility matrix — [TheRock RELEASES.md](https://github.com/ROCm/TheRock/blob/main/RELEASES.md) (search snippet); [TheRock multi-arch wheels issue #3565](https://github.com/ROCm/TheRock/issues/3565)
- ComfyUI + RDNA1 community test thread — [TheRock Discussion #3167](https://github.com/ROCm/TheRock/discussions/3167)
- Community prebuilt wheels: Torch 2.10 / TorchVision 0.25 / TorchAudio 2.10 for Python 3.13 + ROCm 7.2 targeting gfx101x (March 2026) — [KenshiN28rus/comfyui-rocm-gfx1010 v1.0.0](https://github.com/KenshiN28rus/comfyui-rocm-gfx1010/releases/tag/v1.0.0); a build-from-source guide for PyTorch 2.10 with gfx1010 on Debian 13 — [Efenstor/PyTorch-ROCm-gfx1010-Debian13](https://github.com/Efenstor/PyTorch-ROCm-gfx1010-Debian13)
- stable-diffusion.cpp (MIT) backends: CPU (AVX/AVX2/AVX512), CUDA, Vulkan, Metal, OpenCL, SYCL. Models: SD 1.x/2.x, SDXL, SD3/3.5, FLUX.1/FLUX.2, Qwen-Image, Z-Image, Chroma, and others. Features: LoRA, VAE tiling, Flash Attention, TAESD fast decode, ControlNet (SD 1.5 only), IP-Adapter (SD1.5/SDXL), ESRGAN upscaling. The CLI is `sd-cli` — [stable-diffusion.cpp README](https://github.com/leejet/stable-diffusion.cpp)
- AMD's official PyTorch-on-ROCm install doc (the supported-GPU path) — [ROCm docs](https://rocm.docs.amd.com/projects/install-on-linux/en/latest/install/3rd-party/pytorch-install.html)

### Inferences
- The old `HSA_OVERRIDE_GFX_VERSION=10.3.0` trick (making gfx1010 pretend to be RDNA2 gfx1030) is superseded now that native gfx1010 kernels exist in TheRock/community wheels. The override makes rocBLAS load gfx1030 ISA code, which RDNA1 does not fully implement, so it risks crashes and hangs. On a card that already hangs, avoid it.
- Every 3D-gen repo below needs CUDA-only compiled extensions. Having ROCm torch does not unblock them. ROCm on gfx1010 is only really useful for ComfyUI 2D workflows.
- On this box, stable-diffusion.cpp + Vulkan (same RADV stack as the working llama.cpp setup, pinned to the dGPU with its device-selection env var) is the most practical 2D generator. The Intel HD 630 and CPU are fallbacks. CPU works but is slow: expect minutes per SD1.5/SDXL image on a 4-core i7-7700K (not measured, see Gaps).
- The GPU-hang risk is real, so use short batch jobs, headless mode, and small resolutions (SD1.5/SDXL at ≤1024 with VAE tiling).

### Gaps
- I found no published it/s benchmarks for an RX 5700 XT on stable-diffusion.cpp Vulkan, or for i7-7700K CPU-only SD. Measure locally.
- I found no 2025–2026 primary source quantifying how reliable HSA_OVERRIDE_GFX_VERSION=10.3.0 is on RDNA1. The claims about it above are inference.

## 2. Seamless/tileable textures, image-to-PBR, text-to-image for concepts/decals

### Takeaway
Tileable textures are well solved locally. Circular padding (the `--circular/--circularx/--circulary` flags in stable-diffusion.cpp, or ComfyUI seamless-tiling nodes) on SD1.5/SDXL is enough for stylized textures. StableMaterials (openrail, 0.9B, 512px, 5 PBR maps, tileable, 4-step LCM) is the one dedicated open PBR generator that fits in 8 GB. DeepBump (GPL, ONNX, runs on CPU) turns an albedo into normal/height maps.

### Cited Findings
- stable-diffusion.cpp supports `--circular`, `--circularx`, `--circulary` for seamless/tiling output. PR #2036 (circular RoPE for Qwen-Image) was closed 2026-09-23 as superseded by PR #2039, which turns circular handling into a generic post-processing step — [sd.cpp PR #2036](https://github.com/leejet/stable-diffusion.cpp/pull/2036)
- Circular padding (`padding_mode='circular'` patched into Conv2d) makes SD generate tiling images in one pass. The trade-off is slightly lower quality and less "structured" compositions — [Sygil-Dev PR #911](https://github.com/Sygil-Dev/sygil-webui/pull/911); [CompVis/stable-diffusion #250](https://github.com/CompVis/stable-diffusion/issues/250); [ComfyUI seamless tiling node](https://www.runcomfy.com/comfyui-nodes/ComfyUI-seamless-tiling)
- Using SD for game tilesets in practice — [BorisTheBrave, 2025](https://www.boristhebrave.com/2025/02/04/generating-tilesets-with-stable-diffusion/). Research alternative: Tiled Diffusion — [arXiv 2412.15185](https://arxiv.org/html/2412.15185v1)
- StableMaterials: "openrail" license; 0.9B params; outputs basecolor, normal, height, roughness and metallic at 512x512; tileability comes from rolling feature maps in every conv/attention layer; LCM variant in 4 steps (vs 50); text or image prompts — [HF gvecchio/StableMaterials](https://huggingface.co/gvecchio/StableMaterials). Trained on MatSynth + Deschaintre et al. (6,198 materials) plus 4,000 SDXL-generated texture/text pairs — [arXiv 2406.09293](https://arxiv.org/html/2406.09293v3). Published at CVPR 2026 — [CVF open access](https://openaccess.thecvf.com/content/CVPR2026/html/Vecchio_StableMaterials_Enhancing_Diversity_in_Material_Generation_via_Semi-Supervised_Learning_CVPR_2026_paper.html)
- Newer research on PBR latent spaces (MatLat, Dec 2025) — [arXiv 2512.17302](https://arxiv.org/pdf/2512.17302). ComfyUI PBR helper node pack — [smthemex/ComfyUI_PBR_Maker](https://github.com/smthemex/ComfyUI_PBR_Maker)
- DeepBump: a MobileNetV2 U-Net that predicts normals (then height/curvature) from one picture. It ships as a Blender add-on and a CLI, runs on ONNX Runtime (CPUExecutionProvider works), model `deepbump256.onnx`, license GPL (v3) — [DeepBump repo](https://github.com/HugoTini/DeepBump); [project page](https://hugotini.github.io/deepbump). A secondary source says the GPLv3 model "matters if you're shipping the maps" — search snippet, unverified. GPL does not normally reach program output, so this claim is doubtful.

### Inferences
- For low-poly stylized work, much of the "PBR" is flat albedo plus a mild normal map, so SD1.5/SDXL + circular tiling + a stylized LoRA + DeepBump (or plain procedural normal-from-height in Substance Designer/Material Maker/Blender) covers most needs.
- Base-model licenses decide output use. SD 1.5 and SDXL are CreativeML OpenRAIL-M: commercial output allowed, use restrictions only. That is from memory and not re-verified here. SD3/3.5 fall under the Stability Community License ($1M cap, see §4).
- Material Palette, ControlMat, DreamMat, Marigold-normals: not researched within the call budget, see Gaps.

### Gaps
- Licenses and VRAM for Material Palette (CVPR 2024), ControlMat (Adobe), DreamMat, and Marigold normals weights were not verified.
- Does StableMaterials run on ROCm-gfx1010 torch or CPU at acceptable speed? It is a diffusers-style pipeline with no CUDA-only ops noted, but I did not test this.

## 3. Image/text-to-3D on this hardware

### Takeaway
None of the strong 2025–2026 image-to-3D models (TRELLIS.2, Hunyuan3D 2.x texture stage, InstantMesh) runs on a gfx1010 AMD card. All need CUDA-only extensions, and TRELLIS.2 needs a 24 GB NVIDIA GPU. The realistic local options are TripoSR (MIT, ~6 GB, has a CPU path) and Stable Fast 3D (Stability Community License, ~6 GB, `SF3D_USE_CPU=1`, built-in quad/triangle remesh with target vertex count). Both run on CPU, slowly. Hunyuan3D-2 shape-only (6 GB) is geographically licensed and excludes the EU/UK/South Korea, but not Turkey.

### Cited Findings
- **TRELLIS.2** (Microsoft): "An NVIDIA GPU with at least 24GB of memory is necessary". Tested only on Linux with A100/H100. Dependencies: flash-attn (or xformers), nvdiffrast, nvdiffrec, CuMesh, O-Voxel, FlexGEMM. Speed on H100: 512³ ≈ 3 s, 1024³ ≈ 17 s, 1536³ ≈ 60 s. Output: GLB with PBR (base color, roughness, metallic, opacity). Code and model under MIT, but nvdiffrast/nvdiffrec have separate licenses — [microsoft/TRELLIS.2](https://github.com/microsoft/TRELLIS.2)
- **Hunyuan3D-2**: "6 GB VRAM for shape generation and 16 GB for shape and texture generation in total". Has a `--low_vram_mode` flag. The texture stage needs CUDA-compiled `custom_rasterizer` and `differentiable_renderer`, with no AMD/CPU alternative documented. Variants range from 0.6B to 3.0B with Turbo (step-distilled) and Fast (guidance-distilled) versions. Includes a Blender add-on via an API server — [Tencent-Hunyuan/Hunyuan3D-2](https://github.com/Tencent-Hunyuan/Hunyuan3D-2)
- **Hunyuan3D 2.1 license**: "Territory" = worldwide "excluding the territory of the European Union, United Kingdom and South Korea". If the product exceeds 1M MAU, a commercial license from Tencent is required. "Tencent claims no rights in Outputs You generate". Outputs may not be used to improve any other AI model — [Hunyuan3D-2.1 LICENSE](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1/blob/main/LICENSE)
- **TripoSR** (VAST/Stability): MIT covers code and pretrained weights. ~6 GB VRAM by default. <0.5 s per image on an A100. GPU and CPU paths. Output is a vertex-colored mesh by default, or a textured mesh with `--bake-texture`. Depends on `torchmcubes`, whose CUDA build can fail to compile on a CUDA mismatch — [VAST-AI-Research/TripoSR](https://github.com/VAST-AI-Research/TripoSR)
- **Stable Fast 3D (SF3D)**: ~6 GB VRAM; CPU fallback with `SF3D_USE_CPU=1`; experimental Mac MPS. Texture baker and UV unwrapper use CUDA kernels (Metal kernels for MPS). Outputs GLB with configurable texture resolution. Remesh options: none / triangle / quad, with a target vertex count. Gated on Hugging Face. Stability AI Community License — [Stability-AI/stable-fast-3d](https://github.com/Stability-AI/stable-fast-3d)

### Inferences
- **Turkey and Hunyuan3D:** Turkey is not in the EU, UK or South Korea, so a developer based in Turkey is inside the licensed Territory. The catch: the game ships worldwide, including to EU/UK players. The license restricts where the model is used and deployed. Distributing outputs (meshes) into the EU is arguably fine, since Tencent claims no rights in outputs, but some legal reviewers read the territory clause conservatively. This is an inference; get legal review if the game gets big.
- On AMD gfx1010 the practical chain is TripoSR or SF3D on **CPU**. SF3D's CPU path skips the CUDA texture baker, and TripoSR can output vertex colors, which suits flat-shaded low-poly.
- Then clean up in Blender: Decimate or QuadriFlow/Instant Meshes, re-UV, and hand-paint or bake to a palette texture. Stylized low-poly props usually need retopology anyway, because AI meshes are dense triangle soup.
- Honestly, for low-poly props, hand modeling or kitbashing CC0 packs (Kenney, Quaternius, Poly Pizza) is often faster than cleaning up AI meshes.
- Cloud fallback: running TRELLIS.2/Hunyuan3D on a rented 24 GB NVIDIA GPU is the only way to use the high-quality 2025–2026 models. Not local, but licenses allow it: TRELLIS.2 is MIT; Hunyuan is usable from Turkey.

### Gaps
- InstantMesh license/VRAM, SPAR3D requirements, and Hunyuan3D-2mini/2.1 shape VRAM were not fetched within the call budget. From memory: InstantMesh is Apache-2.0 and needs nvdiffrast; SPAR3D uses the Stability Community License. Both unverified.
- No published CPU timings for TripoSR/SF3D on a 4-core desktop CPU; expect minutes, not seconds (not measured).
- Whether the Hunyuan3D-2.0 (not 2.1) license has the same territory clause was not re-fetched. It is widely reported to, but that is unverified here.

## 4. Licensing: model licenses, output rights, Steam AI disclosure

### Takeaway
Output ownership is favorable across the board (Stability and Tencent both disclaim rights in outputs). The binding constraints are the Stability Community License's US$1M annual revenue cap, Hunyuan's territory and 1M MAU clauses (Turkey is fine), and non-commercial audio weights (AudioLDM 2 is CC-BY-NC-SA; AudioCraft/AudioGen is CC-BY-NC). Since 16 Jan 2026, Steam requires disclosing pre-generated AI content that ships to players; dev-efficiency tools are exempt.

### Cited Findings
- Stability AI Community License: free for research, non-commercial and commercial use for organizations with annual revenue ≤ US$1M. Above that, an Enterprise license is required. No limits on how much you generate — [Stability AI license update](https://stability.ai/news/license-update); [Stability AI License](https://stability.ai/license)
- Under that license "you are the owner of derivative works you create", subject to Stability's ownership of its own materials — [Terms.Law summary](https://terms.law/ai-output-rights/stable-diffusion/) (secondary)
- Hunyuan3D 2.1: territory excludes EU/UK/South Korea; 1M MAU trigger; no Tencent rights in outputs; outputs can't be used to train other models — [LICENSE](https://github.com/Tencent-Hunyuan/Hunyuan3D-2.1/blob/main/LICENSE)
- TRELLIS.2 is MIT — [repo](https://github.com/microsoft/TRELLIS.2). TripoSR is MIT — [repo](https://github.com/VAST-AI-Research/TripoSR). UniRig is MIT — [LICENSE](https://github.com/VAST-AI-Research/UniRig/blob/main/LICENSE). stable-diffusion.cpp is MIT — [repo](https://github.com/leejet/stable-diffusion.cpp). StableMaterials is openrail — [HF](https://huggingface.co/gvecchio/StableMaterials). DeepBump is GPL — [repo](https://github.com/HugoTini/DeepBump)
- Steam: Valve rewrote the AI disclosure form on 2026-01-16. It now focuses on AI content that "ships with your game, and is consumed by players", and says efficiency gains from AI dev tools (e.g. code helpers) are "not the focus". There are two categories: **pre-generated** (content made with AI tools) and **live-generated** (made at runtime, which needs guardrails, plus an overlay button for players to report illegal content) — [PC Gamer](https://www.pcgamer.com/software/ai/steam-updates-ai-disclosure-form-to-specify-that-its-focused-on-ai-generated-content-that-is-consumed-by-players-not-efficiency-tools-used-behind-the-scenes/); [BigGo summary](https://finance.biggo.com/news/202601171220_Steam_AI_Disclosure_Update_Focuses_on_Player_Content). Original Jan 2024 policy reversing the de facto ban — [AlternativeTo](https://alternativeto.net/news/2024/1/valve-reverses-ai-content-ban-on-steam-and-introduces-new-updated-guidelines)
- About 20% of Steam games carried an AI disclosure in 2026 — [tech-insider.org](https://tech-insider.org/steam-ai-disclosure-2026/) (low-quality secondary source, figure unverified)

### Inferences
- Any AI-made texture, mesh or SFX that ships in the game has to be disclosed as "pre-generated" on the Steam store page. Using AI only for concept art that is then redrawn or modeled by hand is arguably exempt under the "consumed by players" wording. That is a judgment call; disclose if in doubt.
- In the US, pure AI output has weak copyright protection. That is background knowledge, not researched here. Assets you can't protect may matter less for a game than disclosure does.
- Training-data concerns: SD 1.5/SDXL were trained on LAION web scrapes (reputational/legal risk). Stable Audio Open was trained on CC-licensed Freesound/FMA (lower risk). StableMaterials partly distils SDXL output.

### Gaps
- Whether Turkish law adds any AI-content obligations was not researched.
- The CreativeML OpenRAIL-M terms for SD1.5/SDXL were not re-fetched.

## 5. AI SFX and AI rigging/animation

### Takeaway
Stable Audio Open 1.0 is the only strong commercially usable local SFX model: Stability Community License (free under $1M revenue), 1B params, up to 47 s stereo at 44.1 kHz, better at SFX/field recordings than music, trained on CC-licensed Freesound/FMA. AudioLDM 2 and Meta's AudioGen weights are non-commercial, so don't ship their output. For rigging, UniRig (MIT, SIGGRAPH 2025) is the open auto-rigger, but its runtime needs were not verified.

### Cited Findings
- Stable Audio Open 1.0: Stability AI Community License; up to 47 s stereo at 44.1 kHz; latent diffusion transformer, 1B params (F32). Training data is 486,492 recordings: 472,618 from Freesound (CC0, CC-BY, CC Sampling+) and 13,874 from FMA, screened with PANNs and Audible Magic for copyrighted music. "Better at generating sound effects and field recordings than music". No realistic vocals — [HF model card](https://huggingface.co/stabilityai/stable-audio-open-1.0); [paper](https://arxiv.org/html/2407.14358v2)
- AudioLDM 2 weights: CC-BY-NC-SA-4.0 (non-commercial) — [HF cvssp/audioldm2](https://huggingface.co/cvssp/audioldm2); [AudioLDM2 repo](https://github.com/haoheliu/AudioLDM2)
- AudioCraft (AudioGen/MusicGen) weights: CC-BY-NC — [audiocraft issue #198](https://github.com/facebookresearch/audiocraft/issues/198). AudioGen/AudioLDM are trained on AudioSet, whose audio has no disclosed licenses — [Stable Audio Open paper](https://arxiv.org/html/2407.14358v2)
- UniRig: Tsinghua + Tripo (VAST), SIGGRAPH 2025, MIT license. An autoregressive model predicts the skeleton and skinning weights. Released checkpoint is trained on Articulation-XL2.0; the Rig-XL/VRoid checkpoints were still "pending" at the time of the source — [VAST-AI-Research/UniRig](https://github.com/VAST-AI-Research/UniRig); [ComfyUI wiki](https://comfyui-wiki.com/en/news/2025-04-12-unirig-one-model-to-rig-them-all); [ACM TOG](https://dl.acm.org/doi/abs/10.1145/3730930)

### Inferences
- Stable Audio Open at 1B params should fit in 8 GB, or run on CPU slowly (not measured). SFX go through stable-audio-tools / diffusers, which needs ROCm torch on gfx1010 or CPU.
- A plain alternative: CC0 SFX libraries (Freesound CC0 filter, Kenney audio, Sonniss GDC bundles, which are royalty-free rather than CC0) plus procedural tools (sfxr/jsfxr, ChipTone). No disclosure question for those.
- For rigging: humanoids go to Blender Rigify, or Mixamo (cloud; Adobe account). UniRig is worth a test only for non-humanoid creatures, and probably needs CUDA (unverified).

### Gaps
- Stable Audio Open Small (a 2025 smaller variant) was not verified. Actual VRAM/CPU timing for Stable Audio Open on this box is unknown.
- UniRig VRAM/CUDA dependencies weren't fetched. Other AI animation (MoMask, MDM, Tencent HY-Motion) wasn't researched.

## 6. Recommendation for this machine

### Takeaway
Worth integrating locally: (1) stable-diffusion.cpp on Vulkan (fallback: CPU) with circular tiling for stylized textures, decals and concept art; (2) DeepBump or StableMaterials for normal/PBR maps; (3) Stable Audio Open for SFX drafts. Treat image-to-3D as optional: TripoSR/SF3D on CPU for blockout reference only, or rent cloud NVIDIA for TRELLIS.2. Rely on procedural and CC0 assets for final low-poly meshes, rigs and most SFX.

### Cited Findings
- 2D generation works on this hardware class: SD1.5 at ~1.5 it/s on gfx1010 via TheRock, plus the Vulkan/CPU backends of sd.cpp — [TheRock #7609](https://github.com/ROCm/TheRock/issues/7609); [sd.cpp](https://github.com/leejet/stable-diffusion.cpp)
- The high-end 3D models are out of reach: TRELLIS.2 needs 24 GB NVIDIA; the Hunyuan3D texture stage needs 16 GB plus CUDA rasterizers — [TRELLIS.2](https://github.com/microsoft/TRELLIS.2); [Hunyuan3D-2](https://github.com/Tencent-Hunyuan/Hunyuan3D-2)
- CPU-capable 3D: SF3D (`SF3D_USE_CPU=1`, quad remesh with target vertex count) and TripoSR (CPU path, MIT) — [SF3D](https://github.com/Stability-AI/stable-fast-3d); [TripoSR](https://github.com/VAST-AI-Research/TripoSR)

### Inferences
- Priority order by value per setup hour on this box: sd.cpp Vulkan textures (high) > DeepBump normals (high, trivial, CPU) > Stable Audio Open SFX (medium) > StableMaterials (medium) > TripoSR/SF3D CPU (low for low-poly) > ROCm ComfyUI (low: fragile on gfx1010, hang risk) > 3D rigging AI (skip).
- License-safe default set for a commercial Steam release from Turkey: sd.cpp (MIT) with SD1.5/SDXL (OpenRAIL-M), DeepBump, Stable Audio Open and SF3D (Stability Community, fine under $1M revenue), TripoSR/TRELLIS.2/UniRig (MIT). Avoid AudioLDM 2 and AudioGen. Hunyuan3D is allowed from Turkey but carries EU/UK territory ambiguity.
- Disclose every shipped AI asset as "pre-generated" on Steam.

### Gaps
- No local benchmarks yet. First step: time sd.cpp Vulkan SD1.5 512² on the 5700 XT (headless) and on CPU, and check for hangs.
