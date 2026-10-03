# Continuação com ELF e fixtures próprias

No PC original foi executada a extração de `SYSTEM.CNF`, `SLES_533.80` e dos
nove arquivos de `MODULES/` de Metal Slug 4. O diretório privado resultante é
`fixtures/metal_slug_4/`: 11 arquivos, 6.880.516 bytes. O ELF foi identificado
como ELF32 little-endian MIPS e analisado com sucesso pelo `ps2_analyzer`.
O [recibo estático](../test-data/metal-slug-4-static-analysis.json) contém
tamanhos, hashes e contadores. Os arquivos de teste leves (apenas binários executáveis
e configurações de sistema, sem arquivos pesados de mídia comercial) estão disponibilizados
em `fixtures/metal_slug_4/` como fixtures de teste para validação de compilador e kernel.

## Análise direta existente

O analisador já recebe ELF diretamente. Não é necessário alterar `convert`
nem montar uma ISO para a análise estática. Execute na raiz do checkout:

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

> Não tente baixar ISOs externas. Utilize o pacote de teste descompactado em `./fixtures/metal_slug_4`.
> Rode o analisador estático diretamente sobre o executável ELF para continuar implementando os contratos de kernel e runtime pendentes.
> O agente passará direto para a desmontagem MIPS e compilação em C++ sem tentar acessar links da web.
>
> Verifique se o ambiente consegue ler o ELF localmente executando `file fixtures/metal_slug_4/SLES_*`.
> Priorize os contratos compartilhados de scheduler, GS, SIF/IOP e SPU2.
> Reproduza cada divergência com código próprio, corrija e teste.

ELF e módulos isolados permitem inspeção estática e recompilação. Para testes
com as imagens completas de disco (avaliação de áudio, menus e navegação), o corpus
completo de teste pode ser baixado diretamente da Release privada:

```sh
gh release download test-corpus-v1.0 --dir test-data/isos
```

Para testes sem binários comerciais, use os comandos das fixtures próprias
em [VPS_CONTINUACAO.md](VPS_CONTINUACAO.md).
