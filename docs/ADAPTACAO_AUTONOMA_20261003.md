# Adaptação local automática — checkpoint de 03/10/2026

A meta completa continua aberta: converter ISOs inéditas, com menus, som,
seleções e controles corretos, sem intervenção por jogo e sem IA na conversão.
Gameplay 3D permanece fora desta etapa. Nenhuma das cinco ISOs tem menu,
áudio ou controles aprovados; os resultados abaixo não provam universalidade.

## Hipótese e mecanismo implementado

Hipótese: uma falta de código EE observada pode virar um contraexemplo
reutilizável. O processo convidado para; a ferramenta valida a captura,
gera C++ dos bytes observados, compila, relinka e repete a execução. Os bancos
nativos conferem os bytes das instruções na admissão. A memória mantém
casos privados por ISO, gerador e revisão das fontes. Isso automatiza a
recuperação de entradas que a análise estática não encontrou.

O runtime entregue não chama compilador ou agente durante a execução.
`convert` usa os compiladores durante a conversão offline. Há limites de
rodadas, tempo e tamanho; uma falta repetida para com recibo de ausência de
progresso. Falhas de compilação, capturas incompletas, contexto não admitido
e bloqueios de dispositivos não são transformados em sucesso.

Os testes contradizem a hipótese se um banco não executa os bytes admitidos,
aceita uma versão alterada ou se o ciclo necessita de uma edição humana de
PC/TOML. Menus, dispositivos e caminhos não percorridos exigem evidência própria;
ausência de miss durante um probe curto não demonstra cobertura completa.

## Evidência desta rodada

- Fixture própria: duas funções MIPS desconhecidas, em janelas diferentes,
  foram capturadas, geradas, compiladas e executadas nativamente. Resultado
  aritmético 123, incluindo delay slots. A segunda conversão reutilizou os
  dois casos e realizou zero recuperações.
- Três regressões reproduziram callbacks de RPC, IRQ e GS VSync descartados
  silenciosamente. Após a correção, todos entram no relatório de falta e
  interrompem a execução sob a política estrita, sem completar o callback.
- Uma reprodução nativa confirmou a captura duplicada da mesma falta, uma
  sem contexto. Após a correção, a invocação publica um único pedido com
  contexto. O catálogo continua recusando bindings prontos que seriam
  indevidamente substituídos por uma captura.
- Lote real de cinco ISOs, com objetos convidados de builds anteriores:
  os cinco terminaram probes delimitados. R-Type recuperou duas entradas;
  Sega Ages recuperou outras duas. A nova conversão de ambos reutilizou
  os quatro casos, com zero recuperações adicionais. Nenhum PC foi fornecido
  ao algoritmo. O experimento não qualifica o caminho de build frio.
- Monster House emitiu um requisito de diretório de trabalho. A tentativa
  com somente opção/valor falhou e o ciclo recusou a repetição. O vetor
  completo de argumentos, com nome do programa derivado dos metadados da ISO,
  passou desse diagnóstico e revelou faltas EE seguintes, recuperadas pelo
  mesmo algoritmo. Foram acumulados 24 casos EE compilados. Um replay de
  80 segundos mostrou os logotipos Columbia e THQ; o replay mais longo mostrou
  também A2M e parou numa nova entrada EE aos 114 segundos. Logo, os primeiros
  20 segundos sem falta não demonstravam fechamento do percurso.
- A gravação isolada desse replay contém 10.518.528 amostras, correspondentes
  a 109,568 segundos estéreo a 48 kHz. Todas as amostras são zero: pico e RMS
  zero. Não há áudio nem controles aprovados nesse percurso.
- A memória de argumentos agora só publica uma configuração depois de um
  probe completo sem o diagnóstico de falta. Configurações repetidas,
  interrompidas ou não confirmadas não são reutilizadas. O fingerprint das
  fontes inclui `lab/`, pois o catálogo depende de código dessa árvore.
- Três regressões do barramento SPU2 falharam antes da correção. Elas verificam
  ganhos de registradores, entrega de DMA PCM e descarte da fila no reset.
  A correção liga o IOP ao consumidor, copia os
  dados, preserva a ordem, monta planos parciais e mantém os dois núcleos
  independentes. Um ELF IOP próprio executou SH/SW e produziu PCM conhecido sem
  passar pelo RPC HLE LIBSD. Esse teste não prova áudio comercial correto.
