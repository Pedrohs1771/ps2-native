# Análise direta de ELF

O analisador aceita ELF diretamente, sem montar ISO. O checkout não fornece executáveis comerciais.

Com um ELF próprio ou insumo privado autorizado, fora do Git:

```sh
mkdir -p build/elf-analysis
./build/ps2xAnalyzer/ps2_analyzer /path/to/owned.elf build/elf-analysis/analysis.toml
```

O alvo CMake é `ps2_analyzer`; a saída pode alimentar `ps2_recomp`. Análise/geração não qualificam execução, natividade integral ou menus.

Os antigos diretórios `fixtures/<jogo>/` e instruções de download de corpus foram retirados da preparação pública. `test-data/metal-slug-4-static-analysis.json` contém metadados históricos. Use [REVIEWER_GUIDE.md](REVIEWER_GUIDE.md) para uma demonstração sem conteúdo comercial.

Priorize contratos compartilhados de scheduler, GS, SIF/IOP e SPU2. Reduza divergências a dados próprios; não publique ELFs/IRX extraídos, RAM/VRAM ou fontes geradas de jogos.
