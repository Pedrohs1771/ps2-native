# Auditoria do estado e caminho para publicação — 03/10/2026

O projeto tem uma base funcional de tradução, empacotamento e laboratório.
Ainda não cumpre o beta de menus, áudio e controles, nem demonstra 90% de
compatibilidade. A conclusão é de integração e compatibilidade ainda abertas;
o trabalho restante vai além de acabamento para publicação.

Esta auditoria responde ao pedido de análise. Preserva as mudanças locais,
ISOs, saves, catálogos e pacotes. Nenhum código de runtime foi corrigido aqui,
nenhuma nova conversão comercial foi executada e nada foi enviado ao GitHub.
Os percursos comerciais abaixo são evidências existentes, inspecionadas agora.

## Estado observado

Checkout: branch `codex/ps2-native-recomp`, HEAD
`c4d5ea7ad3872f91604a7b30c77db5367fa746db`. Havia 35 caminhos modificados ou
não rastreados antes da criação deste relatório. O remoto do projeto é
`ps2native`; `origin` aponta para a base ran-j/PS2Recomp.

O recibo atual, [corpus.json](../build/beta-v01-20261003/corpus-build-v1/corpus.json),
já terminou em `BUILT_UNVERIFIED`: cinco imagens únicas, cinco pacotes e
integridade verificada. O tempo total registrado foi 6.046,61 s, cerca de 1h41.
Os documentos que ainda descrevem esse lote como `BUILDING` ficaram antigos.

| ISO | Build registrado | Última evidência de execução inspecionada | Menu aceito |
|---|---:|---|---|
| Metal Slug 4 | 27m45s | Capturas pretas/quase pretas; MPEG e GIF ativos; PCM silencioso | Não |
| Monster House | 20m02s | Magenta; PC observado em `0x111f08`, thread com status 5; sem DMA/GIF | Não |
| R-Type Final demo | 12m07s | Magenta; countdown em `0x116f48`; sem DMA/GIF | Não |
| RoboCop | 26m31s | Preto; DMA/GIF ativos, sem resposta visual comprovada; PCM silencioso | Não |
| Sega Ages SDI & Quartet | 14m21s | Magenta; countdown em `0x1982d8`; sem DMA/GIF; PCM silencioso | Não |

São **5/5 builds e 0/5 percursos de menu qualificados neste lote**. Isso não
mede a compatibilidade da biblioteca PS2 nem invalida demonstrações históricas
de outros runners. Na variante anterior de Metal Slug houve intro animada após
uma correção libgif; isso também não constitui aprovação do menu atual.

As dez imagens antes/depois de input foram examinadas por distribuição de
pixels: nove são uniformes; a outra tem duas cores quase pretas. Não há menu
identificável nessas capturas. Os WAVs locais medidos têm zero amostras não
nulas: Metal Slug 45,056 s, RoboCop 39,595 s e Sega Ages 39,936 s, todos estéreo,
48 kHz/16-bit. Essa conclusão se limita aos percursos capturados.

## O que já está construído

- Inspeção ISO/SYSTEM.CNF, identificação/hash de ELF e staging privado.
- Análise e tradução R5900 para C++, compilação e pacote desktop automático.
- Runtime com scheduler, memória/dispositivos, GS, MPEG, áudio, pad e IOP.
- Captura/replay, testes sintéticos, geração offline EE/IOP/VU e recuperação EE.
- Catálogo EE de 18.193 famílias em 41 capturas, compilado com reuso incremental.

O catálogo de famílias foi conferido diretamente. Manifest:
`62b8bfd8b17d524d2d2f1eba8513988275cb551024391fae2eb50de345bd4fd7`.
Biblioteca `build/ee-families-native/libps2_ee_compiled_families.a`:
108.078.172 bytes, SHA-256
`e8c446ddf91e76cbf1d631de04730579a373bb98dfcc5893d6e99b536491afc2`.
O recibo é `built`, com `strict_approval=false` e `closure_proved=false`.
As 41 capturas não representam 41 jogos; essa biblioteca não foi instalada
automaticamente nos cinco pacotes.