- O primeiro `convert` frio pela CLI falhou na geração do CMake: o desktop
  ativava executáveis de laboratório cujos geradores não estavam presentes.
  A configuração agora pode incluir somente os hooks de captura. Uma reprodução
  pequena falhou antes e passou depois. A repetição do build frio comercial
  compilou 4.259 fontes geradas, mas o processo pai terminou com código 143
  durante o primeiro probe. A tentativa incompleta foi preservada; a retomada
  reutilizou os mesmos objetos e verificou que fontes e ferramentas não mudaram.
  A retomada concluiu em 124,214 segundos: pacote com integridade verificada,
  zero casos reutilizados, probe de 120 segundos e estado `replayed_unverified`.
  SHA-256 do runner: `2171dd9f1a2962b75d7c5af37f69ff62dc4b4ef309b443f82e2fdf6afd13b1d9`.
  É uma retomada de componentes da CLI após interrupção, não uma conversão fria
  contínua nem uma aprovação de menu. O compilador de overlays ao vivo ficou
  desativado.
- O lote `spu-corpus-v2` concluiu cinco replays de 120 segundos com o runtime
  corrigido e bancos privados previamente observados, recompilados e validados.
  Nenhum replay gerou nova recuperação nessa janela. Cada gravação contém
  11.042.816 amostras, 115,029333 segundos estéreo a 48 kHz, todas zero.
  As últimas imagens mostram Ignition em Metal Slug e A2M em Monster House;
  R-Type e RoboCop permanecem pretos, Sega Ages uniforme magenta. Logos e
  ausência de falta nesse intervalo não aprovam menus ou controles.
- Um controle sonoro próprio passou pelo consumidor e pelo mesmo gravador
  isolado: 643.054 das 655.360 amostras foram não nulas. O pico observado foi
  1.237 para PCM de origem com amplitude 1.800. O teste original de amplitude
  exata falhou e permanece preservado; o recibo separado comprova somente que
  a captura detecta som. A causa da atenuação host ainda não foi qualificada.
- A auditoria posterior identificou um confundidor nas medições de áudio:
  o PipeWire restaurava `mute=true` para `ps2EntryRunner`, inclusive no sink
  virtual. Em dois replays do **mesmo binário**, a gravação de 6.717.440
  amostras passou de zero para 4.099.367 amostras não nulas (pico 22.526)
  quando o teste usou uma identidade de aplicação própria. Nenhum ganho ou
  mute da sessão normal foi alterado. Essa evidência corrige a conclusão de
  que todo o áudio zero demonstrava uma falha do produtor. Não aprova
  conteúdo, cadência, áudio de menu ou fidelidade dos outros títulos.
- Uma instrumentação privada, separada do produto, observou 25 cópias SIF
  no replay Metal Slug: 13 continham bytes não nulos e todas foram lidas de
  volta do IOP com igualdade exata. Uma captura EE confirmou o cabeçalho
  estéreo e PCM não nulo. Campos inválidos no printf não demonstravam
  corrupção desse cabeçalho; o formato de argumentos desse diagnóstico
  ainda precisa de análise própria.
- O teste desktop agora usa identidade Pulse própria por execução, removendo
  propriedades herdadas do processo convidado e registrando essa identidade.
  Não altera o mute salvo de `ps2EntryRunner`. A regressão de isolamento
  falhou antes e os 11 testes desse helper passaram após a correção.
- Uma regressão MPEG reproduziu a substituição de argumento nulo por lixo
  da pilha. O runtime agora lê o quinto argumento n32 diretamente de r8
  na inscrição do callback e no tamanho do ring. Os 521 testes C++ passaram;
  os 74 testes Python da CLI e adaptação também passaram.
- O replay seguinte usa o runtime atual, a identidade de áudio isolada e
  um banco observado recompilado. Executou 120 segundos sem falta EE nova.
  Das 11.042.816 amostras capturadas, 8.451.043 são não nulas; pico 22.526
  e RMS 6.624,934. Start foi enviado aos 30, 45 e 70 segundos, mas as imagens
  ainda mostram a abertura Ignition. Não há menu navegável ou efeito causal
  dos controles aprovado. O banco experimental e os objetos convidados
  anteriores não qualificam conversão fria.

Recibos posteriores, sob `build/solo-audio-1791058142879897408/`:

