# Retomada na VPS — versão experimental de 03/10/2026

Leia `AGENTS.md`, `ADAPTACAO_AUTONOMA_20261003.md` e
`PROMPT_CONTINUAR_CODEX.txt`. O caminho local citado nos recibos é histórico;
use o diretório do clone na VPS. Trabalhar sozinho continua sendo a preferência
do usuário. O objetivo de menus/som/controles universais permanece aberto:
**0/5 menus qualificados**, sem prova de compatibilidade de 90% ou natividade
integral de EE/IOP/VU. Rodar por 24 horas não garante esses resultados.

## Checkout e recursos

O repositório não leva builds, ISOs, ELFs, assets, RAM/VRAM, cards ou fontes
geradas dos jogos. Os recibos em `build/` citados nos documentos permanecem no
PC original; não presumir que existem no clone. O ciclo de adaptação e suas
fixtures próprias estão no código e podem ser reconstruídos.

```sh
git clone --depth 1 https://github.com/Pedrohs1771/ps2-native.git
cd ps2-native
```

O repositório é privado: autentique também a VPS. Não copiar tokens para fontes
ou scripts. Com 8 GB de armazenamento, comece pelas fixtures sem ISO e use um
único diretório de build. Compilar e extrair jogos exige espaço adicional; não
há qualificação de que o corpus comercial completo caiba nesses 8 GB.

## Build Linux x86-64

Exemplo para Ubuntu/Debian, baseado nas dependências do workflow do projeto;
esse ambiente de VPS ainda não foi testado:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git python3 pkg-config \
  libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS=-msse4.1 -DCMAKE_CXX_FLAGS=-msse4.1 \
  -DPS2X_BUILD_STUDIO=OFF -DPS2X_ENABLE_DEBUG_UI=OFF \
  -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF

cmake --build build --target ps2x_tests ps2_recomp ps2_native_overlay \
  ps2_analyzer ps2iso-inspect nexo_ee_autoadapt_fixture --parallel 2
./build/ps2xTest/ps2x_tests
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_autoadaptation_execution
```

A fixture executa MIPS próprio, captura código desconhecido, gera C++, compila,
relinka e repete. Verifica resultado 123 e reuso, incluindo KSEG0/KSEG1. Não usa
ISO comercial. CTest também contém testes com geradores próprios; para rodar
todos, construir seus targets antes de `ctest --test-dir build`.

Para probes de jogos sem desktop, o helper exige Xvfb, xauth, xdotool,
ImageMagick e um servidor PulseAudio/PipeWire acessível ao usuário normal.
O helper cria display/sink próprios e isola preferências de mute por execução.
ISOs devem ser fornecidas separadamente pelo usuário.

## Próximo contrato

O último delta contém transferências tipadas do scheduler na chamada runtime
mais próxima, propagando `false` pelos retornos gerados. Os testes verificam
suspensão, callback, destrutores C++, contexto ativo e propagação de erros.
O scheduler também preserva Count quando uma invocação muda e evita ler um
contexto removido. Esse delta passou 525 testes C++, mas ainda precisa de replay
comercial e medição de desempenho próprios.

Prioridade: medir cadência EE/VBlank/MPEG com o delta atual; reduzir a primeira
divergência e corrigir o contrato compartilhado. Não forçar relógios, inventar
áudio ou declarar sucesso por imagem estática/ausência de miss. AutoDMA entrega
PCM, mas SPU RAM/vozes ADPCM, ADSR, reverb, DMA comum e demais serviços não estão
fechados. Preservar os limites e a política de recusa da adaptação automática.
