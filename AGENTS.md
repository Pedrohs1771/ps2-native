# PS2Native — instruções persistentes

## Preferência de assistência do usuário

O usuário revogou o uso de workers em 03/10/2026. Trabalhar sozinho nesta sessão:
não criar nem retomar agentes ou sessões auxiliares, incluindo Luna e Hyper.
O agente raiz implementa, testa, integra e revisa diretamente. Preservar o
trabalho e os recibos deixados pelas sessões encerradas. Não alterar o modelo
ou esforço da sessão atual.

## Objetivo e entrada da sessão

O produto é um conversor automático de ISO PS2 para aplicativo host completo.
O objetivo ativo é **Beta v0.1: boot até menus, áudio e controles, apresentação
host a 60 FPS e saída Coming soon numa transição de partida qualificada**.
Gameplay 3D completa não é a tarefa atual. Leia `docs/BETA_V01.md` primeiro.
Os quatro jogos da imagem e Monster House formam o corpus local inicial.
Correções devem melhorar o
conversor geral, sem exigir que o usuário escreva PCs, patches ou TOMLs por jogo.

Leia primeiro `docs/RETOMADA_MAHORAGA.md`, `git status --short` e o recibo da
última execução. Leia o código e o contrato do subsistema que vai alterar.
Use `README.md` para os marcos M6–M9 e consulte seções específicas do acervo
quando necessário. Não carregar as 29 mil linhas do README consolidado em toda
sessão nem produzir outra consolidação sem pedido explícito.

Os recibos novos prevalecem sobre snapshots históricos para descrever o estado.
A recuperação particionada de 02/10 já compilou 18.193 famílias; verificar
`build/lab/ee-sharded-native-recovery-1790952188934350523/recovery/recovery.json`.
Esse catálogo está disponível; não repetir o load de gameplay como prioridade
do beta. A próxima tarefa é fechar menus/áudio/controles nas ISOs novas.
Uma tarefa interrompida pode ter deixado processos ou artefatos concluídos:
inspecionar antes de refazer. Não presumir que a biblioteca está ligada.

## Estrutura e comandos

- `tools/ps2native/`: CLI Python, pacote ISO e helpers de execução.
- `ps2xRecomp/`: tradutor/geradores C++20; `ps2xAnalyzer/`: análise.
- `ps2xRuntime/`, `ps2xIOP/`: runtime, dispositivos e execução IOP.
- `lab/`, `schemas/`: captura, replay, síntese offline e contratos.
- `build/`: ISOs derivadas, capturas, fontes geradas, objetos e recibos privados.

```sh
python3 -m tools.ps2native --help
python3 -m tools.ps2native.corpus_batch --help
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_ee_data_families lab.tests.test_ee_family_catalog lab.tests.test_prepare_ee_family_batch
cmake --build build --target ps2x_tests ps2_recomp --parallel 4
./build/ps2xTest/ps2x_tests
git diff --check
```

Para C++, rodar os testes do subsistema alterado e fixtures geradas relevantes.
Na iteração usar `PS2X_FAST_ITERATION=ON`, `PS2X_ENABLE_RELEASE_IPO=OFF`; preservar
fontes/objetos inalterados. Compilar apenas o delta quando o ABI permitir.
Python usa stdlib, funções claras, quatro espaços e argv em listas, sem shell
interpolado. Seguir o estilo C++ do arquivo e manter semântica FP explícita.

## Critérios de engenharia

Sempre preservar mudanças locais, insumos, saves e recibos existentes. Criar
jobs novos. Em bug de semântica, fazer uma reprodução pequena que falha antes
da correção e passa depois. Uma comparação com PCSX2 deve identificar commit,
perfil/FP e domínio importado; separar componentes compartilhados da referência.
Não ampliar limites, filtros ou máscaras para esconder uma recusa.

Miss EE deve parar, capturar identidade/bytes/contexto disponíveis e entrar no
ciclo **offline** de geração → compilação → relink → replay. Divergência VU,
VIF/GIF, GS, áudio ou scheduler exige diagnóstico próprio; não tratar toda
falha como código EE ausente. Não acrescentar stubs de sucesso ou patches por
título para satisfazer uma screenshot.

Não promover `built`, `Ready`, exit 0, contadores GPU, imagem estática ou ausência
de miss a aprovação de gameplay. Registrar separadamente build, execução,
fidelidade, percurso, campanha e natividade de EE/IOP/VU. Manter
`strict_approval=false` e `closure_proved=false` nos contratos de laboratório.
Não prometer “qualquer ISO”, “100%”, “60 FPS” ou novidade científica sem evidência.

Não publicar ISOs, BIOS, assets, RAM/VRAM, cards, ELFs extraídos ou código/fontes
derivados dos jogos. Preservar GPLv3 e a atribuição à base PS2Recomp; conferir
licenças por componente antes de copiar código de referências externas.
Não refazer arquitetura, trocar backend inteiro ou adicionar dependência pesada
apenas para buscar SOTA. Primeiro demonstrar o ganho numa reprodução pequena.

## Ritmo de trabalho e entrega

Trabalhar uma tarefa por vez com critério observável e orçamento. Nas primeiras
ações, indicar qual artefato/teste será produzido. Atualizações devem dizer o
que foi aprendido e o próximo bloqueio; não apenas repetir que está trabalhando.
Com testes pertinentes passando, avançar para integração/aceitação; repetir
testes apenas por alteração, falha ou incerteza concreta.

Ao parar, deixar checkpoint curto: revisão/diff, comando, resultado, hashes do
catálogo e runner, bloqueio classificado e próxima ação. Reportar limitações
da release experimental. O prompt de retomada está em
`docs/PROMPT_CONTINUAR_CODEX.txt`; referências em `docs/REFERENCIAS_RECOMP.md`.