## Bloqueios, por prioridade

1. **Aceitação de execução.** Nenhuma imagem do lote tem menu navegável,
   áudio correto e controle causal comprovados. A primeira falha precisa ser
   reduzida antes de repetir builds completos ou aumentar catálogos.
2. **Pacote AOT integrado.** Nos cinco caches, `PS2X_RUNTIME_NATIVE_IOP`,
   `PS2X_RUNTIME_AOT_EE_OVERLAYS`, `PS2X_RUNTIME_EE_DATA_FAMILIES` e
   `PS2X_RUNTIME_PARALLEL_GS` estão `OFF`; `PS2X_IOP_ENABLE_INTERPRETER` está
   `ON`, e os manifests de bancos estão vazios. O boot EE tem código traduzido,
   mas isso não comprova EE/IOP/VU integralmente AOT. Ver
   [pipeline.py](../tools/ps2native/pipeline.py),
   [CMakeLists.txt do runtime](../ps2xRuntime/CMakeLists.txt) e
   [backend IOP](../ps2xRuntime/src/lib/ps2_iop_backend.cpp).
3. **VU no runtime comum.** Os callbacks MSCAL/MSCNT chamam
   `m_vu1.execute/resume`, e o caminho VU0 também usa o executor existente.
   O laboratório AOT não equivale à sua integração no pacote comum. A execução
   efetiva precisa ser auditada por percurso, inclusive nos menus que usam VU.
4. **Ciclo automático incompleto.** `recover-ee` prepara, gera e pode compilar
   a biblioteca de famílias. Não executa o ciclo completo de relink, replay e
   aceitação por pacote. A CLI de build não expõe esse ciclo integrado.
5. **Cobertura sem fechamento.** Os cinco manifests registram `known_gaps` e
   intervalos sem atribuição única. As instruções reportadas como não tratadas
   são 2/25/58/36/4 na ordem Metal Slug/Monster House/R-Type/RoboCop/Sega Ages.
   Esses contadores exigem classificação: dados em segmentos executáveis podem
   confundir a análise. Nenhum deles comprova cobertura completa ou a causa do
   bloqueio de boot.
6. **Fim do percurso e cadência.** Há instrumentação de frame timing no código
   atual, mas não foi encontrado CSV de cadência no lote inspecionado. A tela
   Coming soon e seu detector qualificado de início de partida seguem ausentes.
   Timeout, erro e ausência de código devem continuar sendo falhas distintas.

Os helpers headless usados nessas sessões registram
`overlay_driver_disabled=false`: permitem o driver de compilação diagnóstica
EE. Isso não prova que ele foi usado em cada sessão, mas impede tomar esses
ensaios, sozinhos, como evidência de execução estritamente AOT.

## Próxima tarefa concreta

Começar pelo countdown compartilhado de R-Type e Sega Ages. A leitura dos
fontes privados confirma o mesmo padrão e chamada ao helper
`ps2xFastForwardGuestCountdownLoop`. O sintoma comum ainda não confirma defeito
no helper. Capturar `$v0/$v1` completos, PC de retorno, decrementos, checkpoint
e ciclos numa sessão curta; comparar com uma reprodução sintética limitada.

Orçamento inicial: **4–8 horas de diagnóstico**, com entrega de contraexemplo
ou classificação precisa. Se houver erro geral, a correção precisa falhar no
teste antes e passar depois, seguida de delta/relink nos dois runners. Se o
loop terminar e surgir outra falha, registrar essa mudança de estado sem
promover o jogo a aceito.

Monster House é uma investigação separada: o log acusa falta de `-w`, o runtime
suporta `boot-args.txt`, mas o pipeline não escreve esse arquivo e o helper só
passa ELF/ISO. Conferir os argumentos esperados e a syscall que precede a saída
da thread. Esse encadeamento é uma hipótese de diagnóstico, não causa provada.

