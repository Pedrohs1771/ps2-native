# Continuação com ELF e fixtures próprias

No PC original foi executada a extração de `SYSTEM.CNF`, `SLES_533.80` e dos
nove arquivos de `MODULES/` de Metal Slug 4. O diretório privado resultante é
`fixtures/metal_slug_4/`: 11 arquivos, 6.880.516 bytes. O ELF foi identificado
como ELF32 little-endian MIPS e analisado com sucesso pelo `ps2_analyzer`.
O [recibo estático](../test-data/metal-slug-4-static-analysis.json) contém
tamanhos, hashes e contadores; **não contém os binários do jogo**.

## Análise direta existente

O analisador já recebe ELF diretamente. Não é necessário alterar `convert`
nem montar uma ISO para a análise estática. Num ambiente onde os binários
foram fornecidos legalmente, execute na raiz do checkout:

```sh
file fixtures/metal_slug_4/SLES_533.80
mkdir -p build/elf-analysis
./build/ps2xAnalyzer/ps2_analyzer \
  fixtures/metal_slug_4/SLES_533.80 build/elf-analysis/analysis.toml
```

Use um diretório novo por rodada para preservar os recibos. O target CMake
do analisador é `ps2_analyzer`; as dependências estão no guia da VPS.
O TOML produzido pode ser fornecido a `./build/ps2xRecomp/ps2_recomp`, conforme
o workflow de recompilação existente. Isso gera C++; não comprova execução,
natividade integral ou funcionamento dos menus.

O levantamento local configurou 220 stubs conhecidos e 370 entry points,
detectou 360 funções de biblioteca sem handler correspondente, três tabelas
de salto e dez patches potenciais. São resultados do scanner: não transformar
esses números numa lista comprovada de bloqueios, nem aplicar patches sem
reprodução do contrato. Nenhuma função foi adicionada a `skip`.

## Instrução para o agente Cloud

> Não baixe ISOs externas. Leia AGENTS.md, docs/VPS_CONTINUACAO.md,
> docs/PROMPT_CONTINUAR_CODEX.txt e o recibo estático acima. Verifique se os
> arquivos reais existem antes de tentar análise por ELF: o clone contém
> somente os metadados do jogo. Se eles estiverem ausentes, prossiga com as
> fixtures MIPS próprias de lab/tests/ e os testes de kernel/runtime.
> Priorize os contratos compartilhados de scheduler, GS, SIF/IOP e SPU2.
> Reproduza cada divergência com código próprio, corrija e teste. Não invente
> bytes do jogo, stubs de sucesso ou aprovação de menus.

ELF e módulos isolados permitem inspeção estática. Menus, música e navegação
reais podem carregar arquivos adicionais do disco; a ausência de mídia não
qualifica esse percurso. O estado comercial permanece 0/5 menus aprovados.
Para testes sem binários comerciais, use os comandos das fixtures próprias
em [VPS_CONTINUACAO.md](VPS_CONTINUACAO.md).