| Artefato | Evidência |
|---|---|
| `trace-v4/assessment-with-provenance.json` / `trace-v5/assessment-with-provenance.json` | Mesmo runner; mute restaurado versus identidade própria e PCM não nulo |
| `trace-assessment-v2.json` | 25 transferências SIF; 13 não nulas, readback IOP exato |
| `headless-red-v2.log` / `headless-green.log` | Regressão real de isolamento; 11 testes passaram |
| `mpeg-red.log` / `mpeg-green.log` | Callback n32 nulo; 520/521 antes, 521/521 após correção |
| `python-green.log` | 74 testes da CLI/adaptação passaram |
| `menu-audio-v1/corpus.json` | Replay atual de 120 s, PCM não nulo, gates de menu/áudio/input falsos |
| `rotation-red.log` / `rotation-green.log` | Rotação sem peer corrigida; 523 testes C++ passaram |
| `rotation-comparison-v1.json` | Dois probes de 65 s: 803.434.488 → 847.431.248 ciclos no host tick 3.600; comparação pontual, sem qualificação de menu |
| `transfer-red.log` / `transfer-green.log` | Transferência tipada contida na chamada mais próxima; suspensão, callback, guardas C++ e erros; 525 testes C++ passaram |

Recibos privados, sob `build/solo-native-1791036997119777348/`:

| Artefato | Evidência |
|---|---|
| `auto-native-green.log` | 2 testes nativos passaram; captura única e recuperação/reuso |
| `callback-red.log` / `callback-green.log` | 3 falhas reproduzidas; 506/506 testes C++ após correção |
| `auto-python-final.log` | 71 testes Python passaram no checkpoint intermediário |
| `auto-prepare-regressions.log` | 6 testes do preparador passaram |
| `auto-catalog-regressions.log` | 17 testes do catálogo passaram |
| `corpus-auto-v2/corpus-adaptation.json` | Cinco probes; quatro recuperações EE reais |
| `corpus-auto-warm-v1/corpus-adaptation.json` | Dois jogos; quatro casos reutilizados, zero recuperações |
| `corpus-startup-v1/` | Contraexemplo: configuração parcial não resolveu a inicialização |
| `corpus-startup-v2/` | Configuração completa; sete casos EE compilados; orçamento esgotado |
| `corpus-startup-v3/` | Continuação automática com os casos privados já guardados |
| `corpus-startup-v4/` | 15 casos reutilizados e 9 novos; 24 casos acumulados |
| `menu-qa-v1/` | Imagens reais de logos durante 80 segundos; sem aprovação de menu |
| `menu-qa-v2/` | Nova falta após 114 segundos e áudio com 100% de amostras zero |
| `startup-cache-red.log` / `startup-cache-green.log` | Publicação prematura e fingerprint incompleto reproduzidos e corrigidos |
| `startup-confirmation-red.log` / `memory-python-final-v2.log` | Candidato não confirmado recusado; 74 testes Python passaram |
| `spu-bus-red.log` / `spu-iop-integration-tests.log` | Três falhas reproduzidas; 510/510 testes C++, incluindo execução IOP própria |
| `spu-auto-native-regressions.log` | 5 testes nativos: captura/recovery/reuso, relink e objetos do catálogo |
| `capture-cmake-red.log` / `capture-cmake-green.log` | Separação de hooks de captura e ferramentas offline reproduzida e corrigida |
| `cold-convert-v1/` | Falha real do desktop na configuração CMake; insumos preservados |
| `cold-convert-v2/resume-result.json` | Compilação fria retomada com os mesmos insumos; integridade verificada, menu não aprovado |
| `spu-corpus-v2/corpus.json` | Cinco replays concluídos; áudio zero e nenhum menu aprovado |
| `audio-positive-v1/assessment.json` | Captura detectou som próprio; amplitude host exata não comprovada |
| `entry-contract-red.json` | Thread, alarme e SIF recusaram entradas sem binding antes de gerar recuperação |
| `entry-paths-prior-runtime-red-v2.json` | Oito defeitos reproduzidos no runtime comercial preservado, com os testes finais |
| `entry-paths-green-v3.log` | 519/519 testes C++; recuperação de entradas e preservação do endereço RPC exato |
| `entry-native-recovery-tests-v2.log` | 6 testes nativos passaram, incluindo duas threads MIPS inéditas e reuso na segunda conversão |
| `entry-corpus-v1/` | Dois probes interrompidos por entradas KSEG antes de fechar esse contrato |
| `alias-prepare-red.log` / `alias-native-red.log` | Casos KSEG recusados antes da correção, no preparador e no ciclo nativo |
| `alias-runtime-green.log` | 520/520 testes C++; bindings virtuais distintos e invalidação por escrita física |
| `alias-native-green-v2.log` | 32/32 testes; captura, preparação, catálogo, execução KSEG0/KSEG1 e reuso |
| `alias-tools-tests.log` | 74/74 testes Python da CLI e adaptação |
| `alias-corpus-v1/corpus.json` | Uma recuperação KSEG comercial, zero bancos reutilizados, replay de 180 segundos; menu e áudio pendentes |