Depois de alcançar o primeiro menu: navegação/submenus/retorno, PCM comparado
ao jogo e frame times; então integrar bancos AOT, recuperação offline e teste
fora do checkout. Expandir para os demais títulos após esse percurso passar.
O orçamento do diagnóstico não é promessa de alcançar menu nesse intervalo.

## Publicação e prazo

| Entrega | Estimativa de trabalho | Condição |
|---|---|---|
| Alpha pública de engenharia | 1–2 dias de trabalho focado, estimativa preliminar | Build limpo, documentação atual, testes/CI, revisão dos arquivos distribuídos e limitações explícitas |
| Primeiro beta com menu, áudio e controles corretos | Ainda sem prazo confiável | Depende de isolar e corrigir os bloqueios de execução; estimar novamente após o primeiro percurso aceito |
| Automatizar 90% das ISOs | Sem data defensável; planejar como esforço de meses | Corpus representativo, denominador definido, títulos reservados e aceitação individual |

A primeira linha é preparação de uma ferramenta experimental para colaboração
pública. Não entrega a compatibilidade desejada. Não há base para prometer
fechamento universal em 24/48 horas nem para calcular um percentual global de
progresso a partir dos testes ou das famílias.

Adiar gameplay 3D reduz a validação de campanha, mas não elimina os contratos
de GS/VU, DMA, áudio e scheduler usados pela intro e pelos menus. Identificar
automaticamente uma transição de gameplay também requer evidência do jogo.

Para a alpha, atualizar o README principal: ele ainda prioriza gameplay 3D e
um sprint histórico de 24 h. Manter [BETA_V01.md](BETA_V01.md) como contrato de
aceitação e esta auditoria como checkpoint. A CI já possui Linux GCC/Clang e
Windows, mas não executa os testes Python da CLI nem habilita o laboratório.
O teste local não substitui uma construção em ambiente limpo.

A distribuição deve seguir as regras já existentes em `AGENTS.md`: preservar
GPLv3/créditos da base e manter insumos comerciais e resultados derivados dos
jogos em `build/` privado. Revisar também os componentes binários já rastreados
em `ps2xRuntime/vita/module/` antes de montar o pacote público.

O upstream [PS2Recomp](https://github.com/ran-j/PS2Recomp) se apresenta como
experimental e descreve um runtime/stubs a expandir. A referência
[N64Recomp](https://github.com/N64Recomp/N64Recomp) também exige runtime e
metadados. Consultados em 03/10/2026; são contexto arquitetural, não uma
previsão de prazo deste projeto.

## Verificação executada nesta auditoria

- Build incremental de `ps2x_tests` e `ps2_recomp`: passou.
- `./build/ps2xTest/ps2x_tests`: **496/496**, 5,99 s.
- `unittest discover -s tools/ps2native/tests`: **50/50**, 2,265 s.
- Três módulos de descoberta/catálogo/partição EE: **84/84**, 4,077 s.
- CTest existente: **78/79** no lote paralelo com timeout de 45 s.
  `nexo_ee_data_family_execution_tests` atingiu esse limite; foi recompilado
  e passou isoladamente em **40,97 s** com orçamento de 90 s. Todos os casos
  terminaram aprovados considerando essa repetição; o primeiro timeout fica
  preservado. Não foi diagnosticada regressão semântica nesse teste.
- `git diff --check`: falhou por espaços finais preexistentes no
  `README_GERAL_COMPLETO.md`; nenhum erro de runtime decorre desse resultado.

Logs e recibos desta verificação:
[audit-state-20261003-094829](../build/audit-state-20261003-094829/).
Não foi feita auditoria exaustiva de segurança nem teste de instalação Android.
Próxima ação: reprodução limitada dos countdowns; manter os pacotes do lote
intactos e usar jobs novos para instrumentação/delta.
