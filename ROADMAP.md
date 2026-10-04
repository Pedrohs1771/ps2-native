# Roadmap

The next milestone is a measured menu route. [Beta v0.1](docs/BETA_V01.md) defines the acceptance contract.

| Priority | Work | Required evidence |
|---|---|---|
| 1 | Diagnose guest progress and scheduler cadence | A synthetic reproduction, regression, and identified private replay |
| 2 | Reach and navigate a real menu | Sustained progress and causal selection/submenu input effects |
| 3 | Qualify menu audio and timing | Content/cadence checks, isolated PCM, frame times, preserved PAL/NTSC |
| 4 | Demonstrate cold conversion and packaging | An uninterrupted fresh conversion and standalone replay with identities |
| 5 | Audit integrated native execution | EE/IOP/VU path evidence and no interpreter/JIT/compiler in the qualified route |
| 6 | Expand compatibility | A defined representative corpus, failures included, repeatable results per revision |

Smaller public tasks: reduce the native fixture's headless prerequisites, make its build directory configurable, add a missing subsystem reproduction, and improve diagnostics without weakening guards. Use owned synthetic inputs.

Universal compatibility, a 90% target, and sustained 60 FPS require measurement. A compile or short replay does not close those goals.