Cada runner, objeto reutilizado, catálogo e ISO tem identidade nos recibos.
Esses arquivos incluem dados derivados de jogos e permanecem locais.

## Limites e próximo trabalho

O mecanismo recupera código e uma classe explícita de requisitos de argumentos;
não sintetiza automaticamente a semântica de um dispositivo ausente. O último
probe de Sega Ages registrou RPC IOP não atendido. Metal Slug entregou PCM não
nulo na repetição com áudio de teste isolado das preferências salvas. Os zeros
das gravações anteriores não qualificam a causa no runtime. É necessário
medir fidelidade, cadência e serviços, além de provar GS, VU, input causal e
apresentação. IOP/VU integralmente
nativos, conversão fria contínua pela CLI, menus e a meta de compatibilidade não estão
qualificados.

O caminho SPU2 novo atende o envio de PCM AutoDMA pelo IOP. Não implementa ainda
SPU RAM/vozes ADPCM, ADSR, reverb, todos os modos de mixer nem o contrato completo
de avanço de MADR e interrupções por consumo. DMA comum de voz continua
explicitamente diagnosticado como não implementado; não recebe áudio inventado.
Os hooks corrigidos devem ser medidos nas ISOs antes de qualquer aprovação.

A revisão encontrou entradas que filtravam callbacks/thread entries pela presença
prévia no catálogo: `StartThread`, callbacks MPEG, comandos SIF, RPC guest server,
alarmes, exit handlers e overrides de syscall. Oito regressões reproduziram essas
falhas no runtime preservado; a correção genérica entrega entradas não nulas ao
scheduler estrito, onde a falta captura o contexto real e interrompe a execução.
O RPC estrito preserva o endereço exato, em vez de subtrair `0x10000` e chamar
outra função. O modo de diagnóstico anterior e recusas de entradas nulas foram
preservados. Os 519 testes C++ passaram. Duas threads com corpos MIPS próprios
desconhecidos foram capturadas, compiladas offline e executadas com resultado
123; a segunda conversão reutilizou os dois casos sem recuperação adicional.
Isso prova esse caminho de adaptação; não fecha os serviços, áudio ou menus.

O replay comercial após essa correção revelou chamadas ao kernel em KSEG.
Metal Slug tentou `0x80075000`; a RAM física em `0x75000` continha instruções,
mas a captura registrou `OutsideRam` e não produziu a janela de código. O probe
Monster House também terminou nessa classe de recusa. Isso expôs um contrato
ausente, antes oculto pelos filtros de callbacks; não é aprovação de boot.

A correção de KSEG separa o PC virtual do offset físico de RDRAM. Captura,
preparação, geração, catálogo e byte guards preservam o primeiro e usam o segundo
para ler os bytes. Bindings físicos, KSEG0 e KSEG1 têm índices distintos: um corpo
compilado para um PC físico não aprova outro PC virtual. BIOS, scratchpad, outros
segmentos e janelas que ultrapassam 32 MB continuam recusados. TLB, permissões e
visibilidade de caches não estão qualificados por essa correção. Consultar os
testes finais e o replay posterior antes de afirmar que o kernel comercial fechou.
Os 520 testes C++ e os 32 testes de recuperação passaram. Nos testes nativos,
threads MIPS próprias em KSEG0 e KSEG1 foram capturadas com PC virtual, compiladas
e executadas com resultado 123; cada segunda conversão reutilizou os dois casos.
No teste de guardas, uma escrita física invalidou os três bindings distintos.

O replay posterior de Metal Slug terminou com uma recuperação automática e
zero bancos reutilizados. O primeiro processo parou estritamente em 3,487 segundos
com código 73; o capturador agora registrou `MissingEntry`, preservou a janela
virtual `0x80070000` e seus bytes físicos e o gerador compilou a entrada
`0x80075000`. O segundo processo executou por 180,001 segundos sem nova falta EE.
O banco contém 16.383 bindings, incluindo entradas interiores; essa quantidade
não representa funções independentes nem compatibilidade. SHA-256 do runner:
`2826cbae19f9760c33918e521b12e06bd64d60cd6043a82f9e0272413c20f701`.

As imagens de 100 e 170 segundos mostram progresso na animação Ignition. A gravação
final contém 16.809.984 amostras, 175,104 segundos estéreo a 48 kHz; todas são zero.
O estado é `replayed_unverified`, com menu, áudio, controles e natividade integral
sem aprovação. Essa gravação antecede a auditoria de mute e não demonstra que
o produtor permaneceu zerado. O replay atual acima confirma PCM não nulo.
A recuperação desse código de kernel está demonstrada; a cadência da intro,
a fidelidade do som e os contratos restantes continuam abertos.

O delta posterior contém `EeDispatcherTransfer` na chamada runtime mais
próxima e propaga `false` pelos retornos gerados. Ele verifica que o contexto
continua ativo antes de tocar seus registradores; PC igual ao fallthrough não
aprova uma thread suspensa ou substituída por callback. Os destrutores C++
continuam executando e erros comuns continuam propagando. O scheduler evita
ler contextos que foram removidos ou substituídos e preserva Count nas mudanças
de invocação. Esse delta passou os testes próprios, mas ainda não tem replay
comercial nem ganho de desempenho qualificado.

Próxima ação: medir esse delta na thread convidada e reduzir a diferença entre avanço dos
ciclos EE, VBlank e apresentação MPEG. Não acelerar contadores arbitrariamente
para produzir uma imagem. Repetir o percurso comercial após uma correção geral
demonstrada. Os casos e recibos devem ser preservados; mudanças de gerador ou
fontes criam outra identidade de memória.

Não há rede neural nem treinamento por título. Uma rede pode orientar uma busca
futura, mas sua saída precisaria dos mesmos testes, guardas e provas de semântica.
Também não há prova de novidade científica: a contribuição deve ser comparada
com trabalhos anteriores e avaliada por resultados reproduzíveis.

## Literatura primária consultada

[Di Federico e Agosta, CASES 2016](https://rev.ng/downloads/cases-2016-paper.pdf)
trata da recuperação de destinos de saltos na tradução binária estática,
incluindo análise de dados e expressões com índice limitado. Isso sustenta
a investigação de tabelas de switch; não fornece uma solução PS2 completa.

[Di Federico, Payer e Agosta, CC 2017](https://rev.ng/downloads/cc-2017-paper.pdf)
analisa CFGs e limites de funções, incluindo chamadas indiretas e assembly
manual. O trabalho mostra por que descoberta estática e correção da execução
são obrigações distintas. A nossa combinação de captura e bancos finitos
deve ser comparada com essa literatura; a pesquisa feita não é exaustiva.

[PS2SDK](https://github.com/ps2dev/ps2sdk) e seu
[contrato libsd](https://github.com/ps2dev/ps2sdk/blob/master/common/include/libsd-common.h)
servem de referência para interfaces de voz, AutoDMA, parâmetros, switches,
endereços e batches. Atender somente SetParam/PCM não fecha todos esses
contratos, nem prova a compatibilidade dos serviços comerciais.

O mapa de [registradores SPU2 do PS2SDK](https://github.com/ps2dev/ps2sdk/blob/master/iop/sound/libsd/include/spu2regs.h)
define strides distintos para vozes e ganhos dos núcleos. O
[envio BlockTrans](https://github.com/ps2dev/ps2sdk/blob/master/iop/sound/libsd/src/block.c)
confirma que um IRX pode programar registradores e DMA diretamente, sem chamar
o contrato HLE do host. Isso motivou a ligação do barramento físico ao consumidor.

O [mapeamento de memória do PCSX2](https://raw.githubusercontent.com/PCSX2/pcsx2/master/pcsx2/Memory.cpp),
em `memMapKernelMem`, mapeia KSEG0 e KSEG1 à memória física. Foi consultado como
referência do contrato de endereços; nenhum código desse arquivo foi copiado.
O nosso domínio atual cobre somente a parte desses segmentos que aponta à RDRAM.
