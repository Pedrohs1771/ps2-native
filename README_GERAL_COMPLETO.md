# PS2Native — documentação geral completa, planos e estado da implementação

**Objetivo:** receber uma ISO de PlayStation 2 e produzir aplicações nativas completas para PC e Android, com descoberta, tradução, diagnóstico, correção, validação e empacotamento automáticos.

**Data deste levantamento:** 2026-10-01 22:36:40 -03 (America/Sao_Paulo).

**Repositório:** https://github.com/Pedrohs1771/ps2-native

**Checkout:** `/home/pedrohs/Downloads/ps2-native-recompiler`.

**Branch do levantamento:** `codex/ps2-native-recomp`.

**Commit de código publicado antes deste consolidado:** `d70a1684b72b6b8a1a78d26b46150099034ba594`.

Este arquivo reúne uma síntese atualizada e o conteúdo integral dos documentos do projeto encontrados no levantamento. O plano anterior, o plano NEXO atual, o relatório anterior, a documentação de laboratório, os contratos e os documentos dos componentes estão nos anexos. O índice de proveniência identifica cada original, sua quantidade de linhas e seu SHA-256.

As informações deste início do arquivo descrevem o estado atual observado. Os anexos preservam o histórico, inclusive resultados antigos e propostas que ainda não viraram implementação. Uma passagem antiga sobre um interpretador, um resultado de teste ou uma cena não deve ser usada para descrever automaticamente o build atual.

## Índice da síntese atualizada

1. [Resultado atual e leitura correta dos planos](#estado-geral)
2. [Objetivo integral e critérios de sucesso](#objetivo-integral)
3. [Arquitetura e fronteiras](#arquitetura-atual)
4. [Ingestão, análise e pipeline de pacotes](#pipeline-atual)
5. [EE estático, despacho e overlays](#ee-atual)
6. [Famílias EE e orquestração offline](#familias-ee)
7. [Continuações canônicas em desenvolvimento](#continuacoes-canonicas)
8. [IOP nativo e serviços](#iop-atual)
9. [VU AOT, replay e referência independente](#vu-atual)
10. [GS, gráficos, áudio, controles e saves](#dispositivos-atuais)
11. [Compilação, desempenho e uso da GPU](#desempenho-atual)
12. [Testes, experimentos e isolamento headless](#evidencia-atual)
13. [Processo realizado com Monster House](#monster-house-atual)
14. [PC, Android e auditoria de natividade](#plataformas-atuais)
15. [Memória, fetch, tempo e checkpoints](#contratos-abertos)
16. [Situação dos marcos M0–M11](#marcos-atuais)
17. [Caminho até M6, M7 e M8](#caminho-m6-m7-m8)
18. [Automação universal, nuvem e generalização](#automacao-atual)
19. [Pesquisa, otimizações e hipóteses](#pesquisa-atual)
20. [Riscos e disciplina de conclusão](#riscos-atuais)
21. [Mapa do código e comandos](#mapa-atual)
22. [Continuidade do trabalho e histórico](#continuidade-atual)
23. [Evidências locais e proveniência](#proveniencia-atual)
24. [Glossário](#glossario-atual)
25. [Documentação original completa](#documentacao-integral)

<a id="estado-geral"></a>

## 1. Resultado atual e leitura correta dos planos

O PS2Native já tem um pipeline real de inspeção, extração, análise, emissão de C++, compilação e empacotamento desktop. Há código convidado EE efetivamente compilado, um caminho IOP nativo exercitado e VU AOT exercitado em replays de laboratório. Há também diagnóstico, capturas, contratos de estado, testes de regressão, isolamento headless e reutilização de fontes e objetos.

O produto universal completo ainda não foi entregue. A conversão de uma ISO inédita em jogo integralmente aprovado, sem intervenção de desenvolvimento, ainda não está demonstrada. Não há campanha completa aprovada, pacote final totalmente nativo qualificado, Android físico aprovado, nem evidência de 60 FPS sustentados.

O experimento mais recente de jogo com os catálogos EE/IOP parou durante o carregamento. Experimentos anteriores do caminho diagnóstico alcançaram uma cena 3D e elementos do primeiro capítulo. Esses resultados pertencem a builds e perfis diferentes. A cena histórica não prova que o build com cobertura AOT mais estrita já alcança a mesma progressão.

### 1.1 Estado resumido por frente

| Frente | Implementação observada | Situação da entrega integral |
|---|---|---|
| Inspeção de disco | ISO9660/Joliet, boot por `SYSTEM.CNF`, inventário ELF e hashes | Formatos, código latente e ambientes fora do domínio atual continuam abertos |
| EE estático | C++ gerado, compilação host, registros e retomadas | Semântica e fechamento de todos os caminhos não aprovados |
| EE em RAM | Bancos offline, entradas por dependência e famílias parametrizadas | Produtores, fetch, aliases e automodificação ainda sem fechamento |
| IOP | AOT, famílias IRX, imports, módulos e execução observada no jogo | Ciclo de vida, serviços, estado oculto e fidelidade completa não aprovados |
| VU | AOT conservador, estados canônicos, replay VIF e continuações | Integração no jogo inteiro, domínio completo e timing físico não aprovados |
| GS | Implementação CPU, estado/VRAM e replay; otimização privada de build | Backend acelerado completo e referência independente ainda abertos |
| Áudio, input e saves | Serviços e correções com regressões; observações parciais | Campanha, áudio audível, movimento causal e persistência geral não aprovados |
| PC | Pacote e execução Linux x86-64 exercitados | Jogo integral e demais sistemas desktop não aprovados |
| Android | Staging Gradle/CMake, assets e Activity ARM64 | APK real qualificado e aparelho físico ainda sem evidência de aprovação |
| Autonomia | Etapas e orquestradores limitados já automatizados | Motor geral de reparo e exploração sem desenvolvedor não implementado |
| Nuvem | Arquitetura especificada | Serviço operacional de upload, filas, workers e entrega não demonstrado |
| Catálogo universal | Meta e protocolo definidos | Taxa de 90% e biblioteca inteira ainda sem medição válida |

### 1.2 Qual documento determina a meta

1. A solicitação do usuário mantém a meta de recompilação nativa universal para PS2, PC e Android.
2. O `README.md` NEXO é o plano normativo atual: define contratos causais, fechamento, tempo, memória, natividade e gates M0–M11.
3. O `README_AUTOMACAO_UNIVERSAL.md` conserva o plano anterior, o backlog, as propostas experimentais e o protocolo de generalização.
4. `PROJECT_SPEC.md`, relatórios e documentos de componentes registram decisões e estados de momentos anteriores.
5. Os resultados concretos devem ser consultados pelos seus inputs, hashes, commits, perfis, artefatos e escopos.

O `PROJECT_SPEC.md` antigo admite fallback seletivo. O contrato NEXO atual exige que o aplicativo final não introduza interpretação ou recompilação da ISA convidada. Interpretadores e compiladores podem existir no laboratório de conversão, como referência ou ferramenta de investigação. Essa diferença é mantida explícita neste consolidado.

Os documentos históricos não são certificados de execução atual. Quando duas descrições divergem, conferir primeiro a data, a identidade do build e o caminho que foi executado.

<a id="objetivo-integral"></a>

## 2. Objetivo integral e critérios de sucesso

A entrada do produto é uma imagem de disco de PlayStation 2 dentro do domínio suportado. O objetivo de “qualquer ISO” refere-se aos jogos e variações da biblioteca PS2; a extensão `.iso` sozinha não identifica console, formato, código ou ambiente.

A saída desejada é um executável PC ou pacote Android acompanhado de recursos, runtime, identidade de versão e evidências que justifiquem sua aprovação. O usuário final deve poder submeter a imagem e receber um resultado utilizável sem cadastrar callbacks, editar TOML, procurar endereços ou pedir a um agente que faça correções por título.

### 2.1 Entregas que ainda precisam ser demonstradas

- Descobrir código de boot, módulos, overlays, relocação e código materializado durante o jogo.
- Compilar EE, IOP e VU para código host previamente disponível no pacote final.
- Preservar comportamento dos dispositivos, memória compartilhada, exceções, eventos e timing.
- Validar progressão, controles, áudio, vídeo, gráficos e saves durante o conteúdo relevante.
- Construir e exercer as arquiteturas e plataformas de destino reais.
- Diagnosticar e reparar classes de falha automaticamente, com verificação independente do reparador.
- Medir desempenho sustentado, estabilidade, memória e tamanho de pacote.
- Demonstrar generalização com ISOs inéditas e um protocolo congelado.

### 2.2 O significado da meta dos 90%

Os 90% são uma etapa intermediária mensurável, não o estado atual e não uma redução da ambição de biblioteca inteira. O denominador precisa definir títulos, regiões, revisões, formatos, modos, hardware host e política de avaliação.

É necessário medir separadamente compatibilidade, automação, correção, desempenho e custo. Uma ferramenta que constrói um binário mas precisa de uma correção manual não conta automaticamente como conversão autônoma aprovada. Um jogo que mostra o menu não conta como campanha completa. Uma função traduzida não representa uma fração conhecida de títulos compatíveis.

### 2.3 O que não é uma aprovação

`status: complete` em um manifest de pacote pode significar apenas que a construção daquele artefato terminou. `Ready` em um probe significa que um predicado de admissão encontrou uma estrutura. Igualdade de dois caminhos que compartilham o mesmo emissor é regressão de modelo. Nenhum desses resultados, sozinho, aprova um port integral.

<a id="arquitetura-atual"></a>

## 3. Arquitetura e fronteiras

```mermaid
flowchart TD
    ISO[ISO PS2 e identidade dos inputs] --> ING[Inspeção, extração e inventário]
    ING --> ANA[Análise ELF, módulos, entradas e versões]
    ANA --> AOT[Traduções EE, IOP e VU offline]
    AOT --> CAT[Catálogos, famílias e fontes endereçadas por conteúdo]
    CAT --> BUILD[Compilação host e empacotamento]
    BUILD --> RUN[Runtime e dispositivos]
    RUN --> OBS[Capturas, falhas, traces e checkpoints]
    OBS --> LAB[Laboratório de replay e comparação]
    LAB --> ANA
    BUILD --> GATES[Gates de correção, conteúdo e desempenho]
    GATES --> PC[Entrega PC aprovada]
    GATES --> AND[Entrega Android aprovada]
```

O diagrama reúne componentes existentes e fronteiras previstas. O ciclo integral de reparo autônomo e os gates finais de entrega ainda não estão completos.

### 3.1 Componentes existentes

| Local | Responsabilidade |
|---|---|
| `tools/iso_inspect/` | Leitura e inspeção da imagem, extração e inventário |
| `tools/ps2native/` | CLI, workspaces, ferramentas host, manifests e pacotes |
| `ps2xAnalyzer/` | Análise de executáveis e geração de informações para tradução |
| `ps2xRecomp/` | Decodificação, emissão C++, registros e frontends offline |
| `ps2xRuntime/` | EE, dispatch, memória, dispositivos, apresentação e ponte host |
| `ps2xIOP/` | Subsistema IOP, kernel, módulos, imports, serviços e AOT |
| `lab/` | Captura, snapshots canônicos, replays, probes, síntese e referências |
| `schemas/` | Formatos e contratos versionados de estados, entradas e catálogos |
| `android/` | Activity, Gradle, CMake, assets e runner Android |
| `docs/` | Iteração nativa, handoff, lacunas e pipeline Android |
| `build/lab/` | Evidência local, capturas e artefatos ignorados pelo Git |

### 3.2 Três ambientes distintos

**Desenvolvimento diagnóstico:** facilita observação e pode usar caminhos provisórios. É o perfil atualmente selecionado em `build/` para regressões gerais.

**Experimento nativo de laboratório:** usa traduções e catálogos finitos e registra explicitamente o que executou sem interpretação. Pode continuar dependendo de componentes diagnósticos em outras fronteiras.

**Aplicativo final aprovado:** precisa satisfazer todas as obrigações do domínio e os critérios da plataforma. Não foi demonstrado até este levantamento.

### 3.3 Reuso da base

O projeto parte do PS2Recomp e mantém o trabalho de análise, tradução, runtime e integração existente. As correções e ferramentas adicionais são construídas sobre essa base. O repositório não transforma automaticamente o código comercial do jogo em um projeto redistribuível; os dados e artefatos derivados da ISO continuam inputs e outputs locais, com as restrições descritas nos documentos originais.

<a id="pipeline-atual"></a>

## 4. Ingestão, análise e pipeline de pacotes

O fluxo host implementado inspeciona a imagem, identifica o boot, extrai dados, verifica identidades, executa o analisador e o recompilador e conecta os fontes produzidos ao runtime. Cada execução recebe workspace isolado e logs por ferramenta.

O inventário preserva caminhos de disco e deduplica conteúdos ELF idênticos. `SYSTEM.CNF` informa a seleção de boot. A heurística atual de executáveis secundários EE é conservadora e não identifica todos os formatos de executável, arquivos proprietários ou produtores de código.

### 4.1 Publicação e verificação

- O destino de pacote precisa ser novo.
- A publicação usa staging e inventário de arquivos.
- `verify` compara o pacote com o inventário de hashes e tamanhos.
- Outputs mutáveis declarados, como logs de runtime, recebem tratamento próprio.
- O manifest registra ferramentas resolvidas, seus hashes, resultados, limitações e assessment estático.
- A identidade do pacote não substitui uma verificação de gameplay.

### 4.2 Handoff para CMake

`PS2X_GENERATED_CODE_DIR` conecta o C++ gerado ao projeto desktop ou Android. O runner deve utilizar o registro produzido para o jogo, em vez do placeholder vazio do checkout. Executáveis secundários elegíveis recebem diretórios e registros próprios, com ownership das faixas carregadas.

### 4.3 Lacunas da descoberta

Overlays arbitrários, executáveis não classificados, compressão proprietária, código materializado por funções e versões que não aparecem nos rastros continuam obrigações abertas. Um inventário limpo do boot ELF não fecha essa descoberta.

<a id="ee-atual"></a>

## 5. EE estático, despacho e overlays

O EE já executa regiões traduzidas para C++ compilado em máquina host. Foram desenvolvidos registros compactos, retomadas em entradas interiores, controle de delay slots e correções de instruções e ABIs observadas durante a investigação.

### 5.1 Trabalho que saiu do cadastro individual de endereços

O pipeline registra conjuntos de entradas e dependências. As ferramentas de laboratório capturam falhas de admissão e preparam casos offline. O ciclo recente gera catálogos e famílias em lote, mantendo fontes por conteúdo para reutilizar objetos.

Quando uma entrada não está coberta no caminho estrito, a falha é registrada. O contrato final não permite esconder essa falha por retorno fictício, interpretação da instrução ou compilação convidada durante a execução entregue.

### 5.2 Bancos EE concretos observados

O catálogo de referência da etapa recente contém **34 bancos e 537.368 entradas** provenientes de casos capturados. Os inputs pertencem ao catálogo, têm hashes e metadados próprios e podem alimentar a descoberta posterior de famílias.

Esse número conta entradas em bancos de bytes observados. Não é fechamento universal, taxa de compatibilidade, quantidade de funções independentes nem evidência de todos os escritores que podem produzir novas versões.

### 5.3 Dependências de entrada normal

As dependências de uma retomada normal foram refinadas para evitar exigir desnecessariamente bytes anteriores à entrada. Esse trabalho preserva a distinção entre uma instrução executada como delay slot e a mesma instrução alcançada diretamente.

As tabelas continuam precisando de um contrato maior para tradução virtual/física, visão de fetch, aliases, publication e contexto arquitetural completo. Guardas sobre RAM recente não bastam para caracterizar o instruction cache do PS2.

<a id="familias-ee"></a>

## 6. Famílias EE e orquestração offline

O frontend `ps2_native_data_family` gera uma função nativa para uma estrutura finita com campos de dados parametrizados. Os opcodes, registradores e campos de controle permanecem fixos. O corpo produzido contém operações compiladas; a verificação de bytes e a leitura de parâmetros não são um decodificador de ISA convidada.

### 6.1 Domínio existente

O perfil reconhece 20 classes de campos de dados low-16, incluindo LUI válida, ADDIU, comparações, operações lógicas e loads/stores inteiros ordinários. Apenas campos autorizados recebem máscara `0xffff0000`; os demais mantêm máscara integral.

O emissor cobre regiões lineares suportadas e regiões terminadas em uma transferência com slot completo. JR/JALR, condicionais inteiras e J/JAL possuem tratamento explícito. Operações não suportadas, campos reservados inválidos e contextos não qualificados são rejeitados.

Para J/JAL, o destino usa o high nibble do PC arquitetural real. O link de JAL é relocável e é atualizado antes do slot. Um destino só é tratado como local depois de comparado com a região efetivamente relocada. Destinos externos usam o diretório do runtime.

### 6.2 Descoberta de estruturas com uma observação

O detector deixou de exigir sempre duas variantes de bytes para propor uma estrutura. A política tipada pode identificar os campos de dados mesmo quando uma constante apareceu apenas uma vez. Essa proposta amplia o trabalho possível do frontend, mas não prova que o produtor só emite aquela estrutura ou que todo parâmetro possível seja admissível no jogo.

### 6.3 Orquestrador existente

`lab/prepare_ee_family_batch.py`:

1. Lê catálogos ou lotes anteriores com identidades verificadas.
2. Prepara novas capturas por meio do frontend offline.
3. Copia e passa a possuir metadados e imagens dos casos.
4. Deduplica casos por identidade com comprimentos enquadrados.
5. Executa descoberta, geração e publicação em um comando.
6. Registra contagens, políticas, hashes, tempo e estágio de falha.
7. Mantém aprovação estrita e fechamento como falsos.

O orquestrador não escolhe sozinho um reparo semântico, não explora a campanha e não aprova a fidelidade. A versão publicada ainda usa raízes e regiões terminais, o que omite continuações lineares reais.

### 6.4 Resultado do lote publicado

| Medida | Resultado e escopo |
|---|---|
| Casos pertencentes ao lote | 37 |
| Propostas terminais | 15.272 |
| Estruturas admitidas pelo frontend | 8.293 |
| Propostas recusadas | 6.979, com motivos registrados |
| Unidades de fonte | 293 |
| Geração paralela do catálogo | 14,809 s no experimento identificado |
| Orquestração com o mesmo ledger e fontes | 18,170 s no experimento identificado |
| Lote posterior à correção de destinos locais | 20,586 s, mesmo ledger numérico |
| Fontes idênticas após essa correção | 134 das 293 unidades |

As medições são locais, sob carga não controlada, e representam etapas offline de estruturas capturadas. Não medem tempo para converter e certificar uma ISO inteira.

### 6.5 Limites e recusas

O catálogo executável publicado suporta até 32.768 famílias finitas. Fontes de corpos têm limites por quantidade e bytes; o índice e o manifest possuem limites próprios. Workers são limitados e os resultados têm ordem determinística. Cópias para o cache têm seus bytes verificados.

Recusar uma estrutura preserva uma obrigação aberta. A recusa não autoriza um stub de sucesso e não significa que aquele código seja inalcançável.

<a id="continuacoes-canonicas"></a>

## 7. Continuações canônicas em desenvolvimento

Esta é a mudança em andamento no working tree no momento do levantamento. Ainda não está incorporada ao último commit de código publicado e ainda não alimentou um novo teste de execução do jogo.

### 7.1 Falha concreta que motivou a mudança

O teste recente parou em `0x1a51be8`. O descritor existente desse PC contém uma dependência linear de 28 palavras. O frontend de síntese aceita essa estrutura; o catálogo terminal não a propôs.

A inspeção do mesmo snapshot mostra que, atravessando a fronteira linear de metadados até a primeira transferência com seu slot, a região tem 36 palavras. Isso permite investigar uma regra geral de região, sem cadastrar aquele PC por título.

### 7.2 Regra proposta e parcialmente implementada

Para cada raiz admitida nos metadados:

1. Examinar no máximo 127 posições iniciais.
2. Se aparecer transferência com delay slot, incluir a transferência e o slot completo: no máximo 128 palavras.
3. Se não aparecer transferência, formar exatamente 127 palavras lineares.
4. Se a janela acabar antes de determinar uma região completa, registrar truncamento e não publicar um prefixo curto.
5. Manter entrada de raiz e parametrização tipada; políticas incompatíveis são rejeitadas.

A separação de comprimento evita aceitar simultaneamente uma região linear curta e sua extensão terminal na mesma raiz. Os bits fixos de controle fornecem a distinção entre uma forma terminal e a forma linear de comprimento fixo. O frontend continua responsável pela decisão de suporte semântico.

### 7.3 Evidência e problema aberto desta tentativa

- Inventário de **102.145 raízes** nos 37 casos.
- **2.445.974 palavras examinadas** no inventário.
- **43.452 estruturas distintas** antes da aprovação do frontend.
- **10.223 regiões lineares de 127 palavras** e **91.739 regiões terminais** no inventário de raízes.
- **183 janelas lineares truncadas**, preservadas como lacunas.
- Detector com **28 testes unitários passando**, incluindo limites e incompatibilidades de políticas.
- A tentativa de serializar o relatório excedeu o limite de **64 MiB** e foi interrompida antes da publicação.

Não existe catálogo executável aprovado com essas 43.452 propostas. A publicação, os limites entre propostas e famílias executáveis, a integração no publisher/orquestrador e a ausência de ambiguidades no corpus precisam ser resolvidos e validados. O limite de propostas em desenvolvimento não amplia automaticamente o orçamento do runtime.

### 7.4 Trabalho seguinte

Reduzir duplicação da proveniência ou particionar a publicação com identidades verificáveis, manter limites explícitos e testar o fluxo inteiro. Depois: gerar catálogo, comparar fontes, construir incrementalmente, executar probe e repetir a rota headless. O resultado deve determinar a próxima classe de falha; não se deve assumir gameplay só porque o novo PC encontrou correspondência.

<a id="iop-atual"></a>

## 8. IOP nativo e serviços

O IOP saiu de uma situação exclusivamente diagnóstica para ter um caminho AOT de laboratório integrado ao runtime. O frontend IRX emite kernels nativos especializados e famílias relocáveis; o dispatch verifica instruções e dependências de imports no modelo testado.

### 8.1 Implementação e experimentos

- Frontend offline IRX e startups de módulos originais.
- Famílias relocáveis, com operandos e identidades explicitamente admitidos.
- Catálogo multi-módulo e adaptador de runtime nativo.
- Guards para import stubs, export/import versions e dependências de serviço.
- Carregamento e scheduling combinados de módulos.
- Captura de inputs de módulos, pedidos RPC e resultados observados.
- Separação do alvo de seleção do backend para reduzir rebuilds.

O catálogo local recente contém **3.947 kernels** e um conjunto finito de módulos observados. No experimento de jogo registrado, houve **30.425.512 operações nativas** no último checkpoint RPC, zero operações interpretadas nesse contador e nenhum evento de falha nativa observado naquela rota.

### 8.2 Escopo das comparações

Startups, RAM e campos identificados foram comparados com o modelo diagnóstico em fixtures e probes. Isso verifica regressões naquele domínio. Não transforma o modelo diagnóstico em referência independente do R3000A, nem fecha todos os serviços, versões, substituições de módulo, eventos ou estados ocultos.

Os contadores da rota executada não provam que todos os módulos possíveis ou todos os caminhos de RPC foram compilados. Solicitações com bytes parecidos também precisam de contexto, ordenação e efeitos corretos.

### 8.3 Pendências de M4

Fechar ciclo de vida e ownership dos módulos, domínios de imports e HLE, timing de serviços, estado canônico completo, cobertura alcançável e ausência de interpretação em todos os caminhos do pacote final. O perfil geral `build/` está novamente selecionado para IOP diagnóstico; os artefatos dos experimentos nativos têm identidades separadas.

<a id="vu-atual"></a>

## 9. VU AOT, replay e referência independente

O laboratório possui VU AOT conservador, snapshots versionados, captura de microcode/dados, binding de bancos nativos e replay através do VIF real do runtime. Há tratamento de estados ocultos de pipeline e continuações, incluindo mudanças relacionadas a XGKICK.

### 9.1 Caso original exercitado

Um caso VIF original de Monster House contém nove callbacks VU e dois bancos completos de 16 KiB. Replays nativos da coleção reproduziram os estados e eventos registrados no domínio do modelo e falham se um wrapper de interpretação genérica for chamado naquele caminho.

O caso inclui VIF, VU, GIF e GS CPU. Essa integração é mais ampla que executar uma instrução isolada, mas continua sendo uma chamada observada, com domínio finito e origem diagnóstica. Não corresponde à execução nativa de todo o jogo.

### 9.2 Reuso de banco e construção

A documentação registra compilação dos bancos e CLI em 23,489 s, com seis compilações de fonte e nenhuma recompilação de EE gerado naquela medição. Uma conversão repetida da coleção levou 0,348 s; o replay build inalterado levou 0,307 s. São custos de reuso local daquele conjunto.

### 9.3 Referência independente identificada

O laboratório integra um engine VU separado do PCSX2, com revisão e fontes fixadas por hash e isolamento da entrega. A revisão registrada é `94d86c891b1621c0b252e4fc2e155bf90274dcc0`.

No caso inicial comparado:

- 168 payloads/caminhos GIF concordam.
- Bytes finais de VIF, GIF, GS CPU, microcode e dados VU concordam.
- Registradores projetados, flags e PC concordam.
- O tempo VU diverge: **3.777 ciclos na referência e 4.024 no modelo**, diferença de **247 ciclos**.

O tracing completo encontrou **3.761 duplas de instrução iguais** e decompôs a diferença em **96 ciclos de issue e 151 ciclos de tail**. A divergência foi mantida. Não se ajustou relógio por uma constante para forçar concordância.

### 9.4 Limites dessa independência

O VIF, a arbitragem GIF e o GS CPU da comparação têm componentes compartilhados. A relação entre estados ocultos não está qualificada para todos os estados, e o domínio inicial é restrito. Há diferenças de modelo de stalls e de drenagem de XGKICK que exigem evidência temporal apropriada.

Concordar com PCSX2 não é, automaticamente, concordar com hardware PS2. Uma divergência entre implementações deve ser caracterizada, reduzida e relacionada à especificação e às observações relevantes.

### 9.5 Pendências de M5

Expandir programas, operações e estados iniciais; cobrir ambos os VUs; integrar o AOT ao jogo completo; validar uploads, suspend/resume e pipelines ocultas; caracterizar timing e executar em outra arquitetura. O caso de replay não fecha sozinho M5.

<a id="dispositivos-atuais"></a>

## 10. GS, gráficos, áudio, controles e saves

### 10.1 GS existente

O GS principal trabalha com rasterização CPU e com a implementação de memória local do runtime. A apresentação host usa o caminho gráfico disponível, mas apresentar uma textura por OpenGL não demonstra que a rasterização GS foi transferida para a GPU.

Há snapshots e replays de VIF/GIF/GS/VRAM e uma separação do kernel CPU em alvo privado de build. Faltam referência independente do GS e aprovação de formatos, readbacks, transferências, aliasing, feedback e sincronização no conteúdo integral.

### 10.2 Medição do caso VIF original

Em dez repetições instrumentadas de uma chamada, o receiver GS consumiu 247,990 ms de 250,813 ms de wall time. A parcela exclusiva dos callbacks VU foi 1,296 ms. Esses valores descrevem aquela chamada sob aquela carga; não são percentuais do jogo inteiro.

No ensaio pareado CPU GS `-O1` versus `-O2`, com contração FP desabilitada, as medianas foram 251,435 ms e 212,890 ms, com os estados/eventos comparados idênticos. O resultado mostra uma melhoria local de compilação, sem aprovar semântica GS nem 60 FPS.

### 10.3 Áudio e vídeo

Existem serviços, correções e caminhos de FMV/IPU/runtime. A correção de AutoDMA e casos de decodificação receberam regressões. A aprovação de áudio exige verificar dados, refill, ordem, continuidade, sincronização e escuta em condições apropriadas. Um sink silencioso protege o desktop durante o teste, mas não certifica áudio audível.

No Android, integrações de vídeo e dependências precisam ser exercitadas no destino; a configuração documentada não equivale a todos os codecs e FMVs funcionando.

### 10.4 Controles

Foram corrigidos o ABI de bytes do pad, o mapeamento de teclado e neutralização de eixos opostos. Entrega de um pacote de input não demonstra que o personagem se moveu corretamente. A validação final precisa relacionar entrada, mudança de estado do jogo, câmera e progresso.

Touch e controles Android também precisam de testes próprios de ciclo de vida, foco e comportamento.

### 10.5 Saves

Há observações de criação, leitura e carregamento do save inicial e regressões de consultas de diretório de memory card. As execuções headless arquivam mudanças e restauram os arquivos originais por hash.

Saves de toda a campanha, slots, sobrescrita, conteúdo corrupto, reinício, persistência e interrupção durante escrita ainda não foram aprovados. Restaurar arquivos após um teste protege o estado local; não prova que a implementação de save seja universalmente correta.

<a id="desempenho-atual"></a>

## 11. Compilação, desempenho e uso da GPU

O ciclo de desenvolvimento usa `-O1 -fno-lto`, com otimizações privadas onde há medição. IPO/LTO de release tem gate separado. O objetivo é recompilar os fontes afetados e preservar os objetos do jogo quando a mudança pertence ao runtime ou à orquestração.

### 11.1 Medições recentes de build

| Etapa | Tempo registrado | Limite da conclusão |
|---|---:|---|
| Configuração CMake antes de extrair índices JSON | 123,979 s | Lote e host identificados |
| Configuração após extrair índices de fontes/hashes uma vez | 6,891 s | Mesmo tipo de lote; carga não controlada |
| Compilação nativa fria do lote de famílias | 322,753 s | 293 unidades; não é toda a conversão da ISO |
| Rebuild do catálogo idêntico | 0,816 s | Zero fontes recompilados |
| Link do runner com objetos EE existentes | 17,403 s | Nenhuma recompilação do jogo original naquela etapa |
| Lote offline posterior à revisão de J/JAL | 20,586 s | Geração e publicação, sem novo run de jogo |

### 11.2 Por que os custos são diferentes

Inspecionar disco, analisar código, emitir C++, compilar código host, descobrir uma nova versão, corrigir semântica e validar a campanha são trabalhos distintos. Ganhar segundos no link não resolve uma instrução traduzida incorretamente nem fecha um produtor de código desconhecido.

### 11.3 CPU, GPU e VRAM

O pipeline C++ existente compila e liga no CPU. A GPU pode ser um alvo importante para GS e kernels admitidos pelo contrato, mas não existe neste levantamento um compilador C++ movido à GPU ou uma conversão universal acelerada pela VRAM.

O uso da GPU precisa de backend, contratos de memória e ordem, medição de custos de transferência/sincronização e validação dos resultados. Aumentar paralelismo sem considerar memória, custo dos jobs e cache pode piorar o tempo total.

### 11.4 Meta de minutos

Conversões repetidas de conteúdo já conhecido podem aproveitar cache. Uma conversão fria de uma ISO inédita com descoberta, provas, reparo e campanha integral não foi demonstrada em dez minutos. Os números de etapas rápidas precisam conservar essa distinção.

### 11.5 60 FPS

Não existe medição atual que aprove 60 FPS de gameplay sustentados. A taxa de apresentação, o ritmo da lógica, o tempo convidado e o tempo host são frequências diferentes. Alterar apresentação ou o clock sem preservar física, animação, scripts e áudio não resolve o contrato.

O relatório histórico conserva uma amostra ruim de apresentação em Xvfb/llvmpipe. Ela não é um benchmark de GPU física e também não pode ser descartada para anunciar um jogo liso.

<a id="evidencia-atual"></a>

## 12. Testes, experimentos e isolamento headless

### 12.1 Última regressão ampla registrada

- **78/78 grupos CTest** passaram.
- **487/487 casos da suíte C++ geral** passaram.
- A fixture de famílias comparou **1.518 entradas normais**, em **387 fixtures e 47 estruturas**.
- Os **34 outputs convencionais de overlay** permaneceram byte a byte idênticos com a mesma receita default de entradas normais.
- O lote fresco pós-correção preservou o ledger de 8.293 famílias.

Esses resultados pertencem à revisão publicada em `d70a168` e aos artefatos identificados. A alteração canônica posterior do detector recebeu 28 testes unitários, mas não substitui uma nova regressão integrada da mudança completa.

### 12.2 O que cada camada prova

| Camada | Evidência útil | Limite |
|---|---|---|
| Parsing, hashes e schema | Integridade, domínio e rejeição de inputs inválidos | Não prova semântica do jogo |
| Geração e compilação | Fonte válido e código host construído | Não prova equivalência da tradução |
| Mesmo emissor/modelo | Regressões, ordem e estado comparado | Pode compartilhar um erro semântico |
| Referência identificada separada | Diferenças entre implementações no domínio admitido | Pode ter aproximações e componentes compartilhados |
| Execução headless | Progressão, captura e input na rota observada | Não cobre todos os caminhos |
| Hardware/plataforma física | Evidência de comportamento e desempenho no destino | Exige domínio, duração e identidade apropriados |
| Corpus inédito | Generalização e autonomia dentro do protocolo | Não implica biblioteca inteira sem denominador válido |

### 12.3 Comparação EE

As fixtures comparam os campos do codec EE identificado e os 32 MiB de RAM entre família e tradução conservadora. Delay slots, entradas normais, links e operandos têm casos específicos. O estado comparado não é um checkpoint completo de toda a máquina PS2.

### 12.4 Probe de admissão

O probe recente consultou 14.483 endereços preparados: 3.456 `Ready`, 11.027 `MissingEntry` e zero `Ambiguous`, em 0,179 s. Foram contabilizadas 5.866.982 verificações de candidatos.

Essas consultas incluem posições especulativas da janela e contextos construídos. O probe não executou gameplay. `Ready` não é percentual de cobertura alcançável, e ausência de ambiguidade nesse conjunto não prova ausência em todo estado possível.

### 12.5 Isolamento

O harness utiliza display Xvfb privado de número 90 ou superior, Xauthority própria e identificação do processo dono. Screenshots e input pertencem à sessão validada. O áudio vai para sink temporário silencioso. O desktop do usuário não é alvo de navegação nem de captura.

Processos de teste só podem ser encerrados após verificação de ownership. Arquivos de memory card alterados são arquivados e restaurados conforme os hashes originais. Os relatórios registram término, timeout e falhas como estados distintos.

### 12.6 O que ainda não foi validado

Campanha, fidelidade independente integral, estado completo da máquina, execução ARM64 física, GS independente, áudio completo, estabilidade sustentada, cobertura de todos os produtores e autonomia sem assistência continuam sem aprovação.

<a id="monster-house-atual"></a>

## 13. Processo realizado com Monster House

Monster House é o primeiro caso de engenharia e validação. O objetivo do PS2Native continua sendo a ferramenta reutilizável, não um conjunto de patches restritos a esse jogo.

### 13.1 Identidade do input

- Input local: `Monster House (BR-USA) (T2.0) (www.romsportugues.com).iso`.
- SHA-256 registrado: `cb596f3249f48e392bc20ec04cdf29074805ae6ef8469c3c18b1abca0c865f74`.
- Boot identificado: `SLUS_214.00`.
- O inventário inicial contém 12 conteúdos ELF únicos: um boot e 11 outros MIPS ELF.

Os números do relatório estático inicial são mantidos nos anexos como histórico. Eles não são reclassificados como contadores ativos de todos os erros atuais.

### 13.2 Correções e capacidades extraídas do processo

| Classe | Trabalho realizado | Validação ainda necessária |
|---|---|---|
| Chamadas/retomadas | Registros em lote, entradas interiores, guards e bancos offline | Fechamento de todos os destinos |
| MMI/FPU | Casos e correções de operandos, aliasing e aritmética observada | ISA e flags completas |
| ABI libm | Argumentos/retornos double e cadeia da câmera | Outras ABIs e caminhos |
| IOP | Serviços, IRX AOT, imports e RPC | Domínio completo, timing e estado oculto |
| VIF/GIF/DMA | Fragmentação, formatos e continuidade de streams | Todos os canais, modos e interleavings |
| VU | Bits crus, dependências VI, AOT, continuações e XGKICK | Ambos os VUs e execução integral |
| Input | Ordem de bytes do SDK e mapeamento de teclado | Movimento causal e Android |
| Memory card | Consultas de diretório e save inicial | Persistência e campanha completas |
| GS | Replay, snapshots e otimização CPU | Renderização acelerada e fidelidade integral |
| Ferramentas | Capturas, replay, headless, cache e orquestradores | Reparador e explorador universais |

### 13.3 Último teste de jogo do lote nativo recente

A rota passou por `0x1a51b70`, `0x1a51b9c` e `0x1a51bb4`, chamou regiões já cobertas e parou em `0x1a51be8`. Durou 189,480 s e terminou durante loading. O driver utilizou templates finitos de menu; não foi um explorador universal.

O runner arquivado foi construído antes da correção final de destinos J/JAL locais. O catálogo produzido depois dessa correção não foi usado em um novo run de jogo. Inputs, binários e traces dessas etapas precisam permanecer diferenciados.

Os processos próprios da execução terminaram. As mudanças de memory card foram arquivadas e os cinco arquivos originais foram restaurados por hash.

### 13.4 Resultados históricos mais avançados

O relatório anterior descreve vídeos, menus, save inicial, HUD/tutorial e um interior 3D com geometria e personagens. O mesmo relatório registra defeitos de renderização, ausência de prova de movimento causal e baixo desempenho no ambiente observado.

Esses resultados continuam sendo úteis como regressão do caminho diagnóstico. Não fecham o caminho AOT mais estrito, a campanha, M6, M7, M8 ou a biblioteca universal.

<a id="plataformas-atuais"></a>

## 14. PC, Android e auditoria de natividade

### 14.1 PC

O alvo desktop constrói para a ABI do host. O caminho exercitado é Linux x86-64. O pacote inclui runner, dados do disco, manifest e wrapper de lançamento. Templates ou código de portabilidade não equivalem a execução aprovada no Windows, macOS ou outras arquiteturas.

### 14.2 Android

O builder prepara projeto por título, fontes gerados, wrapper CMake, assets e `Ps2PackageActivity`. A Activity copia os dados para armazenamento privado antes do native startup. O runtime resolve o boot nesse diretório.

A configuração documentada pede ARM64-v8a, JDK 17, Gradle, SDK/NDK/CMake Android e a ponte Activity. Ausência de pré-requisitos produz diagnóstico e caminho do projeto staged; não autoriza declarar um APK construído.

Precisam ser exercitados: ARM64/SIMD, `sse2neon`, bibliotecas, vídeo, áudio, input/touch, suspensão/retomada, armazenamento, memória, assinatura, tamanho de assets e desempenho térmico. Split/OBB e distribuição de dados grandes não estão aprovados como solução pronta.

Não foi localizada evidência que aprove a instalação e a execução sustentada deste jogo em um aparelho Android físico.

### 14.3 Perfil selecionado no diretório geral de build

O levantamento do `build/CMakeCache.txt` registra:

```text
PS2X_BUILD_NEXO_LAB=ON
PS2X_ENABLE_RELEASE_IPO=OFF
PS2X_FAST_ITERATION=ON
PS2X_IOP_ENABLE_INTERPRETER=ON
PS2X_RUNTIME_AOT_EE_OVERLAYS=OFF
PS2X_RUNTIME_EE_DATA_FAMILIES=OFF
PS2X_RUNTIME_NATIVE_IOP=OFF
```

Essa seleção é diagnóstica para regressões. Ela não identifica automaticamente os perfis usados pelos runners nativos congelados. As bibliotecas e executáveis de cada experimento têm seus próprios manifests.

### 14.4 Auditoria final necessária

A ausência de um símbolo de interpretador é um teste auxiliar. A aprovação precisa combinar componentes permitidos, proveniência das operações, dependências, paths alcançáveis e traces do pacote final. O aplicativo entregue não pode chamar frontend convidado, produzir código convidado executável ou resolver uma ausência de AOT interpretando a ISA.

HLE e dispositivos podem ser implementados nativamente. Precisam preservar ABI, efeitos, memória, tempo e comportamento observável no domínio admitido.

<a id="contratos-abertos"></a>

## 15. Memória, fetch, tempo e checkpoints

### 15.1 Identidade de código

O contrato NEXO inclui processador, endereço virtual, tradução física, modo, contexto de entrada, visão de fetch, versão do código, perfil semântico e dependências. Um PC e os bytes mais recentes de RAM não representam tudo isso.

### 15.2 Escritores e publicação

Stores EE/IOP, DMA, loaders, HLE e uploads VU precisam convergir para um protocolo consistente. Os efeitos devem ser registrados, tornados visíveis no momento correto, invalidar dependências e selecionar código AOT já coberto.

Uma escrita que altera instruções ainda por executar exige saída na fronteira apropriada. Revalidar apenas a próxima chamada não resolve automodificação dentro da região ativa. A integração completa desses contratos ainda não está demonstrada.

### 15.3 Produtores de código

Propor uma estrutura a partir de bytes capturados não prova que um materializador só produz aquela estrutura. É necessário relacionar o domínio de inputs do produtor, os bytes finais, a família nativa e todas as chamadas alcançáveis. Se a estrutura de opcodes é desconhecida, a conclusão deve continuar aberta.

### 15.4 Estado canônico

O projeto tem codecs e envelopes versionados para fronteiras específicas, com endianness, dimensões e checksums. Há casos que abrangem VIF/VU/GIF/GS. A captura EE recente inclui RAM e contexto EE identificado.

Não existe evidência de checkpoint canônico completo de toda a máquina que feche os estados ocultos de todos os processadores/dispositivos, filas, eventos, disco, áudio e ambiente, com replay equivalente em duas arquiteturas.

### 15.5 Tempo

Clocks convidados, ciclos de processadores, filas e eventos precisam manter relações causais. Wall time host serve para desempenho; não substitui tempo arquitetural. Otimizações de espera, fusão ou execução paralela só podem cruzar fronteiras demonstradas seguras.

<a id="marcos-atuais"></a>

## 16. Situação dos marcos M0–M11

**Nenhum marco foi formalmente declarado 100% aprovado neste levantamento.** Há implementação e evidência parcial relevante em vários deles. O critério de saída do README continua sendo a referência; uma coluna com trabalho existente não significa aprovação desse critério.

| Marco | Trabalho existente / direção | Critério ainda sem aprovação integral |
|---|---|---|
| M0 | Evidências congeladas, hashes e separação entre construção e aprovação | Auditoria consolidada de manifests e estados sem ambiguidade em todo o fluxo |
| M1 | Codecs, envelopes canônicos, eventos e replays de fronteiras | Replay equivalente e qualificado em duas arquiteturas |
| M2 | Semântica EE prioritária, entradas normais, delay slots e regressões | Memória, exceções, campos/operadores e diferenciais suficientes sem sucesso fictício |
| M3 | Bancos EE, famílias, ownership e diagnóstico de guards | Escritores, aliases, publicação e mudanças de fetch integrados |
| M4 | IOP AOT, IRX relocável, imports e execução observada | Módulos/serviços alcançáveis sem interpretação com domínio e fidelidade suficientes |
| M5 | VU AOT conservador, replay e continuações | Estado completo, ambos os VUs, timing e integração integral |
| M6 | GS CPU, snapshots, replay e otimização localizada | Imagem, readbacks e transferências aprovados com aceleração fiel |
| M7 | Progressão parcial de Monster House em perfis identificados | Primeiro jogo integral: campanha/modos, áudio, controles e saves em build normal |
| M8 | Pipeline Android e projeto por título | Instalação física, execução sustentada e ciclo de vida aprovados |
| M9 | Famílias finitas e desenho de fechamento/cápsulas | Abrangência dos produtores e fusões demonstradas em casos reais |
| M10 | Capturas e orquestradores limitados | Reparos e exploração de submissões inéditas sem correção humana |
| M11 | Protocolos e plano de corpus | Catálogo congelado, taxas conjuntas, falhas e custos publicados |

Os marcos expressam dependências e gates, não uma fila estritamente linear. Trabalhos de semântica, backend e infraestrutura podem evoluir juntos. A validação final não pode ser substituída por marcar tarefas de implementação como concluídas.

<a id="caminho-m6-m7-m8"></a>

## 17. Caminho até M6, M7 e M8

### 17.1 Prioridade imediata de execução

1. Completar a publicação limitada das regiões canônicas, sem omissão silenciosa.
2. Manter o ledger de recusas e separar propostas de kernels realmente suportados.
3. Validar escolha de família, guards, contexto e efeitos em fixtures.
4. Construir incrementalmente e congelar a identidade do novo runner.
5. Repetir a rota de carregamento headless e registrar o primeiro bloqueio real.
6. Investigar a classe de bloqueio por replay e regressão genérica.
7. Expandir a cobertura alcançável e integrar o caminho VU nativo no jogo.

### 17.2 Para fechar M6

- Definir um caminho GS conservador verificável e o backend acelerado.
- Exercitar local memory/VRAM, swizzles, formatos, readbacks e transferências.
- Preservar aliasing, feedback, máscaras, blending, depth e ordem observável.
- Comparar imagens e estados com referências adequadas.
- Medir o custo real de GPU, uploads, sincronização e fallbacks de dispositivo.
- Reprovar explicitamente casos sem evidência suficiente.

### 17.3 Para fechar M7

- Exercitar todo o primeiro jogo no build que será entregue.
- Validar progressão e condições de conclusão relevantes.
- Verificar controles e movimento causal, câmera e colisão.
- Verificar áudio, vídeos, scripts, transições e sincronização.
- Criar, carregar e persistir saves durante o conteúdo, incluindo reinício.
- Verificar ausência de substituições diagnósticas ou skips de efeitos.
- Registrar crashes, divergências, desempenho e estabilidade sustentada.

### 17.4 Para fechar M8

- Construir o pacote ARM64 com toolchain identificado.
- Validar paridade das operações host e dos artefatos de estado.
- Instalar num aparelho físico e verificar inicialização.
- Exercitar input, armazenamento, áudio/vídeo, background/foreground e retomada.
- Medir memória, temperaturas, consumo, frame pacing e estabilidade.
- Repetir os critérios de conteúdo do pacote de destino.

### 17.5 Prazo

Não há estimativa confiável de data para fechar esses três marcos. A execução atual ainda para no carregamento do caminho recente, e GS, campanha e aparelho físico têm trabalho aberto. Os tempos rápidos de geração/link não sustentam uma promessa de concluir o port perfeito em poucas horas.

O planejamento deve usar duração medida de tarefas delimitadas e atualizar a estimativa depois de reduzir os bloqueios e conhecer a cobertura. Um novo erro semântico pode mudar o caminho crítico. A meta final permanece intacta mesmo sem uma data defensável hoje.

<a id="automacao-atual"></a>

## 18. Automação universal, nuvem e generalização

### 18.1 Automação que já existe

Inspeção, extração, boot selection, chamadas a ferramentas, geração/compilação, staging/publicação, verificação de pacote, preparação de capturas, ownership de casos, descoberta limitada, geração paralela e recibos são automatizados em partes do pipeline.

### 18.2 Automação que ainda falta

O motor geral precisa fechar o ciclo entre falha, redução, hipótese causal, reparo, regressão, verificação independente, seleção e promoção. Hoje, identificar e implementar várias dessas correções ainda exige trabalho de desenvolvimento.

É necessário distinguir uma ferramenta que automatiza uma etapa de uma ferramenta que soluciona autonomamente qualquer título novo. Um template de menu também não constitui um explorador de campanhas desconhecidas.

### 18.3 Motor de reparo pretendido

```text
ISO inédita e inputs congelados
  -> inventário e construção
  -> execução e diagnóstico limitado
  -> primeira divergência causal
  -> redução de caso
  -> hipótese de correção genérica
  -> regressão e verificação
  -> promoção se critérios forem satisfeitos
  -> repetição com orçamento explícito
  -> pacote aprovado ou diagnóstico de obrigação aberta
```

O reparador não pode alterar o oráculo, apagar divergências, encaixar clocks em constantes ou ampliar silenciosamente o domínio admitido para obter sucesso.

### 18.4 Serviço de nuvem pretendido

O plano descreve armazenamento por conteúdo, workers isolados, filas duráveis, cancelamento, checkpoints, orçamento, cache de evidência e entrega por plataforma. Esses componentes precisam existir e ser exercitados antes de se declarar um serviço operacional.

Distribuir compilação reduz parte dos custos. Não fornece automaticamente semântica de hardware, fechamento de código latente ou aprovação de uma campanha.

### 18.5 Mais três ISOs

O ciclo previsto usa três novas submissões com características distintas para revelar dependência do primeiro título. Os títulos e revisões precisam ser congelados antes da avaliação. Registrar separadamente reparos genéricos, perfis, intervenção humana, descobertas, tempo e resultado de cada gate.

Não há neste levantamento três jogos adicionais integralmente convertidos e aprovados. Os anexos conservam o protocolo para esse ciclo.

### 18.6 Avaliação posterior

Depois de estabilizar as primeiras submissões, o catálogo deve incluir diversidade de publishers, engines, IOP, VU, formatos e padrões gráficos, com hardware e critérios publicados. A taxa conjunta precisa exigir todas as condições desejadas; não deve selecionar só títulos que o modelo já viu.

<a id="pesquisa-atual"></a>

## 19. Pesquisa, otimizações e hipóteses

O plano anterior preserva 24 propostas experimentais distribuídas em suas seções de técnicas e pesquisa avançada. O NEXO amplia a arquitetura com materializadores, cápsulas causais, IR de efeitos temporais, reconstrução e fusão condicionada.

Uma hipótese promissora continua sendo hipótese até ser implementada, testada e relacionada ao domínio. Este relatório não atesta ineditismo na comunidade. Ferramentas conhecidas, como e-graphs e verificação de tradução, mantêm sua proveniência nos documentos originais.

### 19.1 Frentes de pesquisa e critérios

| Frente | Benefício pretendido | Evidência necessária |
|---|---|---|
| Fechamento por materializadores | Cobrir código latente sem listar só bytes observados | Invariantes do produtor e abrangência das chamadas |
| Famílias de dados | Reusar código host entre variações de constantes | Predicado, equivalência e domínio dos parâmetros |
| Regiões canônicas | Evitar omissões e prefixos ambíguos | Contrato de extensão, truncamento e seleção |
| Catálogo de versões | Escolher AOT diante de novas versões de RAM | Escritores, aliases e fetch coerentes |
| Primeira divergência causal | Reduzir diagnóstico repetitivo | Checkpoints completos e dependências de efeitos |
| Esperas por eventos | Eliminar loops sem alterar clocks observáveis | Horizonte seguro e equivalência temporal |
| VU V0/V1/V2 | Remover interpretação e otimizar pipeline | Estado oculto, dependências, timing e comparação |
| GS acelerado | Reduzir rasterização CPU | VRAM, feedback, readbacks e ordem preservados |
| Fusão causal | Evitar tráfego e coordenação redundantes | Composição dos contratos e reconstrução de estado |
| Cache por contrato | Reusar tradução, objetos e provas | Identidade de semântica, ABI, inputs e verifier |
| Regressão metamórfica | Detectar classes fora do primeiro input | Transformações que preservam relação conhecida |
| Exploração por novidade | Cobrir conteúdo e falhas úteis | Novidade causal e condições de sucesso verificáveis |
| Planejador por custo | Priorizar o trabalho que reduz o caminho crítico | Custo medido e orçamento de CPU/memória/tempo |

### 19.2 Onde já há implementação relacionada

Bancos offline, fontes por conteúdo, famílias EE, IRX relocável, replay VU, tracing completo de issue, snapshots de dispositivos e profiling aninhado são peças existentes. Cada peça tem escopo descrito; não equivale a realizar a arquitetura inteira.

### 19.3 Promoção de otimizações

Uma otimização deve ser confrontada com baseline e variantes, casos reduzidos, referências e um corpus retido. Registrar ganho, tamanho, custo de prova/compilação e divergências. Aprovação de um caso real é relevante, mas não amplia automaticamente o domínio de validade.

<a id="riscos-atuais"></a>

## 20. Riscos e disciplina de conclusão

| Risco atual | Consequência | Resposta de engenharia |
|---|---|---|
| Cobertura derivada apenas de rastros | Novas rotas encontram código ausente | Analisar produtores e manter obrigações abertas |
| Semântica compartilhada no teste | Dois caminhos concordam no mesmo erro | Referências independentes e testes adversariais |
| Prefixos ou máscaras sobrepostos | Dispatch ambíguo ou seleção errada | Domínios explícitos e validação de compatibilidade |
| Proveniência repetida enorme | Estouro de relatório, memória e tempo | Deduplicação/partição limitada, sem descartar casos silenciosamente |
| Automodificação dentro da região | Corpo nativo executa bytes antigos | Fronteiras de saída e publicação/fetch corretos |
| Timing aproximado | Áudio, física, DMA e callbacks divergem | Clocks/eventos canônicos e investigação causal |
| GPU sem contrato de memória | Imagem aparente correta com readback incorreto | Testar VRAM, aliases, feedback e ordenação |
| Pacote Android apenas staged | Artefato anunciado sem execução real | Toolchain e hardware físico como gates |
| Fixtures feitas para um jogo | Compatibilidade aparente sem generalização | ISOs inéditas e avaliação congelada |
| Pressão por prazo | Hacks e skips viram suposta entrega | Separar diagnóstico, construção e aprovação |

### 20.1 Critérios que não podem ser omitidos

Não contar timeout como conclusão, imagem parcial como campanha, `.complete` de captura como fidelidade, igualdade de hashes como equivalência universal, ausência de símbolo como auditoria integral ou centenas de milhares de entradas como percentual de jogos.

A sequência de desenvolvimento pode ter rough edges. A aprovação final precisa provar os requisitos do objetivo inteiro e deixar explícitas as dependências do domínio.

<a id="mapa-atual"></a>

## 21. Mapa do código e comandos

### 21.1 Arquivos centrais para as frentes recentes

| Arquivo/local | Papel |
|---|---|
| `lab/discover_ee_data_families.py` | Descoberta de formas tipadas e protótipo de regiões canônicas |
| `lab/generate_ee_family_catalog.py` | Conversão offline, agrupamento de fontes e manifest |
| `lab/prepare_ee_family_batch.py` | Ownership, preparação, descoberta e publicação de lote |
| `lab/prepare_ee_miss.py` | Validação de captura EE e preparação de metadados |
| `lab/generate_ee_bank_catalog.py` | Bancos EE concretos e inputs pertencentes ao catálogo |
| `ps2xRecomp/src/lib/native_data_family.cpp` | Validação e síntese C++ de família nativa |
| `ps2xRecomp/src/lib/control_flow_emitter.cpp` | Controle, links, slots e destinos relocados |
| `ps2xRuntime/src/lib/ps2_ee_data_family.cpp` | Ownership dos descritores e admissão de família |
| `ps2xRuntime/cmake/ee_family_catalog.cmake` | Validação e cache de fontes do catálogo |
| `ps2xIOP/src/emulator/core/iop_native.cpp` | Execução IOP nativa e guards do domínio |
| `lab/prepare_pcsx2_reference.py` | Fontes e identidade da referência VU separada |
| `lab/compare_vu_issue_traces.py` | Comparação completa de issue e timing |
| `lab/tests/` | Fixtures, geração e regressões de laboratório |
| `schemas/` | Contratos completos incorporados nos anexos |

### 21.2 Comandos do produto host existente

```sh
cd /home/pedrohs/Downloads/ps2-native-recompiler
python3 -m tools.ps2native inspect --iso "/caminho/jogo.iso" --json-output
python3 -m tools.ps2native build --iso "/caminho/jogo.iso" --target desktop --out /caminho/pacote-novo
python3 -m tools.ps2native verify --package /caminho/pacote-novo
```

Os caminhos são exemplos; a saída precisa ser nova. Executar `build` não equivale a aprovar aquele jogo. O README do builder define requisitos e limites de cada comando.

### 21.3 Lote offline publicado

```sh
python3 lab/prepare_ee_family_batch.py \
  --previous-batch /caminho/lote-anterior \
  --capture /caminho/captura-ee \
  --family-generator build/ps2xRecomp/ps2_native_data_family \
  --overlay-generator build/ps2xRecomp/ps2_native_overlay \
  --output /caminho/lote-novo \
  --workers 8
```

Essa é a interface do fluxo publicado, que ainda usa sua política de regiões terminais. O protótipo canônico não deve ser apresentado como integrado a esse comando.

### 21.4 Construção e regressão de desenvolvimento

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON \
  -DPS2X_FAST_ITERATION=ON -DPS2X_ENABLE_RELEASE_IPO=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -j 4
build/ps2xTest/ps2x_tests
```

Opções nativas EE/IOP/VU e manifests devem ser escolhidos com o contrato do experimento. O diretório de build e seus caches não devem ser confundidos com uma identidade fixa de pacote final.

### 21.5 Inventário de arquivos versionados

O levantamento identificou **482 arquivos versionados**, incluindo **31 documentos Markdown** antes deste novo arquivo. Fontes gerados, ISO, RAM capturada, dados extraídos e referências locais em `build/` não entram nessa contagem.

| Diretório | Arquivos versionados no levantamento |
|---|---:|
| `tools/` | 23 |
| `ps2xAnalyzer/` | 16 |
| `ps2xRecomp/` | 52 |
| `ps2xRuntime/` | 150 |
| `ps2xIOP/` | 67 |
| `lab/` | 100 |
| `schemas/` | 15 |
| `android/` | 9 |
| `docs/` | 4 |

<a id="continuidade-atual"></a>

## 22. Continuidade do trabalho e histórico

### 22.1 Estado do working tree antes da documentação

```text
 M lab/discover_ee_data_families.py
 M lab/tests/test_ee_data_families.py
```

Essas alterações pertencem à tentativa de regiões canônicas. A documentação registra seu estado intermediário. Ela não as transforma em código publicado, catálogo construído ou feature entregue.

### 22.2 Sequência recente de commits

| Commit | Data registrada | Mudança |
|---|---|---|
| [`d70a168`](https://github.com/Pedrohs1771/ps2-native/commit/d70a168) | 2026-10-01T22:17:25-03:00 | Batch EE structure synthesis with bounded offline orchestration |
| [`93c5cbb`](https://github.com/Pedrohs1771/ps2-native/commit/93c5cbb) | 2026-10-01T21:09:56-03:00 | Scale guarded EE family synthesis with stable incremental catalogs |
| [`518891d`](https://github.com/Pedrohs1771/ps2-native/commit/518891d) | 2026-10-01T19:36:29-03:00 | Admit finite native EE data families with guarded runtime rechecks |
| [`c2d6bda`](https://github.com/Pedrohs1771/ps2-native/commit/c2d6bda) | 2026-10-01T18:42:34-03:00 | Synthesize guarded native EE data families with relative PCs |
| [`bcc0330`](https://github.com/Pedrohs1771/ps2-native/commit/bcc0330) | 2026-10-01T17:55:17-03:00 | Discover bounded typed EE data-family candidates offline |
| [`18c43ed`](https://github.com/Pedrohs1771/ps2-native/commit/18c43ed) | 2026-10-01T17:26:32-03:00 | Automate bounded offline EE miss batches with verified owned inputs |
| [`bbe8352`](https://github.com/Pedrohs1771/ps2-native/commit/bbe8352) | 2026-10-01T16:03:01-03:00 | Refine EE guards by normal entry without rebuilding native banks |
| [`3745bec`](https://github.com/Pedrohs1771/ps2-native/commit/3745bec) | 2026-10-01T14:40:22-03:00 | Capture EE guard misses and extend offline banks incrementally |
| [`76a50eb`](https://github.com/Pedrohs1771/ps2-native/commit/76a50eb) | 2026-10-01T12:55:29-03:00 | Compile finite EE overlay banks offline and stop on uncovered versions |
| [`cc519a2`](https://github.com/Pedrohs1771/ps2-native/commit/cc519a2) | 2026-10-01T11:36:58-03:00 | Isolate IOP backend profile builds and document native game observations |
| [`efdf6fa`](https://github.com/Pedrohs1771/ps2-native/commit/efdf6fa) | 2026-10-01T11:03:08-03:00 | Capture IOP module inputs and RPC observations in the laboratory |
| [`860474d`](https://github.com/Pedrohs1771/ps2-native/commit/860474d) | 2026-10-01T09:33:58-03:00 | Guard native IOP imports and exercise combined module scheduling |
| [`b3030c9`](https://github.com/Pedrohs1771/ps2-native/commit/b3030c9) | 2026-10-01T08:46:07-03:00 | Add shared IRX AOT catalogs and native runtime IOP integration |
| [`d163990`](https://github.com/Pedrohs1771/ps2-native/commit/d163990) | 2026-10-01T07:09:26-03:00 | Add relocatable IRX families with strict native operand binding |
| [`4382e41`](https://github.com/Pedrohs1771/ps2-native/commit/4382e41) | 2026-10-01T06:22:12-03:00 | Add offline IRX AOT frontend and validate original HKSIF startup |
| [`2f353d6`](https://github.com/Pedrohs1771/ps2-native/commit/2f353d6) | 2026-10-01T05:16:13-03:00 | Add strict instruction-specialized IOP AOT execution and IRX tests |
| [`a7938e2`](https://github.com/Pedrohs1771/ps2-native/commit/a7938e2) | 2026-09-30T22:08:55-03:00 | Queue a second XGKICK after its upper pair issues |
| [`86780aa`](https://github.com/Pedrohs1771/ps2-native/commit/86780aa) | 2026-09-30T21:41:06-03:00 | Trace complete VU issue histories and isolate callback timing deltas |
| [`7bc1043`](https://github.com/Pedrohs1771/ps2-native/commit/7bc1043) | 2026-09-30T20:53:34-03:00 | Compare original VIF cases with an isolated PCSX2 VU engine |
| [`5522316`](https://github.com/Pedrohs1771/ps2-native/commit/5522316) | 2026-09-30T19:54:49-03:00 | Isolate CPU GS flags for incremental development builds |
| [`f63827f`](https://github.com/Pedrohs1771/ps2-native/commit/f63827f) | 2026-09-30T19:54:49-03:00 | Measure nested host time in original VIF replays |
| [`ea497eb`](https://github.com/Pedrohs1771/ps2-native/commit/ea497eb) | 2026-09-30T19:10:30-03:00 | Replay original VIF captures with cached native bank collections |
| [`2c2ba0f`](https://github.com/Pedrohs1771/ps2-native/commit/2c2ba0f) | 2026-09-30T17:36:04-03:00 | Add transactional VIF GIF and CPU GS checkpoints |
| [`b6c7ca0`](https://github.com/Pedrohs1771/ps2-native/commit/b6c7ca0) | 2026-09-30T16:44:24-03:00 | Add NEXO canonical VU replay and native runtime banks |

### 22.3 Procedimento de retomada

1. Conferir branch, HEAD, working tree e os ponteiros de evidência.
2. Identificar se há processo próprio ainda ativo antes de iniciar outro job.
3. Ler a falha e a identidade do último artefato, sem inferir o estado pelo nome do diretório.
4. Fazer a correção na classe genérica apropriada.
5. Validar os contratos, os testes e o caminho real afetado.
6. Preservar fontes/objetos sem mudanças e compilar o mínimo necessário.
7. Registrar outputs, build profile, limites e obrigações abertas.
8. Publicar alterações revisadas, com dados comerciais e credenciais fora do Git.

### 22.4 Compromisso do objetivo

O trabalho não termina por atingir um número de testes, um tamanho de README ou uma cena. A conclusão precisa satisfazer a meta original e os critérios normativos. Este arquivo é uma consolidação completa da documentação e do estado observado; não é o certificado final do recompiler.

<a id="proveniencia-atual"></a>

## 23. Evidências locais e proveniência

### 23.1 Ponteiros de trabalho

| Ponteiro/local | Conteúdo |
|---|---|
| `build/lab/latest-ee-structure-batch-job.txt` | Lote publicado, validações, fontes e runners arquivados |
| `build/lab/latest-ee-canonical-regions-job.txt` | Inventário do protótipo de regiões canônicas |
| `build/lab/latest-monsterhouse-native-v0-evidence.txt` | Evidências do VU V0 |
| `build/lab/latest-monsterhouse-native-vif-evidence.txt` | Evidências de binding/replay VIF nativo |
| `build/lab/latest-readme-consolidation-job.txt` | Levantamento e verificações deste documento |
| `build/ps2native/.../package/manifest.json` | Pacote experimental e seu assessment histórico |

Os ponteiros `latest-*` são conveniências mutáveis. A identidade histórica pertence ao diretório e aos hashes registrados, não ao nome do ponteiro. Estes outputs ficam locais e podem não existir num clone público.

### 23.2 Recibos específicos consultados

`local-direct-validation.json` registra build, codegen, execução diferencial, CTest e suíte geral com retorno zero. `post-local-target-default-emission.json` identifica a comparação dos 34 outputs convencionais. `post-local-target-batch/report.json` identifica o lote offline fresco. `post-local-target-catalog-comparison.json` separa a identidade anterior da posterior.

Houve uma tentativa de comparação de output com `--legacy-footprints` contra hashes produzidos pela receita default. Ela não tinha inputs equivalentes. O registro posterior utiliza a mesma receita default e mantém essa distinção, em vez de converter o primeiro resultado em uma regressão inexistente.

### 23.3 Snapshot do levantamento

O resumo abaixo vem dos arquivos atuais e dos recibos existentes. Seus hashes identificam artefatos; não constituem provas semânticas.

```json
{
  "captured_at_utc": "2026-10-02T01:36:40.575585+00:00",
  "code_commit_before_report": "d70a1684b72b6b8a1a78d26b46150099034ba594",
  "branch": "codex/ps2-native-recomp",
  "scope": "documentary audit; no full-port approval",
  "strict_approval": false,
  "closure_proved": false,
  "evidence": {
    "latest_full_validation": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/local-direct-validation.json",
      "exists": true,
      "sha256": "5c20cb3b072046b477e44c34d24d2759a4205969103a2d8e3778e08ac5e17e75",
      "fields": {
        "local-direct-build": {
          "returncode": 0,
          "seconds": 14.043858753000677
        },
        "local-direct-codegen": {
          "returncode": 0,
          "seconds": 0.0036985469996579923
        },
        "local-direct-execution": {
          "returncode": 0,
          "seconds": 26.256274268000197
        },
        "final-full-ctest": {
          "returncode": 0,
          "seconds": 37.056389641998976
        },
        "final-general": {
          "returncode": 0,
          "seconds": 6.615851016000306
        }
      }
    },
    "canonical_inventory": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-canonical-regions-1790904002441897956/inventory.json",
      "exists": true,
      "sha256": "274dfc49bd93b3a4812d641d1f51d455c3f7bc49410954b5987b64d116d078dd",
      "fields": {
        "seconds": 2.1080756629999087,
        "strict_approval": false,
        "closure_proved": false,
        "counts": {
          "roots": 102145,
          "scanned_words": 2445974,
          "terminal": 91739,
          "linear_127": 10223,
          "truncated_linear": 183,
          "unique_shapes": 43452
        },
        "next_miss_region": {
          "pc": 27597800,
          "source_words": 36,
          "source_ends": 27597944,
          "last_words": [
            "0x3c0501a4",
            "0x24a55510",
            "0x3c1001a5",
            "0x26101df0",
            "0x200f809",
            "0x0"
          ],
          "case": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/post-local-target-batch/cases/0aed45cbf3b800783964ddd20b99970e0cf4f0e1113533fc3757e11a2374d1a9",
          "new_terminal": true
        }
      }
    },
    "latest_family_batch": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/post-local-target-batch/report.json",
      "exists": true,
      "sha256": "83e87d7b738ebaf2db818646b720d9995f0f3d17d49c70affc5f9eedc58eeb17",
      "fields": {
        "status": "PUBLISHED_LABORATORY",
        "family_count": 8293,
        "source_count": 293,
        "owned_cases": 37,
        "candidate_count": 15272,
        "declined_structures": 6979,
        "seconds": 20.585734391999722,
        "strict_approval": false,
        "closure_proved": false
      }
    },
    "fresh_family_comparison": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/post-local-target-catalog-comparison.json",
      "exists": true,
      "sha256": "e9ef52bd94ba25a1850c54f5dd08e6bcef3e6955cd0211a93c6989fb667fd689",
      "fields": {
        "ledger_identical": true,
        "unchanged_source_units": 134
      }
    },
    "latest_default_overlay_comparison": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/post-local-target-default-emission.json",
      "exists": true,
      "sha256": "7e468cdf9bc3ff759dfde1e90d656b4546e60ac83d92cba667e54c05b3f6b5e7",
      "fields": {
        "seconds": 1.034769072999552,
        "all_identical": true
      }
    },
    "native_archive": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-structure-batch-1790899954237457454/native-binaries/manifest.json",
      "exists": true,
      "sha256": "2a653dae8699df3dc10797afa5f731ff110662a70d3b8cc5d1e1d0a2dd558337",
      "fields": {
        "strict_approval": false
      }
    },
    "iop_catalog": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/iop-live-capture-1790862066587585134/catalog/catalog.json",
      "exists": true,
      "sha256": "1499a3b14d967568fddc7bcf47933cb46cb27f1b4e2c86ce65fb16190e006ac6",
      "fields": {
        "kernel_count": 3947
      }
    },
    "legacy_ee_catalog": {
      "path": "/home/pedrohs/Downloads/ps2-native-recompiler/build/lab/ee-miss-1790871750868608333/catalog/catalog.json",
      "exists": true,
      "sha256": "0d6118c7c934b75c9dd044f96f412d6ad6eb3103201b126216cc07c5a6154f2b",
      "fields": {}
    }
  }
}
```

### 23.4 Documentos originais incorporados

| Anexo | Original | Linhas | Bytes | SHA-256 dos bytes originais |
|---|---|---:|---:|---|
| [1](#anexo-01) | [README.md](README.md) | 1532 | 95589 | `041e38dbe28d29fdf2672f7bd881ae7c49b0c7c8bf1c2978564bf0e5592438fa` |
| [2](#anexo-02) | [README_AUTOMACAO_UNIVERSAL.md](README_AUTOMACAO_UNIVERSAL.md) | 2756 | 146446 | `378861bf8d2ad2308eaf69b278073d0fe0e2ab4dbf51071fafecaf60da1ee793` |
| [3](#anexo-03) | [RELATORIO_PS2NATIVE_ESTADO_ATUAL.md](RELATORIO_PS2NATIVE_ESTADO_ATUAL.md) | 582 | 52228 | `cb4eceb264c56f20b5bd37391f61cb17c4f4a19ffc077cf09c474691e52b5936` |
| [4](#anexo-04) | [PROJECT_SPEC.md](PROJECT_SPEC.md) | 156 | 12572 | `ae42f29ca50b459bce0f9854acc41f577404f594f72bf0efbf60be7182152468` |
| [5](#anexo-05) | [lab/README.md](lab/README.md) | 1123 | 70422 | `1cd8dd7482e0746ba67320b1ee574ae533023634dcbf501006ec0e907fda36d7` |
| [6](#anexo-06) | [lab/PCSX2_VU_REFERENCE.md](lab/PCSX2_VU_REFERENCE.md) | 214 | 11967 | `28787716badabb6f98df3fce3cb908c6eb52c7baf4d651ade8daf7b258823114` |
| [7](#anexo-07) | [tools/ps2native/README.md](tools/ps2native/README.md) | 124 | 7562 | `625434f1bd747291c79364e9a316d637db0ef2ff1152a0992670687f5c323727` |
| [8](#anexo-08) | [tools/iso_inspect/README.md](tools/iso_inspect/README.md) | 42 | 4794 | `b1fe117b795523561161cda37fc90635e51205a35bd275737d38589cca804473` |
| [9](#anexo-09) | [docs/FAST_NATIVE_ITERATION.md](docs/FAST_NATIVE_ITERATION.md) | 654 | 37581 | `86f38a0272f3aef1074ed5560787663947d9738d6fd507875a9adc1a6be588ca` |
| [10](#anexo-10) | [docs/NATIVE_HANDOFF.md](docs/NATIVE_HANDOFF.md) | 116 | 9526 | `16e1a4f5362fceb004757ec4dc2611f1c3ec375877ae1b07baba44ed9d97698a` |
| [11](#anexo-11) | [docs/ANDROID_PIPELINE.md](docs/ANDROID_PIPELINE.md) | 188 | 20388 | `d2939d7c41d614fd89124e571b3d72441f6ea85339f7e8405879b4e6e5513a28` |
| [12](#anexo-12) | [docs/RECOMPILER_GAPS.md](docs/RECOMPILER_GAPS.md) | 74 | 16480 | `167fe59ac6decdf2bcddeb5b00f4ed95ca64fac1a2d997c6bf841277c79b4067` |
| [13](#anexo-13) | [android/README.md](android/README.md) | 41 | 2012 | `7ed9c4f1ef5c1eef258b3d00265f73b839dba6f22fcf851f24c8825299e20968` |
| [14](#anexo-14) | [ps2xIOP/README.md](ps2xIOP/README.md) | 134 | 7274 | `3675db68978b2d72b3ecdaeebeea07a8eee2700446a48c8b50ae9f389aa5e521` |
| [15](#anexo-15) | [ps2xRuntime/Readme.md](ps2xRuntime/Readme.md) | 77 | 2869 | `82120aefb2db56b10af603502a834b82a5d9ebd8fa2237946744855b358ed2d4` |
| [16](#anexo-16) | [ps2xAnalyzer/Readme.md](ps2xAnalyzer/Readme.md) | 85 | 4169 | `ced2830b4973818cf959210d4ac03971c000f8634177b39b88bec93b69ce877a` |
| [17](#anexo-17) | [schemas/nexo-device-state-v1.md](schemas/nexo-device-state-v1.md) | 152 | 8377 | `9c72b971ddc33a0092266eda044ca38bbeab38ccdd4adfac327c234b01db3ad5` |
| [18](#anexo-18) | [schemas/nexo-ee-aot-v0.md](schemas/nexo-ee-aot-v0.md) | 119 | 6666 | `7ee40403e17ddb6d80d4afbde5822ddf0c8a30af72b8fe164a29b5f4a0388013` |
| [19](#anexo-19) | [schemas/nexo-ee-data-family-candidates-v1.md](schemas/nexo-ee-data-family-candidates-v1.md) | 120 | 6962 | `64ad64f5686438fda12d021a39a8a57f612342a9ebff07d8524f6743ab8b396c` |
| [20](#anexo-20) | [schemas/nexo-ee-entry-dependencies-v1.md](schemas/nexo-ee-entry-dependencies-v1.md) | 115 | 6831 | `8fcfb8417aa7aa3574da0bd5f02897eeeb6e74a29af4e565ad50d6174e2864fa` |
| [21](#anexo-21) | [schemas/nexo-ee-family-catalog-v0.md](schemas/nexo-ee-family-catalog-v0.md) | 149 | 8955 | `6848ad74b3aac8982b0a96b36576a2adbc969a07b5be59993c7d5b654dc32d9c` |
| [22](#anexo-22) | [schemas/nexo-ee-miss-batch-v1.md](schemas/nexo-ee-miss-batch-v1.md) | 57 | 3507 | `cc6a0ad21a278af94197d5c09fb3f6431f79fa49f51eedb766367ff879c38130` |
| [23](#anexo-23) | [schemas/nexo-ee-miss-v1.md](schemas/nexo-ee-miss-v1.md) | 153 | 8983 | `3d3d8d63da039d7c22ac8c85fca319e81f47cd605591a09c8d1581fd6d1597cd` |
| [24](#anexo-24) | [schemas/nexo-ee-native-data-family-v0.md](schemas/nexo-ee-native-data-family-v0.md) | 131 | 7294 | `0bd5c9b5e8fb9fd77cd4ec618ffcd543eb2c99f7aaa92ea6dbdd084f8051032a` |
| [25](#anexo-25) | [schemas/nexo-iop-aot-v0.md](schemas/nexo-iop-aot-v0.md) | 194 | 11397 | `3aacfbeda31b6efbf21775b190a32caefb34a3e7f0bfad1c303d46514ca49eda` |
| [26](#anexo-26) | [schemas/nexo-observed-vif-case-v1.md](schemas/nexo-observed-vif-case-v1.md) | 162 | 8881 | `b0634250b441d47d7948fc6b16bf3e6e5030ca1ee575eccf3c1666dca83944df` |
| [27](#anexo-27) | [schemas/nexo-vu-aot-v0.md](schemas/nexo-vu-aot-v0.md) | 40 | 2487 | `f5e0d68e2efbda53342b9a1bd0cdd39cb8fcefbea355932932457d209af1a1ae` |
| [28](#anexo-28) | [schemas/nexo-vu-capture-v1.md](schemas/nexo-vu-capture-v1.md) | 48 | 2573 | `e7511d58280ccf6f30171964938a130b062d90654e0549ad011a8e603958744e` |
| [29](#anexo-29) | [schemas/nexo-vu-runtime-binding-v0.md](schemas/nexo-vu-runtime-binding-v0.md) | 60 | 3367 | `fb3deec03d25387f6e18c19f355fc6e7f859f863be8c1bcaab5e4a2400154819` |
| [30](#anexo-30) | [schemas/nexo-vu-state-v1.md](schemas/nexo-vu-state-v1.md) | 88 | 4834 | `3d49bc5660b3acada362420204adcd076a55bfce94364afdd29bf02ea118d9f1` |
| [31](#anexo-31) | [schemas/nexo-vu-state-v2.md](schemas/nexo-vu-state-v2.md) | 45 | 2466 | `a39f4d146a41485aa31e8c3308ddf55f273e4b4a7ed63f95a862befeb3351a4e` |

As versões e contagens são as existentes no levantamento. O plano anterior tem 2.756 linhas, embora uma mensagem histórica tenha citado 2.745. O NEXO atual tem 1.532 linhas. Os originais permanecem como fontes separadas no repositório, e suas cópias completas estão a seguir neste único arquivo.

<a id="glossario-atual"></a>

## 24. Glossário

| Termo | Significado neste projeto |
|---|---|
| AOT | Tradução/compilação antes da execução do aplicativo entregue |
| EE | Emotion Engine/R5900 e seu estado arquitetural |
| IOP | Processador R3000A, kernel, módulos e serviços relacionados |
| VU | Unidades vetoriais e microprogramas, com pipeline e estado oculto |
| GS | Graphics Synthesizer e sua memória/efeitos |
| VIF/GIF | Fronteiras e protocolos de transporte relevantes a VU/GS |
| IRX | Módulo executável do ambiente IOP |
| HLE | Implementação host de serviço, que precisa preservar seu contrato |
| Família | Conjunto finito/parametrizado admitido por predicado explícito |
| Materializador | Código que produz, carrega ou transforma código executável |
| Guard | Verificação dos pressupostos de uma tradução ou família |
| Fetch | Visão de instruções efetivamente observada pelo processador |
| Closure/fechamento | Abrangência demonstrada dos destinos e versões alcançáveis no domínio |
| Checkpoint | Estado canônico de uma fronteira com escopo e identidade definidos |
| Replay | Reexecução de inputs e estado identificados para comparação |
| Campo low-16 | Constante/offset de dados nos 16 bits inferiores admitidos |
| Delay slot | Instrução com relação arquitetural ao branch anterior |
| Entrada normal | Execução direta de uma instrução sem reclassificá-la como slot pendente |
| `Ready` | Correspondência/admissão de uma estrutura no probe ou runtime |
| `tested_only` | Evidência de teste no domínio observado, sem qualificação completa |
| Zero-shot | Avaliação de submissão inédita sem ajuste humano específico durante o teste |
| Build aprovado | Construção satisfeita no seu escopo; ainda distinto de gameplay aprovado |
| Port completo | Entrega que passou pelos critérios de conteúdo, natividade, correção e plataforma |

<a id="documentacao-integral"></a>

## 25. Documentação original completa

Os anexos seguintes incorporam integralmente os 31 documentos Markdown encontrados antes da criação deste consolidado. Não são resumos. Cada anexo informa sua origem e o hash dos bytes originais.

Os links relativos dentro de um anexo pertencem à localização original indicada no cabeçalho. Algumas referências e comandos descrevem snapshots históricos. Consultar a síntese atualizada e os recibos identificados antes de usar esses trechos como estado atual ou procedimento de execução.

O plano NEXO atual aparece primeiro, seguido pelo plano universal anterior, o relatório anterior, a especificação e a documentação operacional. Os contratos e os componentes completam o conjunto. As cópias preservam o conteúdo original; o código das fontes não é anexado ao README.

1. [README.md — 1,532 linhas](#anexo-01)
2. [README_AUTOMACAO_UNIVERSAL.md — 2,756 linhas](#anexo-02)
3. [RELATORIO_PS2NATIVE_ESTADO_ATUAL.md — 582 linhas](#anexo-03)
4. [PROJECT_SPEC.md — 156 linhas](#anexo-04)
5. [lab/README.md — 1,123 linhas](#anexo-05)
6. [lab/PCSX2_VU_REFERENCE.md — 214 linhas](#anexo-06)
7. [tools/ps2native/README.md — 124 linhas](#anexo-07)
8. [tools/iso_inspect/README.md — 42 linhas](#anexo-08)
9. [docs/FAST_NATIVE_ITERATION.md — 654 linhas](#anexo-09)
10. [docs/NATIVE_HANDOFF.md — 116 linhas](#anexo-10)
11. [docs/ANDROID_PIPELINE.md — 188 linhas](#anexo-11)
12. [docs/RECOMPILER_GAPS.md — 74 linhas](#anexo-12)
13. [android/README.md — 41 linhas](#anexo-13)
14. [ps2xIOP/README.md — 134 linhas](#anexo-14)
15. [ps2xRuntime/Readme.md — 77 linhas](#anexo-15)
16. [ps2xAnalyzer/Readme.md — 85 linhas](#anexo-16)
17. [schemas/nexo-device-state-v1.md — 152 linhas](#anexo-17)
18. [schemas/nexo-ee-aot-v0.md — 119 linhas](#anexo-18)
19. [schemas/nexo-ee-data-family-candidates-v1.md — 120 linhas](#anexo-19)
20. [schemas/nexo-ee-entry-dependencies-v1.md — 115 linhas](#anexo-20)
21. [schemas/nexo-ee-family-catalog-v0.md — 149 linhas](#anexo-21)
22. [schemas/nexo-ee-miss-batch-v1.md — 57 linhas](#anexo-22)
23. [schemas/nexo-ee-miss-v1.md — 153 linhas](#anexo-23)
24. [schemas/nexo-ee-native-data-family-v0.md — 131 linhas](#anexo-24)
25. [schemas/nexo-iop-aot-v0.md — 194 linhas](#anexo-25)
26. [schemas/nexo-observed-vif-case-v1.md — 162 linhas](#anexo-26)
27. [schemas/nexo-vu-aot-v0.md — 40 linhas](#anexo-27)
28. [schemas/nexo-vu-capture-v1.md — 48 linhas](#anexo-28)
29. [schemas/nexo-vu-runtime-binding-v0.md — 60 linhas](#anexo-29)
30. [schemas/nexo-vu-state-v1.md — 88 linhas](#anexo-30)
31. [schemas/nexo-vu-state-v2.md — 45 linhas](#anexo-31)

<!-- PS2NATIVE_DOCUMENTATION_APPENDICES_BEGIN -->

---

<a id="anexo-01"></a>

# ANEXO 01 — README.md

Origem: [README.md](README.md). Linhas originais: **1532**. Bytes: **95589**. SHA-256: `041e38dbe28d29fdf2672f7bd881ae7c49b0c7c8bf1c2978564bf0e5592438fa`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:README.md -->
# PS2Native NEXO — Compilação Nativa por Contratos Causais

**Documento fundacional · Especificação de pesquisa e implementação · Revisão 0.1 · 30 de setembro de 2026**

**Arquivo:** [README.md](sandbox:/workspace/scratch/2c1b44b59f19/PS2Native_NEXO/README.md)

## 1. Missão e tese arquitetural

O PS2Native NEXO propõe transformar uma imagem de disco de PlayStation 2 em um aplicativo autônomo para computadores e Android, com execução nativa do código EE, IOP e VU, descoberta automática dos programas presentes e latentes, preservação do comportamento observável e otimização conjunta dos processadores e dispositivos.

A unidade fundamental de compilação será a **Cápsula Causal Nativa — CCN**: uma região de comportamento que pode atravessar instruções, chamadas, módulos, transferências, microprogramas e operações gráficas, acompanhada de um contrato verificável de memória, tempo, materialização de código e efeitos externos.

A descompilação propõe estruturas de alto nível; a semântica operacional delimita o que elas podem significar; a recompilação produz caminhos nativos conservadores; a verificação autoriza substituições mais eficientes. Todas essas representações permanecem ligadas ao mesmo contrato, em vez de formar ferramentas independentes conectadas por heurísticas irreversíveis.

O salto proposto é **compilar a cadeia causal de uma operação do jogo, incluindo as condições que produzem seu código futuro**, e não apenas traduzir as instruções visíveis no ELF inicial.

O objetivo de produto permanece:

- Uma ISO como entrada do usuário.
- Nenhuma configuração, correção ou seleção manual por título.
- Pacotes independentes para as ABIs anunciadas.
- Nenhum interpretador de EE, IOP ou VU no aplicativo final.
- Nenhuma compilação de código convidado durante a execução final.
- Compatibilidade automática de pelo menos 90% de um catálogo global definido, avançando para 100%.
- Execução em tempo real e apresentação estável; 60+ quadros reais quando o contrato temporal do jogo e o dispositivo permitirem, com a meta adicional de conversão automática de cadência especificada separadamente.

**Estado desta proposta:** arquitetura especificada, ainda não implementada nem validada. O relatório anexo é a evidência disponível sobre o protótipo existente. Os novos mecanismos, teoremas condicionais e metas deste documento não são resultados experimentais.

**Ineditismo:** a contribuição reivindicada é a organização técnica proposta neste documento, especialmente a composição entre fechamento de código latente, cápsulas multicomponente e reconstrução de estado observável. Não se declara prioridade científica ou patenteabilidade: comprovar ausência de antecedentes exige revisão própria. Ferramentas conhecidas são instrumentos de implementação, não evidência de que o sistema inteiro já exista.

## 2. Vocabulário normativo

Os termos **DEVE**, **NÃO DEVE** e **PODE** indicam requisitos, proibições e opções da arquitetura proposta.

| Termo | Definição operacional |
|---|---|
| Código convidado | Instruções EE, IOP, VU e demais programas originalmente executados no console |
| AOT estrito | Código de máquina das CPUs hospedeiras produzido antes da entrega; nenhum novo bloco convidado compilado durante a execução final |
| Nativo | Instruções do jogo executadas por funções da arquitetura hospedeira, com serviços de dispositivos também implementados no host |
| Runtime | Implementação nativa de memória, eventos, dispositivos e integração com o sistema operacional |
| Zero-touch | Conversão e validação sem intervenção humana específica na submissão |
| Zero-shot do pipeline | Submissão inédita para uma versão congelada do sistema, sem perfil, patch, rastro ou resultado prévio específico do título |
| Fechamento de código | Demonstração de que toda transferência de controle possível no domínio declarado encontra tradução nativa previamente produzida |
| Domínio de execução | Conjunto explícito de entradas, periféricos, estados iniciais, versões de hardware e serviços externos considerados |
| Equivalência fiel | Preservação de observações, causalidade, progresso e tempo convidado no domínio declarado |
| Otimização guardada | Substituição válida sob uma condição verificável, com caminho conservador nativo quando a condição falha |
| Materializador | Código que carrega, descomprime, reloca, modifica ou gera bytes que poderão ser executados |

Um runtime de dispositivos conserva comportamento de hardware em software. Portanto, existe modelagem ou emulação semântica de dispositivos mesmo quando toda instrução convidada foi compilada. “Nativo” não significa recuperar o projeto original do estúdio nem eliminar as obrigações de compatibilidade.

A compilação de shaders pelo driver, o carregamento normal de bibliotecas e os componentes internos do sistema operacional não são recompilação de instruções PS2. Essa distinção DEVE aparecer no contrato do produto, sem apresentar SPIR-V como código de máquina universal de GPU.

## 3. Ponto de partida comprovado pelo relatório

O relatório de 30/09/2026 descreve uma implementação local derivada de PS2Recomp, com o commit-base documental `75d729c`. Este documento não realizou nova auditoria do repositório localizado no computador do autor.

| Evidência no relatório | O que será aproveitado | O que ainda precisa mudar |
|---|---|---|
| ISO → análise → C++ → pacote Linux | Ingestão, orquestração e rastreabilidade | Separar descoberta, semântica, prova e geração por ABI |
| 705.383 bindings no caso estudado | Entradas interiores e despacho em lote | Associar endereço a versão de código, contexto de entrada e contrato |
| Overlays EE compilados durante execução Linux | Descoberta e coleta no laboratório | Retirar compilador do produto final; demonstrar fechamento AOT |
| Cache por bytes e ferramentas | Armazenamento endereçado por conteúdo | Incluir pressupostos semânticos, memória, dispositivos e versões do verificador |
| Registro reduzido de 136.045.196 para 2.253.899 bytes | Compactação e geração incremental | Despacho paginado sem expansão obrigatória de todos os aliases |
| IOP e VU interpretados | Referências provisórias e fixtures | Gerar implementações nativas completas e validar estado oculto |
| GS principalmente em software | Caminho comparativo inicial | Construir backend GPU fiel, com sincronização e formatos corretos |
| 484/484 testes C++ e 18/18 testes Python | Regressões existentes | Ampliar semântica, independência de oráculos e testes de integração |
| Ganhos VU de aproximadamente 31–33% em replays | Metodologia de replay alternado | Medir impacto no frame e em entradas distintas |
| Cerca de 1,53 uploads/s em Xvfb/llvmpipe | Linha de base daquele ambiente | Medir GPU física, frame completo e controle causal |
| Preparação Android | Estrutura de empacotamento | Validar ARM64 físico, áudio, vídeo, controles e ciclo de vida |
| Monster House parcialmente observado | Caso de regressão integrado | Aprovar progressão, imagem, som e saves; expandir corpus |

Os 13.706 erros/instruções não tratadas pertencem a um relatório inicial retido. Não são uma contagem auditada de falhas atuais. Da mesma forma, bindings, arquivos gerados e testes aprovados não fornecem uma porcentagem de jogos compatíveis.

O ganho necessário não pode ser calculado dividindo 60 por 1,53: a medição foi de uploads de apresentação em um ambiente específico, e o caminho físico de GPU ainda precisa de benchmark.

## 4. Limites que o contrato precisa resolver

### 4.1 “Qualquer ISO” e “toda a biblioteca” são universos distintos

Uma biblioteca comercial catalogada é um conjunto finito. “Qualquer ISO do mundo” também pode incluir homebrew, corrupção, conteúdo incompleto, revisões desconhecidas, programas deliberadamente adversariais e jogos dependentes de servidores ou periféricos indisponíveis.

A ferramenta DEVE aceitar a submissão de qualquer arquivo, identificar seu formato e produzir um resultado estruturado. Isso não implica conseguir produzir um jogo funcional a partir de informação ausente.

O denominador principal será um catálogo versionado de imagens e revisões. Arquivos inválidos serão identificados separadamente; títulos válidos que excedam os recursos, exijam comportamento ainda desconhecido ou não sejam convertidos continuam sendo falhas no denominador correspondente.

### 4.2 Universalidade não decorre de um algoritmo de descoberta

Para programas gerais com recursos não limitados, prever todo comportamento futuro envolve problemas indecidíveis. Um PS2 fisicamente limitado tem estado finito; nesse modelo fechado, enumerar estados é teoricamente possível. O obstáculo passa a ser a explosão combinatória, acrescida do ambiente externo, e não uma aplicação indiscriminada do problema da parada.

Logo, não se afirma que AOT universal seja logicamente impossível para uma máquina finita. Afirma-se que este documento não fornece um método viável que garanta fechamento, prazo e desempenho para toda entrada concebível.

O contrato prático será um compilador parcialmente decisório: quando consegue justificar seus pressupostos, emite o pacote; quando não consegue, registra a obrigação não resolvida. Um timeout não vira prova, incompatibilidade demonstrada ou sucesso.

### 4.3 A ISO não contém necessariamente todo o ambiente

Firmware, comportamento de periféricos, relógio, rede e respostas de dispositivos não estão integralmente descritos no disco. Para manter uma única entrada, o sistema deve oferecer implementações comportamentais próprias e verificadas dos serviços necessários.

Se uma execução depende de bytes específicos de firmware ausentes, de segredo externo ou de um servidor não reproduzido, a dependência permanece explícita. Não será resolvida inventando dados ou incorporando silenciosamente material externo.

### 4.4 60 FPS não é consequência automática de código nativo

Um jogo originalmente limitado a 30 ou 50 atualizações por segundo pode executar perfeitamente em tempo real sem gerar 60 estados visuais distintos. Dobrar sua frequência pode alterar física, scripts, áudio e controle.

O sistema manterá métricas distintas de simulação, renderização, apresentação e repetição/interpolação. A meta de 60+ quadros reais exige capacidade do dispositivo e, quando necessário, uma transformação temporal adicional demonstrada. Essa transformação é uma frente de pesquisa, não uma consequência da tradução da ISA.

## 5. O paradigma NEXO

O NEXO combina cinco mecanismos:

1. **Representações ligadas por refinamento:** instruções exatas, estruturas descompiladas e código otimizado permanecem relacionados por obrigações verificáveis.
2. **Fechamento de código por materializadores:** analisar os produtores de código e suas famílias de resultados, além dos bytes encontrados em execuções.
3. **Cápsulas causais multicomponente:** otimizar regiões que atravessam EE, IOP, DMA, VIF, VU, GIF e GS quando seus efeitos intermediários podem ser reconstruídos.
4. **Tempo observável explícito:** conservar eventos, leituras de relógio e arbitragem sem obrigar toda operação a percorrer um dispatcher por ciclo.
5. **Autorreparo com autoridade limitada:** modelos e sintetizadores propõem; verificadores e evidências independentes decidem.

| Estratégia | Unidade predominante | Restrição que o NEXO procura superar |
|---|---|---|
| Tradução literal | Instrução ou bloco | Custo de estado, despacho e fronteiras artificiais |
| Descompilação isolada | Função ou estrutura de programa | Hipóteses sem garantia suficiente sobre dispositivos e código futuro |
| Descoberta por execução | Caminho observado | Ausência de cobertura dos caminhos não visitados |
| Substituição de SDK por nomes | Rotina conhecida | Ambiguidade de ABI, versão, efeitos e memória observável |
| NEXO | Cápsula causal com família de código e contrato temporal | Composição das quatro dimensões no mesmo artefato verificável |

A hipótese de pesquisa é que uma parcela relevante do custo e da dificuldade de fechamento está nas fronteiras artificiais entre esses componentes. A arquitetura será julgada por experimentos que removem cada mecanismo, mantendo os demais constantes.

## 6. Arquitetura global

```mermaid
flowchart TD
    ISO["Imagem de disco"] --> ING["Ingestão e identidade"]
    ING --> DISC["Descoberta e materializadores"]
    SPEC["Semântica de máquina"] --> IR["IR de efeitos temporais"]
    DISC --> IR
    IR --> BASE["Tradução nativa conservadora"]
    IR --> HYP["Abstrações e cápsulas candidatas"]
    HYP --> CHECK["Verificação de contratos"]
    BASE --> CHECK
    CHECK --> AOT["Build AOT por plataforma"]
    AOT --> TEST["Validação e exploração"]
    TEST --> DIAG["Contraprovas e diagnóstico"]
    DIAG --> DISC
    DIAG --> HYP
    TEST --> GATE["Critérios de aceitação"]
    GATE --> OUT["Aplicativo e certificado de escopo"]
```

O laboratório de conversão PODE executar interpretadores, compiladores dinâmicos, solvers e exploradores. O aplicativo entregue contém apenas o runtime necessário, funções AOT, recursos e metadados de validação.

O laboratório e o produto serão builds distintos. Excluir o laboratório por flags sem verificar símbolos, dependências e caminhos alcançáveis não basta.

## 7. Modelo formal de comportamento

### 7.1 Estado global

Define-se o estado convidado:

\[
S=(E,I,V_0,V_1,M,C,D,G,A,P,K,Q,T,X).
\]

Os componentes representam EE, IOP, VUs, memória, caches e visibilidade, DMA e barramentos, GS, áudio, periféricos, serviços de sistema, eventos pendentes, relógios e ambiente externo.

Cada processador inclui registradores, flags, exceções, estado de pipeline relevante e contexto de retomada. O estado de VU inclui operações em voo. A memória inclui aliases e regiões de dispositivos; o GS inclui memória local e efeitos pendentes.

A semântica é um sistema de transição rotulado:

\[
S \xrightarrow{e,\Delta t} S'.
\]

O rótulo identifica um efeito ou uma transição interna. O tempo é convidado: não corresponde à duração de execução da função nativa.

### 7.2 Observações

O contrato define a projeção \(\pi_O\) dos rastros sobre:

- Valores que o programa pode ler, inclusive memória de código lida como dados.
- Exceções, interrupções, transferências e sua ordem observável.
- Respostas de serviços, arquivos, controles e periféricos.
- Alterações persistentes em saves e cartões.
- Saída audiovisual nos pontos definidos pelo contrato.
- Leituras de contadores, sincronismo e progresso.

Uma operação interna só pode desaparecer se nenhum observador permitido distinguir sua remoção, inclusive observadores futuros.

### 7.3 Relação entre execução original e nativa

Sejam \(R\) o modelo de referência, \(N\) o programa nativo, \(D\) o domínio de entradas e \(\rho\) a relação entre seus estados. A obrigação fiel é uma equivalência observacional por simulação nos dois sentidos, permitindo passos internos adicionais:

\[
\forall u\in D:\quad
\pi_O(\operatorname{Traces}(R,u))
=
\pi_O(\operatorname{Traces}(N,u)).
\]

Para uma referência determinística e uma entrada fixada, compara-se o rastro correspondente. Se a referência contém escolhas reais de ambiente, elas precisam ser acopladas; não é válido escolher retrospectivamente uma execução de referência conveniente.

A prova deve preservar terminação, divergência observável e progresso. Um programa que deixa de emitir efeitos porque entrou em loop não satisfaz a equivalência por ter reproduzido um prefixo correto.

Perfis que permitem aproximação audiovisual ou alteração temporal usam outra relação, explicitamente nomeada. Eles não recebem o mesmo certificado de equivalência fiel.

## 8. Cápsula Causal Nativa

Uma cápsula é definida por:

\[
C=(\mathcal E,P,\mathcal V,F_r,F_w,\mathcal O,\Theta,
B,N_o,\rho,\kappa,\mathcal M,\Pi).
\]

| Campo | Significado |
|---|---|
| \(\mathcal E\) | Entradas válidas, com contexto de branch, exceção e pipeline |
| \(P\) | Pré-condição sobre estado e ambiente |
| \(\mathcal V\) | Versões e famílias de código cobertas |
| \(F_r,F_w\) | Efeitos de leitura e escrita, incluindo aliases e dispositivos |
| \(\mathcal O\) | Portas de observação e efeitos externos |
| \(\Theta\) | Transformação temporal e fronteiras de eventos |
| \(B\) | Implementação nativa conservadora |
| \(N_o\) | Implementação nativa otimizada opcional |
| \(\rho\) | Relação entre estados representados |
| \(\kappa\) | Guarda de validade da implementação otimizada |
| \(\mathcal M\) | Mapas de reconstrução nos pontos de saída |
| \(\Pi\) | Certificados e evidências associados |

A cápsula pode conter uma função, parte de uma função ou uma cadeia de trabalho distribuída. Seu tamanho é decidido pela observabilidade, pelos limites de prova e pelo custo de execução.

### 8.1 Composição condicional

Se duas cápsulas satisfazem seus contratos e suas condições de interface são compatíveis, sua composição preserva o contrato composto, desde que:

1. A pós-condição da primeira estabeleça a pré-condição da segunda.
2. Os efeitos compartilhados sejam ordenados ou comutativos sob prova.
3. As hipóteses sobre código, memória e ambiente permaneçam válidas.
4. As transições internas não eliminem progresso ou observações temporais.

Essa é uma obrigação a mecanizar por indução e composição de simulações. A formulação não constitui uma prova já executada do PS2Native.

### 8.2 Retomada sem interpretação

Se uma guarda falha antes da cápsula, executa-se \(B\). Se um evento exige sair durante sua execução, a saída ocorre em uma porta previamente compilada, com reconstrução de estado.

Uma cápsula só pode atravessar um intervalo sem portas quando houver prova de que nenhum evento observável pode interrompê-lo. Caso contrário, o compilador deve inserir portas ou manter uma região menor.

Eventos externos já publicados não podem ser desfeitos por rollback. Recuperação exige fronteiras de commit ou computação especulativa em estado privado, antes de qualquer efeito irreversível.

## 9. IR de Efeitos Temporais — IET

A IET manterá três vistas relacionadas:

| Vista | Conteúdo | Uso |
|---|---|---|
| Exata | Bits, registradores, memória, exceções e transições de dispositivos | Referência de tradução e caminho conservador |
| Estrutural | SSA, loops, funções, chamadas, regiões e relações de alias | Descompilação e análise |
| Causal | Contratos compostos, trabalho vetorial, transferências e operações gráficas | Fusão e planejamento de execução |

Uma abstração não substitui sua origem: registra uma relação verificável com ela. Nomes de variáveis, tipos de estruturas e reconhecimento de funções podem permanecer hipóteses sem afetar a correção do executável.

### 9.1 Tipos e efeitos

Tipos fundamentais:

```text
bits<N>               inteiro modular ou representação crua
guest_addr<space>      endereço convidado sem identidade de ponteiro host
guest_fp<profile>      operação de ponto flutuante do perfil especificado
mem_token<region>      dependência de acesso e visibilidade
event_token<domain>    dependência causal entre componentes
clock<domain>          relógio convidado
code_family<id>        família de código fechada
resume_state<id>       estado reconstruível em uma porta
```

Efeitos relevantes:

```text
read, write, fetch, cache_update, dma_publish, interrupt,
exception, fifo_push, fifo_pop, clock_read, code_publish,
vram_observe, audio_commit, save_commit, external_input
```

Operações com efeitos usam tokens e não podem ser reorganizadas apenas por parecerem independentes na SSA de valores.

### 9.2 Exemplo ilustrativo

```text
capsule skin_batch(ctx, vertices, matrices, count):
  requires code_family == vu_skin_family
  requires no_unmodeled_alias(vertices, matrices)
  requires next_observable_event >= certified_exit_time

  (packet, t1) = vif_unpack_exact(ctx.vif, vertices, count, ctx.time)
  (gif, vu_state, t2) = vu_skin_exact(packet, matrices, t1)
  (gs_state, t3) = gs_submit_ordered(ctx.gs, gif, t2)

  ensures reconstructible_at(exit_port)
  returns (vu_state, gs_state, t3)
```

Os nomes `skin` e `vertices` são hipóteses legíveis. A validade depende das operações exatas e das provas de substituição, não do acerto desses nomes.

## 10. Descompilação bidirecional e geração conservadora

A frente de descompilação recupera fluxo, expressões, tipos candidatos, estruturas repetidas e bibliotecas. Ela pode usar análise abstrata, execução simbólica, síntese e modelos de linguagem.

A frente conservadora traduz cada instrução descoberta para operações exatas, mantendo pontos de entrada e efeitos. Esse caminho existe mesmo quando a estrutura de alto nível não foi recuperada.

O fluxo é bidirecional porque uma contraprova de uma abstração retorna à representação exata e invalida as hipóteses dependentes, preservando os fragmentos já demonstrados.

### 10.1 Otimização por obrigações

Cada transformação deve registrar:

```text
origem + pré-condição + candidato + efeitos + obrigação + resultado
```

Resultados possíveis: `proved`, `refuted`, `bounded_only`, `tested_only`, `unknown`.

Somente `proved`, dentro do escopo correspondente, autoriza eliminar o caminho conservador. Testes podem apoiar uma variante experimental, mas não promovê-la automaticamente a equivalência demonstrada.

### 10.2 Saturação de equivalências

E-graphs podem representar alternativas para expressões e regiões puras. Tokens de efeitos e condições temporais delimitam as reescritas. Igualdades matemáticas reais não autorizam transformações de ponto flutuante convidado.

A biblioteca `egg` documenta e-graphs e equality saturation como técnicas anteriores ao próprio projeto; sua utilização não será apresentada como invenção do NEXO. [R4] genui{"citation":{"ref":"turn3view2"}}

O custo de extração será multicritério: latência, código, memória, tráfego, prova e recompilação. Regiões terão limites de nós, tempo e alternativas para evitar explosão combinatória.

### 10.3 Backend e confiança

LLVM é uma opção de backend, não um certificado automático. A IET deve impedir que overflow modular, aliases, shifts, divisão excepcional ou ponto flutuante convidado sejam convertidos em comportamento indefinido do host. A semântica de `poison`, atributos e flags de otimização precisa ser respeitada. [R6] genui{"citation":{"ref":"turn3view4"}}

Alive2 pode validar transformações LLVM dentro de seu escopo; sua validação limitada não demonstra, por si, loops arbitrários, drivers, dispositivos ou o executável completo. [R3] genui{"citation":{"ref":"turn4search0"}}

## 11. Descoberta do disco e dos programas

### 11.1 Preservação da imagem

A ingestão cria duas vistas ligadas:

- Vista de setores e intervalos, preservando a informação efetivamente contida na imagem.
- Vista de arquivos, caminhos, extensões e metadados conhecidos.

A extração por arquivos não pode substituir a imagem quando o programa usa leituras por LBA, layouts especiais, alinhamento, setores sobrepostos ou dados não representados por arquivos comuns.

O detector deve distinguir ISO de dados, imagens com outras organizações e arquivos apenas renomeados para `.iso`. Setores ou metadados inexistentes na entrada não podem ser reconstruídos por suposição.

### 11.2 Inventário multimodal

Fontes de candidatos:

1. `SYSTEM.CNF`, boot ELF e cabeçalhos de módulos.
2. Segmentos, relocations, símbolos e tabelas presentes.
3. Rotinas de leitura, descompressão, carga, relocação e publicação.
4. Escritas em memória posteriormente utilizada para fetch.
5. Uploads de microprogramas VU e encadeamentos DMA.
6. Entradas indiretas descobertas por análise de valores e exploração.

Flags ELF isoladas não classificam definitivamente EE versus IOP. A identificação combina carregador, espaço de endereços, ISA, importações e comportamento de execução.

### 11.3 Código e dados

O sistema mantém hipóteses sobre bytes, não uma classificação irreversível por região. O mesmo conteúdo pode ser dados em uma fase e código em outra.

Desassemblar toda palavra alinhada gera candidatos, não prova alcançabilidade. Por outro lado, um endereço executável não precisa coincidir com o início de uma função inferida.

O fechamento exige que todo alvo indireto admissível tenha tradução ou pertença a uma família nativa coberta. Alvos inalcançáveis só podem ser excluídos mediante análise conservadora suficiente para o domínio.

## 12. Código latente e fechamento por materializadores

### 12.1 O problema

Um inventário de executáveis não encontra necessariamente código comprimido, relocável, gerado, modificado ou construído a partir de dados. Executar algumas rotas e congelar os blocos visitados também não demonstra que todas as versões futuras foram cobertas.

O NEXO analisa a função que produz o código.

Se \(m(x)\) produz bytes executáveis, o problema passa a ser encontrar uma implementação nativa parametrizada \(n_m(x,s)\) tal que:

\[
\forall x\in D_m,\forall s\in P_m(x):\quad
\operatorname{Exec}_{PS2}(m(x),s)
\simeq_O n_m(x,s).
\]

Também é necessário provar que as invocações alcançáveis de \(m\) usam entradas de \(D_m\). Provar apenas a equivalência da família, sem provar sua abrangência, deixa o fechamento incompleto.

### 12.2 Quatro classes de resolução

| Classe | Tratamento AOT |
|---|---|
| Conteúdo fixo carregado do disco | Extrair ou materializar durante a conversão e compilar |
| Família finita de overlays | Enumerar variantes alcançáveis ou superconjunto conservador |
| Template com parâmetros de dados | Gerar função nativa parametrizada e provar a família |
| Gerador de programa arbitrário sem família fechada demonstrada | Registrar obrigação aberta e não aprovar AOT estrito |

Não se pressupõe que a quarta classe domine ou seja rara na biblioteca; sua prevalência será medida.

### 12.3 Exemplo de especialização sem JIT

Um materializador produz rotinas de cópia com endereços e tamanho variáveis. Se a estrutura semântica é sempre a mesma, pode-se emitir antecipadamente:

```text
native_copy_family(dst, src, length, guest_context)
```

A função deve preservar sobreposição, acessos desalinhados, exceções, ordem e tempo observável. Não se pode substituí-la cegamente por `memcpy`.

Os bytes convidados produzidos continuam disponíveis para leitura, checksum e outros observadores. A otimização substitui sua execução, não necessariamente suas escritas.

Parâmetros de dados podem variar sem produzir código de máquina novo. Se os parâmetros passam a codificar uma sequência arbitrária de opcodes consumida por um executor genérico, o mecanismo voltou a interpretar uma ISA e viola o contrato.

### 12.4 Linguagem restrita de famílias

O descritor de família pode escolher entre kernels e formas de controle previamente compilados e carregar constantes verificadas. Não pode conter um programa convidado arbitrário despachado instrução por instrução.

Loops sobre arrays de dados são permitidos. Loops que leem e executam um fluxo genérico de instruções EE/IOP/VU são proibidos no runtime final.

Interpretadores de scripts que já pertenciam ao jogo podem ser recompilados como parte dele. Isso não autoriza introduzir um interpretador novo do processador PS2 para cobrir falhas de tradução.

### 12.5 Algoritmo de fechamento

```text
worklist := entradas iniciais + materializadores conhecidos
closed := vazio
obligations := vazio

while worklist não vazia e orçamento disponível:
    item := selecionar_por_risco_e_custo(worklist)
    resultado := analisar_conservadoramente(item)

    se código fixo:
        traduzir entradas e registrar sucessores
    se família parametrizada:
        sintetizar candidato e verificar contrato universal
    se alvo indireto:
        calcular conjunto conservador de destinos
    se pressuposto não demonstrado:
        registrar obrigação; solicitar contraprova ou refinamento

    acrescentar dependências novas à worklist
    atualizar certificados e invalidações

aceitar fechamento somente se:
    entradas iniciais cobertas
    sucessores e famílias fechados no domínio
    todas as obrigações obrigatórias resolvidas
```

A worklist vazia por falta de novos rastros não é suficiente. O certificado deve explicar por que não restam sucessores possíveis fora do conjunto coberto.

### 12.6 Síntese concreta de uma família

O sintetizador trabalha sobre os produtores de bytes, com os seguintes passos:

1. Construir um slice retroativo das escritas que alimentam fetch, incluindo descompressão, relocação, endianness e aliases.
2. Separar constantes, parâmetros de dados, escolhas de estrutura e dependências externas.
3. Decodificar simbolicamente as expressões de bytes durante a conversão, produzindo restrições sobre instruções possíveis.
4. Particionar escolhas que mudam o fluxo ou a seleção de registradores; manter imediatos e endereços como parâmetros quando for correto.
5. Traduzir cada estrutura finita para uma função nativa, com primitivas exatas e parâmetros tipados.
6. Provar um invariante do produtor que relacione seus bytes finais ao descritor da família.
7. Provar a execução da família para todos os parâmetros admissíveis e verificar que as chamadas alcançáveis satisfazem esse domínio.

O aprendizado por contraprovas pode refinar as partições. Um exemplo observado sugere um template; a obrigação universal determina se ele cobre mais do que o exemplo.

Se a análise abstrata retorna `top` para a estrutura de instruções, o sistema não converte esse resultado em uma família universal executável. Deve refinar, enumerar um conjunto finito justificável ou registrar falha de fechamento.

Por exemplo, variar somente o endereço de destino pode produzir uma única função parametrizada. Variar campos que escolhem instruções e registradores exige demonstrar uma estrutura limitada. Se a única generalização restante for executar uma sequência arbitrária de operações codificadas, a família é rejeitada pelo contrato nativo.

### 12.7 Certificado de fechamento como ponto fixo

Seja \(A\) uma sobreaproximação dos estados alcançáveis nas entradas de código e \(F\) o conjunto de traduções e famílias aceitas. O fechamento requer:

\[
Init\subseteq A,\qquad Post(A)\subseteq A,\qquad
\forall s\in A:\operatorname{NextCode}(s)\subseteq\operatorname{Covered}(F).
\]

Essas relações incluem transições de dispositivos e escritores externos permitidos. `Post` não é apenas o CFG do ELF.

`NextCode(s)` é o conjunto de identidades executáveis que podem ser selecionadas a partir desse estado; em um estado terminal sem continuação, ele é vazio. Cada identidade inclui processador, versão, endereço e contexto de entrada.

O sistema pode provar essas condições por módulos e contratos de interface, sem enumerar todos os estados concretos. Entretanto, uma aproximação tão ampla que inclua destinos desconhecidos impede o certificado até ser refinada. Essa é a fronteira explícita entre análise conservadora e esperança baseada em rastros.

## 13. Memória, identidade e invalidação

### 13.1 Identidade de execução

Um PC isolado não identifica código. A chave de execução inclui:

```text
processador + endereço virtual + tradução física + modo arquitetural
+ contexto de entrada + visão de fetch + identidade da versão de código
+ perfil semântico + contrato de dependências
```

A visão de fetch pode diferir da RAM mais recente por regras de cache e visibilidade. A implementação deve modelar quando uma escrita passa a afetar instruções executadas.

### 13.2 Escritores cobertos

Stores EE/IOP, DMA, loaders, HLE, uploads VU e qualquer outro escritor reconhecido devem passar pelo mesmo protocolo de publicação. Páginas protegidas pelo sistema operacional podem ajudar no diagnóstico, mas não substituem esse protocolo.

Aliases virtuais convergem para uma identidade física e suas visões arquiteturais. O epoch pode ser por página, linha ou intervalo, conforme o custo, mas nunca omitir um alias observável.

### 13.3 Publicação e execução concorrente

Uma atualização segue:

1. Registrar as escritas no modelo de memória.
2. Atualizar visibilidade no momento arquitetural correto.
3. Invalidar entradas e guardas dependentes.
4. Publicar a nova identidade de código.
5. Selecionar implementação AOT já coberta.

Uma escrita que muda instruções ainda por executar na cápsula atual exige saída na fronteira correta. Invalidar apenas futuras chamadas não resolve automodificação dentro da região ativa.

### 13.4 Cache de artefatos

O cache persistente inclui bytes, relocations, pressupostos, semântica, pipeline de otimização, alvo, ABI, dependências de runtime e versão do verificador. Um hash forte identifica conteúdo; não demonstra equivalência.

A consulta pode usar hashes para localizar candidatos e validação exata de descritores antes de ligá-los. Perfis de desempenho por driver ou dispositivo não alteram o certificado semântico.

## 14. Semântica EE e entradas interiores

O novo frontend deve cobrir explicitamente:

- Operações escalares, extensões e resultados parciais de registradores.
- MMI, lanes, saturação e acumuladores HI/LO.
- Loads/stores, alinhamento, merges e exceções.
- Branches, branch-likely, delay slots e efeitos anulados.
- Saltos indiretos, link registers e entradas em meio de regiões.
- COP0, modos de execução, TLB, contadores e interrupções.
- COP1, representação, flags e comportamento numérico particular.
- Interface macro VU0 e sincronização associada.
- Instruções de cache, sincronismo e efeitos sobre fetch.

Uma entrada direta no endereço de um delay slot não é equivalente à execução desse endereço como delay slot. O identificador de entrada deve distinguir esses contextos e preservar informações de exceção, como o vínculo ao branch anterior.

Comportamentos indefinidos ou dependentes de revisão do hardware, incluindo combinações problemáticas de controle, requerem caracterização do modelo selecionado. A arquitetura não escolhe arbitrariamente uma interpretação conveniente.

### 14.1 Aritmética e ponto flutuante

A semântica numérica terá primitivas explícitas. Operações host rápidas só substituem primitivas convidadas sob equivalência demonstrada para o domínio utilizado.

Testes incluirão padrões extremos, zeros com sinal, representações especiais, arredondamento, normalização, flags, aliases de operandos e uso dos bits como inteiros. `fast-math`, contração FMA e conversões implícitas não serão habilitados globalmente.

O caso relatado de `SQRT.S`/`RSQRT.S` torna obrigatório testar seleção de operandos separadamente da aproximação aritmética. A preservação de bits observada em VU também impede tratar todo valor carregado como número abstrato.

### 14.2 Despacho

Entradas interiores serão representadas por diretórios compactos de páginas e offsets, com mapas para pontos de retomada compilados. Caminhos diretos válidos poderão ser ligados diretamente; indiretos consultam a identidade completa.

Quando nenhum destino AOT válido existir, o runtime emite `UNSEEN_CODE` e um registro reproduzível. Não chama um compilador, não interpreta a instrução e não retorna sucesso fictício.

## 15. IOP, módulos e contratos de serviços

O IOP receberá frontend, IR, análise de código latente e backend próprios. Tratar seus módulos apenas como serviços nomeados não cobre código IRX desconhecido ou drivers particulares.

O carregador deve representar relocations, importações, exportações, substituição de módulos, callbacks, GP, pilha, reinicializações e endereços observáveis. Nomes de funções e assinaturas conhecidas ajudam a descobrir contratos; não autorizam substituir uma rotina sem verificar a versão e seus efeitos.

### 15.1 Dois caminhos nativos

- **Tradução AOT do módulo:** preserva a implementação presente na imagem.
- **Serviço nativo por contrato:** substitui uma implementação reconhecida quando retorno, memória, callbacks, estados intermediários observáveis e comportamento temporal estão cobertos.

A alternativa conservadora continua sendo código compilado do módulo. Um serviço não reconhecido não cai num interpretador IOP.

Funções de sistema que não estão disponíveis na ISO exigem o modelo comportamental do ambiente. Elas não podem ter uma alternativa compilada de bytes que o sistema não possui.

### 15.2 ABI observável

O contrato de chamada descreve registradores de argumentos, largura, extensão, layout de estruturas, retorno, efeitos sobre registradores preservados, stack, GP e memória.

A falha double/float relatada em `libm` será transformada em uma classe de regressão: reconhecer o nome `sqrt` não basta para escolher uma assinatura, uma aproximação numérica ou a ABI do host.

HLE de uma função longa também deve preservar interrupções e alterações intermediárias acessíveis a outros componentes. Igualdade da memória apenas na entrada e na saída é insuficiente quando há observadores concorrentes.

### 15.3 Kernel e serviços

O modelo deverá cobrir threads, prioridades, semáforos, event flags, alarmes, handlers, filas, timers, resets e cancelamentos. As regras precisam ser explícitas em relação a reentrância e chamadas a partir de interrupções.

IOMAN, LOADFILE, SIF/RPC e bibliotecas de suporte devem preservar limites, erros, assinaturas, buffers e estados de sessão. O PS2SDK é uma referência primária útil para interfaces e testes homebrew, mas não constitui descrição completa de toda implementação comercial. [R8] genui{"citation":{"ref":"turn3view1"}}

## 16. VU como programa nativo com estado de pipeline

VU0 e VU1 serão compilados a partir de uma semântica que inclua execução de pares de instruções, conflitos, latências, flags, registradores especiais, memória e interação com VIF/GIF.

### 16.1 Estado completo

O snapshot canônico deve incluir:

```text
VF, VI, ACC, Q, P, I, R
flags e resultados ainda não publicados
PC, branch pendente e contexto de delay
operações das pipelines com tempos de conclusão
memória de dados e visão de microcódigo
estado de início, continuação, pausa e término
estado observável de transferências e XGKICK
```

Campos concretos serão derivados do modelo adotado. O formato não pode ser um dump de `struct` dependente de padding ou da ABI x86.

### 16.2 Compilação em três níveis

| Nível | Transformação | Condição |
|---|---|---|
| V0 | Código nativo por microbloco com pipeline explícita | Semântica implementada e entradas cobertas |
| V1 | Agenda estática com timestamps e dependências resolvidas | Latências e acessos suficientemente conhecidos |
| V2 | Kernel vetorial ou computacional de alto nível | Equivalência dos dados, flags, efeitos e estado de saída |

O nível V0 elimina a interpretação, mas pode continuar caro. V1 evita recalcular a mesma lógica de pipeline em cada instrução. V2 permite SIMD de CPU ou processamento em GPU quando o agrupamento compensa e o contrato permite.

Uma otimização que calcula a geometria correta, mas altera um GIF tag, um flag consultado depois ou o estado de `MSCNT`, é incorreta.

### 16.3 Critério de offload

Enviar um lote à GPU exige tamanho suficiente, ausência de observações intermediárias incompatíveis e uma estratégia comprovada de reconstrução. Microprogramas pequenos, readbacks frequentes ou comunicação intensa podem permanecer na CPU.

Os 54.000 replays do relatório ajudam a preservar regressões. A igualdade entre duas versões nesses inputs não substitui a caracterização independente do hardware, especialmente para pipelines não serializadas nas capturas antigas.

## 17. Tempo, eventos e execução paralela

### 17.1 Relógios separados do host

O runtime usa um eixo temporal inteiro amplo ou relações racionais exatas entre domínios. Frequências, divisores, wraparound e instantes de atualização pertencem ao perfil semântico.

O relógio convidado não é avançado pelo tempo gasto pelo host. Uma CPU mais rápida deve reduzir a latência física da execução, não acelerar involuntariamente o jogo.

Leituras de `Count`, esperas, timers, eventos de áudio, vídeo e disco devem continuar coerentes com o mesmo histórico temporal.

### 17.2 Horizonte seguro

Para uma cápsula, calcula-se um horizonte até o próximo evento que possa alterar sua execução ou tornar visível um efeito. Ela pode avançar até esse horizonte; além dele, precisa sincronizar ou refinar a análise.

Intervalos de tempo abstratos podem orientar a análise. Se um intervalo atravessa uma fronteira observável, deve ser refinado ou tratado pelo caminho conservador. Escolher arbitrariamente um ponto dentro dele pode mudar o jogo.

### 17.3 Comutatividade condicionada

Duas operações \(a\) e \(b\) só podem trocar de ordem quando:

\[
\{P\}\;a;b\;\simeq_O\;b;a
\]

e a troca preserva o calendário de observações relevante.

Ausência de conflito em RAM não basta: ambos podem compartilhar barramento, FIFO, prioridade DMA, clock, recurso de dispositivo ou instante de interrupção.

O primeiro runtime executará uma agenda determinística conservadora. Paralelismo entre threads host será introduzido apenas para regiões com independência demonstrada ou protocolo explícito de sincronização.

### 17.4 Arbitragem e empates

Eventos simultâneos não serão ordenados por um contador arbitrário apenas para tornar a execução repetível. A ordem deve corresponder ao modelo de arbitragem ou representar suas alternativas admissíveis.

Determinismo de implementação e fidelidade ao hardware são propriedades diferentes.

## 18. GS: memória local exata e execução acelerada

O backend gráfico terá como referência o estado observável do GS, não apenas uma imagem visualmente semelhante.

### 18.1 Memória e aliasing

Texturas, render targets, buffers de profundidade, paletas e transferências podem compartilhar memória local. O sistema manterá uma representação canônica dos bytes e vistas derivadas com validade controlada.

Swizzles, formatos, máscaras, packing e reinterpretações devem ser descritos por operações exatas. Uma textura host não pode ser tratada como proprietária exclusiva de bytes que também são observados por outra vista.

Cada acesso produz dependências sobre intervalos e máscaras de bytes ou palavras. Pixels diferentes ainda podem compartilhar uma unidade de armazenamento; a independência deve ser provada na representação física apropriada.

### 18.2 Caminhos de renderização

| Caminho | Papel |
|---|---|
| Rasterização host direta | Operações cujo comportamento coincide com o contrato GS |
| Compute ordenado por regiões | Blending, testes, formatos e feedback que exigem controle mais preciso |
| Kernel nativo conservador | Casos sem mapeamento acelerado aprovado ou hardware insuficiente |

O caminho conservador não interpreta EE/IOP/VU. Ele executa uma implementação nativa do dispositivo, mas pode reprovar a meta de desempenho.

### 18.3 Ordenação e feedback

O planejamento gráfico cria um grafo de dependências de leitura/escrita, efeitos e observações. Primitivas podem ser processadas em paralelo somente quando suas dependências permitem.

Feedback de framebuffer, leitura de destino, testes de alpha, mistura de formatos e transferências parciais exigem preservar a ordem efetiva. Dividir uma imagem em tiles não elimina dependências entre tiles nem aliases físicos.

Uma otimização pode manter resultados intermediários na GPU, mas deve materializar a memória correta antes de readbacks, leituras de CPU, transferências dependentes ou outra porta de observação.

### 18.4 Semântica numérica gráfica

Interpolação, precisão, clipping, testes, blending, saturação, truncamento, dither e máscaras serão especificados independentemente das conveniências da API host. Estados não expressáveis exatamente pela rasterização fixa devem usar shaders ou kernels adequados.

Reescritas que omitem primitivas, corrigem a câmera à força ou substituem resultados inválidos por valores visualmente plausíveis são diagnósticos, não correções fiéis.

### 18.5 Sincronização host

Barreiras, visibilidade e ownership de recursos devem seguir o modelo da API. Vulkan atribui à aplicação responsabilidades explícitas de sincronização; ordem de submissão não substitui todas as dependências de memória necessárias. [R9] genui{"citation":{"ref":"turn2search0"}}

A abstração do backend deve permitir Vulkan e uma implementação apropriada para plataformas que usem outra API. Igualdade do algoritmo em duas APIs não dispensa validação de precisão e sincronização em cada backend.

## 19. Dispositivos restantes e ambiente de execução

| Subsistema | Obrigações principais |
|---|---|
| DMA | Tags, cadeia, fragmentação, prioridades, stalls, interrupções e visibilidade |
| VIF | Parsing incremental, resíduos de pacotes, máscaras, ciclos, modos e uploads |
| GIF | Arbitragem de caminhos, registros, formatos e continuidade dos fluxos |
| SIF | Memória compartilhada, comandos, RPC, callbacks e resets |
| SPU2 | Decodificação, vozes, envelopes, mistura, efeitos, DMA, interrupções e saída temporal |
| IPU | Bitstream, estados parciais, comandos e resultados observáveis |
| CDVD | Leituras por setor, status, erros, seeks, streaming e sincronismo |
| Controles | Pacotes, identificação, modos, pressão, analógicos e amostragem temporal |
| Memory card | Diretórios, erros, formato, persistência e atomicidade apropriada |
| Rede e periféricos especiais | Contratos de interação e dependências externas explícitas |

### 19.1 Áudio

A produção de amostras usa o relógio convidado. O resampler e o dispositivo de saída host ficam numa fronteira identificada. Correção interna, sincronismo audiovisual e qualidade final são medidos separadamente.

O oráculo compara amostras internas quando determinísticas, sequência de eventos e alinhamento temporal. Um único hash do áudio capturado pelo sistema operacional não é adequado se diferentes resamplers ou dispositivos introduzem variação esperada.

Underruns, repetições, cortes e deriva audiovisual devem aparecer na telemetria. Acelerar o jogo para manter o buffer cheio não é reparo válido.

### 19.2 Vídeo e IPU

Decodificação multimídia nativa pode substituir um caminho convidado quando os resultados observáveis e o consumo temporal estejam cobertos. Caso contrário, preserva-se a operação do dispositivo. Desabilitar uma dependência de vídeo no Android não elimina a necessidade funcional.

### 19.3 Saves e estado inicial

Cartões e saves usam serialização canônica e transações de escrita. Testes incluem criar, listar, carregar, sobrescrever, apagar, esgotar espaço e retomar após interrupção do aplicativo.

Memória não inicializada, relógio e outros elementos variáveis precisam de política explícita. Escolher sempre zero só é correto quando autorizado pelo modelo ou pelo domínio certificado.

### 19.4 Serviços externos

Um jogo que exige servidor, câmera, microfone, instrumento ou outro periférico deve ser avaliado nesse contrato. A compatibilidade do modo offline não demonstra compatibilidade de todas as funções.

## 20. Fusão causal: exemplo completo

Considere uma sequência que lê dados, descomprime geometria, prepara DMA, executa VIF/VU, produz GIF e rasteriza.

O caminho conservador implementa todas as etapas e seus estados. O candidato otimizado pode:

1. Reconhecer o contrato do descompressor e produzir uma representação temporária adequada.
2. Eliminar cópias intermediárias sem observadores, mantendo o efeito lógico das escritas.
3. Resolver parâmetros constantes de VIF.
4. Substituir a família VU por um kernel vetorial verificado.
5. Emitir operações gráficas diretamente para o backend.
6. Reconstruir os estados convidados nos pontos onde voltam a ser observáveis.

Para autorizar essa transformação, deve provar:

- Que nenhum componente lê os buffers eliminados antes da reconstrução.
- Que interrupções e acessos concorrentes não revelam estados omitidos.
- Que bytes, flags e tags permanecem equivalentes.
- Que o tempo convidado e os sinais de conclusão são preservados.
- Que readbacks e observações futuras recebem a representação correta.
- Que as famílias de código e os parâmetros usados permanecem válidos.

Se um callback pode observar o buffer no meio da operação, a região deve ser dividida ou materializar o estado nesse ponto. Se essa exigência destrói o ganho, mantém-se o caminho conservador.

O benefício esperado vem da eliminação de representações e comunicação redundantes. A magnitude é uma hipótese mensurável; não se presume aceleração universal.

### 20.1 Compilar também a reconstrução

A implementação otimizada pode armazenar um estado mais compacto que o conjunto de registradores e buffers convidados. Para cada porta, o compilador gera antecipadamente uma função de reconstrução que transforma esse estado compacto no estado convidado exigido.

Essa função usa valores retidos, versões de memória e resultados calculados; não interpreta um rastro de instruções PS2. Sua correção integra o contrato da cápsula.

O compilador calcula retroativamente o conjunto de valores exigidos por observadores futuros. Se um valor eliminado ainda puder ser lido, deve conservar informação suficiente para reconstruí-lo. Leituras desconhecidas ampliam conservadoramente esse conjunto.

A reconstrução pode exigir cópias ou recomputação, cujo custo entra na escolha da variante. Dados de entrada necessários não podem ser descartados ou sobrescritos antes da última porta que deles depende.

### 20.2 Fronteiras móveis, sem efeitos retroativos

A posição das fronteiras entre componentes passa a ser uma decisão de compilação: uma versão mantém VIF, VU e GS separados; outra atravessa duas dessas fronteiras; uma terceira funde toda a região autorizada.

Todas preservam o mesmo contrato externo e o calendário convidado, inclusive efeitos de contenção que permaneçam relevantes. O runtime seleciona uma versão já compilada segundo guardas e capacidade do host.

Essa seleção altera a representação e a distribuição do trabalho. Não altera retroativamente um efeito já observado, não muda a cadência convidada e não requer gerar código de máquina novo.

## 21. Compilação e entrega por plataforma

### 21.1 Alvos independentes

| Alvo | Saída proposta | Validação obrigatória |
|---|---|---|
| Linux x86-64 | Executável e bibliotecas nativas | ABI, SIMD, drivers, áudio e entrada |
| Linux ARM64 | Executável ARM64 | Paridade semântica e comportamento da plataforma |
| Windows x86-64/ARM64 | Pacote por arquitetura anunciada | ABI, exceções, armazenamento e backend gráfico |
| macOS ARM64 | Aplicativo assinado conforme distribuição | Backend gráfico, ciclo de vida e empacotamento |
| Android ARM64 | APK instalável e recursos associados | Hardware físico, áudio, touch, storage e termal |

A aprovação de uma ABI não se transfere automaticamente às demais. A primeira linha de implementação será Linux x86-64 e Android ARM64; outras plataformas só serão anunciadas após seus gates.

### 21.2 Android

Requisitos específicos:

- AOT de todo código convidado necessário, incluindo IOP e VU.
- Integração nativa de superfície, áudio, controles e ciclo de vida.
- Suspend/resume, perda de superfície e recuperação de contexto gráfico.
- Tratamento de perda de dispositivo, pressão de memória e interrupções de áudio.
- Layout touch automático e suporte a controle externo.
- Bibliotecas sem pressupor páginas host de 4 KiB.
- Validação de alinhamento e execução em configurações de páginas de 16 KiB, conforme a documentação Android. [R10] genui{"citation":{"ref":"turn2search4"}}
- Assets grandes em armazenamento apropriado, com acesso aleatório e sem cópia integral desnecessária a cada execução.
- Assinatura, integridade e atualização de pacote reproduzíveis dentro dos limites do mecanismo de assinatura.

Não se transportam bibliotecas `.so` x86-64 do cache Linux para ARM64. Reutilizam-se IR, contratos e resultados independentes da arquitetura; os objetos nativos são gerados novamente.

### 21.3 Recursos e tamanho

O pacote pode conter um arquivo de recursos que preserve a vista de setores necessária e índices de acesso. Extrações derivadas são armazenadas somente quando compensam seu custo.

Código e dados originais podem ser lidos para validação, checksums e serviços. Sua presença no pacote não implica interpretação, desde que o runtime não execute a ISA convidada a partir deles.

O build não deve criar uma função independente para cada binding quando múltiplas entradas compartilham uma região. O tamanho será controlado por compartilhamento de templates, blocos frios compactos e otimização seletiva.

### 21.4 Reprodutibilidade

Toolchains, dependências, regras e parâmetros terão identidade fixa. A geração manterá conteúdo e timestamps quando nada mudar, preservando o benefício incremental do protótipo.

Assinaturas e metadados variáveis serão separados da reprodução dos payloads. Dois builds devem poder comparar exatamente os componentes determinísticos e explicar diferenças esperadas.

### 21.5 Auditoria de natividade

A auditoria combina construção por componentes permitidos, proveniência das operações geradas, inspeção de dependências e rastros do executável final. Procurar símbolos com nomes de interpretadores, isoladamente, é insuficiente.

O runtime autorizado não possui uma operação genérica capaz de consumir bytes EE/IOP/VU e executá-los. Consultas de identidade podem escolher funções existentes; não podem sintetizar instruções host. Páginas de dados convidadas não se tornam páginas executáveis do host.

Bibliotecas carregáveis devem integrar o inventário AOT aprovado. Instrumentação registra qualquer criação de código executável e identifica exceções próprias da plataforma, como o driver gráfico. Um mecanismo de extensão que pudesse introduzir traduções convidadas novas invalidaria o perfil estrito.

## 22. Autonomia: diagnóstico e reparo

O motor de autonomia será uma máquina de estados com orçamento, não um loop ilimitado de edição.

```mermaid
stateDiagram-v2
    [*] --> Construir
    Construir --> Comparar
    Comparar --> Explorar: equivalência local
    Comparar --> Localizar: divergência
    Localizar --> Minimizar
    Minimizar --> Propor
    Propor --> Verificar
    Verificar --> Construir: reparo aprovado
    Verificar --> Propor: contraprova
    Explorar --> Construir: código novo
    Explorar --> Avaliar: critérios cobertos
    Avaliar --> Aceito: gates aprovados
    Avaliar --> Bloqueado: obrigação aberta
    Propor --> Bloqueado: orçamento esgotado
    Aceito --> [*]
    Bloqueado --> [*]
```

### 22.1 Primeira divergência causal

O sistema compara checkpoints canônicos e usa hashes hierárquicos para localizar a região divergente. A investigação retrocede até os produtores dos valores ou eventos afetados.

“Primeira divergência” significa a primeira no alinhamento causal estabelecido. Dois hosts podem executar operações internas em ordens distintas e ainda produzir o mesmo comportamento; comparar número de instrução host ou timestamp de parede geraria falsos diagnósticos.

### 22.2 Redução de casos

A minimização preserva a falha e suas dependências: estado inicial, bytes de código, pipeline, memória relevante, eventos pendentes e entrada externa.

O resultado é uma fixture portátil com descrição da propriedade violada. Cortar o replay sem conservar um evento pendente pode apagar a causa ou fabricar outra falha.

### 22.3 Classes de reparo

| Falha | Ação admissível |
|---|---|
| Alvo desconhecido | Refinar descoberta e fechamento |
| Tradução incorreta | Corrigir lowering ou regra semântica demonstrada |
| ABI incorreta | Refinar contrato de chamada e adaptador |
| Ordenação errada | Restaurar dependência ou fronteira temporal |
| Família mal generalizada | Restringir guarda válida ou ampliar prova e implementação |
| Otimização incorreta | Rejeitar variante e usar caminho nativo conservador |
| Desempenho insuficiente | Gerar candidato equivalente e medir |
| Modelo de hardware incerto | Solicitar evidência independente ao laboratório; manter obrigação aberta |

### 22.4 Limites de autoridade

O proponente não pode modificar os critérios de aceitação, apagar testes, alterar observações obrigatórias, substituir o oráculo pelo próprio candidato ou excluir estados difíceis do domínio para obter aprovação.

Uma guarda mais restrita pode desabilitar uma otimização, desde que o caminho conservador cubra os demais casos. Ela não pode restringir silenciosamente o conjunto de entradas aceitas pelo produto.

Patches por endereço que pulam lógica, retornos artificiais de sucesso, omissão de geometria e ajustes visuais sem equivalência são rejeitados pelo perfil fiel.

### 22.5 Atualização do modelo semântico

Uma especificação de hardware incorreta não é consertada provando conformidade com ela mesma. Alterações nessa especificação exigem evidência independente e uma nova versão semântica.

Durante uma avaliação zero-shot, a especificação e os critérios ficam congelados. Se precisarem mudar, o resultado pertence a uma nova rodada e não reescreve a anterior como sucesso automático.

## 23. Papel da inteligência artificial

Modelos de linguagem e outros modelos aprendidos podem:

- Formular hipóteses sobre estruturas, bibliotecas, ABIs e materializadores.
- Propor invariantes de loop e particionamentos de cápsulas.
- Gerar candidatos de otimização e reparo.
- Planejar exploração de menus e rotas.
- Produzir tentativas de prova e explicações de contraprovas.

Não podem servir como oráculo final de equivalência nem autorizar uma transformação apenas por confiança estatística.

O núcleo de aceitação verifica os artefatos produzidos. Se uma prova contém axiomas novos, passos admitidos ou dependências não verificadas, isso aparece no certificado e pode impedir a aprovação.

A arquitetura continua operacional sem um modelo específico. A ausência de inferência pode aumentar o custo ou reduzir a taxa de fechamento, mas não muda o significado de `proved`.

Texto encontrado em imagens de disco, nomes de arquivos, strings e documentação extraída é dado não confiável. Não altera instruções do orquestrador, permissões, critérios ou acesso a serviços.

## 24. Exploração automática e evidência de conteúdo

### 24.1 Objetivos de exploração

O explorador procura novos estados e obrigações, não apenas novos endereços. Sua função de prioridade considera:

\[
U=\alpha C_{code}+\beta C_{device}+\gamma C_{state}
+\delta C_{progress}+\eta C_{uncertainty}-\lambda Cost.
\]

Os coeficientes são parâmetros de planejamento. Essa função não é um critério de correção.

Fontes de exploração incluem entradas sistemáticas, políticas aprendidas, análise de condições, execução simbólica localizada, mutações válidas de saves e replays de eventos.

### 24.2 Controle causal

Exibir um personagem não prova que os controles funcionam. O teste deve comparar execuções com entradas diferentes e verificar alterações causais coerentes no estado e na saída.

O mesmo princípio vale para pause, menus, inventário, câmera, interação, save/load e transições. Resultados podem ser específicos do gênero; a inferência automática desses objetivos continua sendo problema de pesquisa.

### 24.3 Campanha e modos

Uma rota de aceitação deve ser produzida automaticamente para que o resultado conte como zero-touch integral. Rotas humanas podem formar um conjunto separado de avaliação funcional, com sua proveniência indicada.

Checkpoints inseridos artificialmente podem exercitar estados profundos, mas não demonstram que a progressão até eles funciona. Uma aprovação de campanha requer uma cadeia válida de estados alcançados ou um contrato de avaliação explicitamente mais limitado.

Jogos sem campanha linear usam objetivos adequados: partidas, temporadas, rotas, personagens, pistas, sessões prolongadas e persistência. O sistema não deve inventar uma condição de conclusão universal.

### 24.4 Limite da exploração

Horas sem falha e saturação aparente de cobertura não provam fechamento. Exploração fornece casos e contraprovas; análise conservadora e certificados tratam a abrangência formal.

Se a ferramenta não consegue validar suficientemente o conteúdo, emite `VALIDATION_INCOMPLETE`, mesmo que já tenha produzido um executável.

## 25. Verificação, oráculos e base de confiança

### 25.1 Camadas de evidência

| Camada | Método | Limitação explícita |
|---|---|---|
| Instruções | Especificação executável, testes dirigidos e provas locais | Depende de semântica correta |
| Blocos | Comparação simbólica e concreta de estado e efeitos | Depende do domínio e das condições de entrada |
| Loops e famílias | Invariantes, indução ou prova equivalente | Não resolvidos automaticamente em geral |
| Dispositivos | Testes de protocolo, rastros e hardware de referência | Cobertura física limitada |
| Cápsulas | Prova de composição e reconstrução | Pressupostos de interface precisam ser completos |
| Backend | Validação de tradução e testes de código final | Trechos não verificados permanecem na base de confiança |
| Jogo | Exploração, rotas e comparação audiovisual | Evidência de conteúdo, não prova universal de todos os inputs |
| Plataforma | Dispositivo real, ciclo de vida e desempenho | Resultado restrito à configuração medida |

### 25.2 Independência

Serão usados, quando disponíveis, microtestes em hardware PS2, um modelo executável separado e implementações independentes como referências comparativas.

PCSX2 é uma implementação madura útil para investigação diferencial, mas não é um oráculo infalível. Reutilizar a mesma fórmula errada em gerador, referência e teste pode produzir concordância falsa. [R7] genui{"citation":{"ref":"turn1view4"}}

Cada evidência terá uma marca de independência: origem física, implementação independente, código compartilhado ou autorreferência. Resultados do último grupo não resolvem sozinhos dúvidas de semântica.

### 25.3 Especificação executável

Sail é uma opção para formalizar ISAs e gerar modelos executáveis e definições para ferramentas de prova. Isso não significa que já exista nele um modelo completo e validado de R5900, IOP e VU adequado a este projeto. [R2] genui{"citation":{"ref":"turn1view3"}}

O projeto pode adotar outra linguagem, desde que mantenha interpretação precisa dos bits, geração de testes e conexão com o verificador.

### 25.4 Base de confiança

O manifesto identifica, no mínimo:

```text
modelo de hardware e ambiente
kernel de prova e verificador de certificados
parsers e representação do problema
solvers sem certificados verificáveis, quando utilizados
partes não verificadas do compilador e linker
runtime, bibliotecas e backend gráfico
sistema operacional, driver e hardware host
```

CompCert demonstra que preservação semântica precisa de um escopo definido e explicita fronteiras de confiança, como etapas externas. Usá-lo ou adotar sua metodologia não prova automaticamente o pipeline PS2 completo. [R5] genui{"citation":{"ref":"turn1view0"}}

### 25.5 Integridade e correção

Hashes verificam identidade e integridade. Assinaturas vinculam artefatos a um emissor. Certificados descrevem propriedades demonstradas sob pressupostos. Testes mostram resultados em execuções determinadas.

Nenhuma dessas quatro categorias substitui as demais.

### 25.6 Nível de garantia separado do modo de execução

`strict-aot` descreve como o código convidado é executado. Não significa, por si, que todo bit do aplicativo foi formalmente verificado.

O certificado registra separadamente `reference-tested`, `region-proved` ou `end-to-end-proved`, sempre acompanhado do domínio e da base de confiança. Somente o último pode ser descrito como prova de ponta a ponta, e apenas se a cadeia efetivamente estiver coberta.

Aceitação empírica de fidelidade deve ser chamada de validação de fidelidade; não de teorema sobre todas as execuções. A prova de fechamento também permanece relativa ao modelo de hardware e ambiente declarado.

## 26. Desempenho e contrato de 60+ FPS

### 26.1 Quatro frequências

O relatório de desempenho distingue:

```text
simulation_hz       evolução de estados de lógica
render_hz           renderizações calculadas para instantes do jogo
present_hz          imagens apresentadas pelo host
generated_hz        imagens repetidas ou sintetizadas por pós-processamento
```

Uma cena estática pode produzir quadros calculados com pixels idênticos. Por isso, contar hashes visuais distintos também não mede corretamente a frequência de renderização. A instrumentação deve registrar a origem e o instante lógico dos quadros.

### 26.2 Perfis temporais

| Perfil | Garantia pretendida | Relação com a meta absoluta |
|---|---|---|
| `faithful-time` | Velocidade e cadência convidadas preservadas | Base de compatibilidade fiel |
| `presentation-60` | Apresentação host a pelo menos 60 Hz | Não demonstra 60 quadros reais do jogo |
| `native-60` | Lógica/renderização adequadas a pelo menos 60 quadros reais | Exige transformação adicional quando a origem não oferece essa cadência |
| `native-unlocked` | Cadência variável acima do mínimo certificado | Exige contrato explícito de independência temporal |

Cadências fracionárias e diferenças entre campos e quadros são registradas exatamente. Um modo de aproximadamente 59,94 Hz não será descrito como prova literal de um limite mínimo de 60,000 Hz.

### 26.3 Transformação temporal proposta

O NEXO investigará recuperação de relações entre tempo, física, animação, câmera, scripts, áudio e renderização. Variáveis candidatas serão analisadas por fluxo de dados e intervenção controlada.

Uma transformação só poderá ser promovida quando tiver um contrato novo explícito, por exemplo:

- Tempo de jogo e áudio preservados em segundos.
- Eventos discretos mantidos na sequência correta.
- Estados nos instantes originais relacionados ao comportamento de referência.
- Novos estados intermediários definidos por uma regra verificada.
- Saves, input, colisões e scripts consistentes com essa regra.

Não existe informação suficiente, em geral, para recuperar a intenção original do desenvolvedor nos instantes intermediários. Portanto, isso certifica uma extensão especificada do jogo, não uma equivalência automaticamente deduzida em todos os tempos.

Se a transformação não for demonstrada, a conversão pode passar em `faithful-time` e falhar em `native-60`. A taxa que satisfaz a meta absoluta será calculada pela interseção dos requisitos, sem substituir uma pela outra.

### 26.4 Critérios de estabilidade

Para um dispositivo, resolução, rota e perfil fixados, medir:

- Tempos de CPU, GPU, espera e frame completo, com percentis.
- Deadlines perdidos e duração de sequências de atraso.
- Velocidade convidada em relação ao tempo real.
- Latência de entrada e áudio, underruns e deriva audiovisual.
- Memória, consumo e temperatura em regime sustentado.
- Carregamento, primeira execução, compilação de shaders do driver e retomada.

Um SLO inicial proposto para `native-60` é cumprir pelo menos 99,9% dos deadlines de 16,667 ms nas janelas interativas definidas, sem desacelerar o relógio convidado, após aquecimento documentado. Devem ser divulgados também o pior atraso e a sequência máxima de perdas.

Esse SLO estatístico não significa ausência matemática de todo stutter. “Estável” deve sempre apontar para o SLO e para a duração medida. Testes sustentados de pelo menos 30 minutos são um gate inicial, complementado pelas rotas longas de conteúdo.

### 26.5 Limites físicos e planejamento

O tempo mínimo depende do caminho crítico, do trabalho, da largura de banda e da comunicação. Um limite inferior simplificado é:

\[
T_{frame}\ge
\max(T_{critical},W_{cpu}/P_{cpu},W_{gpu}/P_{gpu},B/BW).
\]

Os termos são estimativas dependentes da máquina; não são previsão completa. Sincronização, caches, latência e concorrência podem aumentar o custo.

Mover VU para GPU pode reduzir computação e aumentar comunicação. Fundir cápsulas pode eliminar cópias e elevar pressão de registradores. A política de escolha exige medições no alvo e mantém alternativas nativas semanticamente equivalentes.

Não se promete 60 FPS em qualquer computador ou celular. O certificado de desempenho identifica o dispositivo ou uma classe de capacidade definida por testes reproduzíveis.

## 27. Compatibilidade, autonomia e avaliação zero-shot

### 27.1 Denominador

O catálogo deve identificar título, região, revisão e hash, com uma política de agrupamento publicada antes da avaliação. Duplicatas e revisões não podem ser acrescentadas seletivamente para aumentar a taxa.

Serão divulgadas métricas por imagem/revisão e por título agrupado. Ambas incluirão falhas, timeouts, dependências externas e validação incompleta.

### 27.2 Taxas separadas e conjuntas

Para um catálogo \(L\), versão congelada \(v\), orçamento \(b\) e plataforma \(p\):

\[
C_{fiel}(L,v,b,p)=
\frac{|\{g\in L:AOT(g)\land Auto(g)\land Fiel(g,p)\}|}{|L|}.
\]

\[
C_{absoluto}(L,v,b,p)=
\frac{|\{g\in L:AOT(g)\land Auto(g)\land FielBase(g,p)
\land Native60(g,p)\}|}{|L|}.
\]

`FielBase` exige uma base fiel aprovada; `Native60` exige a extensão temporal aprovada quando necessária. Não são duas equivalências simultâneas incompatíveis da mesma execução.

Para “PC e Android”, calcula-se também a interseção dos sucessos nas plataformas anunciadas. A média de 95% no PC e 85% no Android não demonstra 90% funcionando em ambos.

### 27.3 Protocolo zero-shot

Antes da rodada, congelar:

```text
compilador, runtime, modelo semântico e regras
modelos aprendidos e prompts operacionais
catálogo, critérios, orçamento e dispositivos
oráculos, corpus de treinamento e caches permitidos
```

A submissão não pode consultar patches, perfis, replays, endereços ou soluções previamente preparados para aquele título. Conhecimento genérico de ISA, dispositivos e bibliotecas comuns é permitido, com proveniência registrada.

Análise, exploração, síntese de artefatos e busca durante a conversão são permitidas: fazem parte da inferência do pipeline. Se “zero-shot” for usado no sentido estrito de aprendizado de máquina, seus critérios precisam ser publicados separadamente; ausência de intervenção não garante ausência de dados anteriores no treinamento de um modelo.

### 27.4 Aprendizado entre rodadas

Correções genéricas encontradas numa rodada podem melhorar a próxima versão. Não podem ser aplicadas retroativamente e contadas como se a versão congelada anterior já tivesse resolvido aqueles títulos.

Resultados com cache aquecido do próprio jogo pertencem a uma categoria diferente de conversão fria inédita. Reuso de módulos genéricos deve ser discriminado.

### 27.5 Amostragem e confiança

Se todo o catálogo for avaliado, a fração observada é uma medida desse catálogo e desses critérios. Se apenas uma amostra for avaliada, é necessária inferência estatística coerente com o desenho amostral.

Como ilustração, 95 sucessos em 100 itens aleatórios têm limite inferior de aproximadamente 88,8% no intervalo bilateral de Wilson de 95%; isso não demonstra um piso de 90%.

Jogos relacionados por engine ou revisão são correlacionados. Amostragem estratificada, agrupamento por engine e análise de sensibilidade serão necessários. Uma amostra escolhida entre títulos já conhecidos por funcionar não sustenta inferência global.

## 28. Pipeline de conversão proposto

### 28.1 Estados do job

```text
RECEIVED -> IDENTIFIED -> INVENTORIED -> MODELED -> DISCOVERED
-> LOWERED -> CONTRACTS_CHECKED -> BUILT -> EXERCISED
-> EVALUATED -> PACKAGED
```

Os estados não equivalem a aprovação de compatibilidade. Um job pode produzir um pacote experimental e manter `compatibility=unknown`.

### 28.2 Algoritmo principal

```text
convert(image, targets, policy):
    job = freeze_inputs_and_policy(image, targets, policy)
    disc = ingest_without_losing_sector_semantics(image)
    model = instantiate_environment(policy.semantic_profile)
    graph = discover_initial_programs(disc, model)

    while job.has_budget():
        exact_ir = lower_with_explicit_effects(graph, model)
        baseline = compile_native_baseline(exact_ir, targets)
        candidates = propose_abstractions_and_families(exact_ir)
        checked = verify_candidates(candidates, baseline, policy)
        closure = establish_code_closure(graph, checked, policy)
        binaries = build_target_artifacts(baseline, checked)
        evidence = explore_and_compare(binaries, disc, model, policy)

        if evidence.has_counterexample():
            graph, checked = refine_from_minimized_case(evidence)
            continue

        if gates_pass(closure, evidence, binaries, policy):
            return package_with_scoped_certificate(job, binaries, evidence)

        if no_justified_next_action():
            break

    return structured_unresolved_result(job)
```

`no_justified_next_action` encerra uma busca dentro de orçamento. Não declara que o problema é insolúvel.

### 28.3 CLI futura

Os comandos abaixo especificam uma interface a implementar; não são comandos já existentes no protótipo auditado.

```sh
ps2native convert jogo.iso \
  --target linux-x86_64 \
  --target android-arm64 \
  --contract strict-aot \
  --evaluation zero-shot \
  --out pacote

ps2native inspect-certificate pacote
ps2native verify-package pacote
ps2native replay caso.nexo
ps2native benchmark pacote --device dispositivo --route rota
```

O fluxo padrão não solicita nome de engine, TOML, endereços ou seleção de hacks. Opções avançadas definem plataforma, recursos e contrato; não terceirizam ao usuário a correção do título.

### 28.4 Interface nativa interna

IDL conceitual:

```text
invoke(capsule_id, machine_state, event_horizon) -> Exit

Exit =
    Continue(next_entry)
  | Yield(event_id, canonical_state)
  | GuestException(exception_state)
  | CodeVersionChange(version_id, resume_state)
  | ContractFailure(reason, reproducible_case)
```

`GuestException` representa comportamento do programa original. `ContractFailure` representa limite ou falha do sistema de conversão; não deve ser transformado automaticamente em uma exceção convidada equivalente sem fundamento.

## 29. Manifesto, evidências e diagnóstico

### 29.1 Exemplo de manifesto não aprovado

O exemplo abaixo ilustra a estrutura; não descreve uma conversão executada.

```json
{
  "schema": "nexo.package.v1",
  "example": true,
  "artifact_status": "built",
  "acceptance_status": "blocked",
  "input": {
    "image_digest": null,
    "catalog_revision": null
  },
  "contract": {
    "requested_execution": "strict-aot",
    "requested_temporal_profile": "native-60",
    "semantic_profile_digest": null,
    "environment_domain_digest": null
  },
  "native_execution": {
    "ee": "aot",
    "iop": "aot",
    "vu0": "aot",
    "vu1": "aot",
    "guest_interpreter_present": false,
    "guest_runtime_compiler_present": false,
    "code_closure": "unknown"
  },
  "validation": {
    "boot": "passed",
    "content": "incomplete",
    "audio": "unknown",
    "save_reload": "unknown",
    "native_60": "unmeasured"
  },
  "zero_shot": {
    "pipeline_frozen": true,
    "title_specific_prior_artifacts": false,
    "human_interventions": 0
  },
  "unresolved": [
    "CODE_FAMILY_CLOSURE",
    "CONTENT_VALIDATION",
    "TARGET_PERFORMANCE"
  ]
}
```

Campos obrigatórios ausentes impedem aceitação real. Valores nulos só são válidos em exemplos ou resultados parciais explicitamente marcados.

### 29.2 Certificado de cápsula

Cada certificado registra:

```text
identidade da cápsula e dos bytes de origem
entradas e contextos cobertos
pré-condição e domínio quantificado
efeitos, tempo e hipóteses de ambiente
famílias de código e suas provas de abrangência
mapas de reconstrução e pontos de commit
implementações conservadora e otimizada
obrigações, métodos, resultados e limites
identidade dos verificadores e dependências
```

Um certificado de loop limitado inclui o limite. Um certificado de teste inclui os casos. Nenhum é serializado como prova universal.

### 29.3 Códigos de resultado

| Código | Significado |
|---|---|
| `ACCEPTED_STRICT` | Todos os gates do contrato solicitado passaram |
| `BUILT_UNVERIFIED` | Pacote construído, aprovação insuficiente |
| `UNSUPPORTED_IMAGE_LAYOUT` | Organização da entrada ainda não suportada |
| `INCOMPLETE_INPUT` | Informação necessária ausente na imagem |
| `UNRESOLVED_SEMANTICS` | Comportamento necessário sem modelo suficiente |
| `UNRESOLVED_CODE_CLOSURE` | Família ou alvo possível fora da cobertura demonstrada |
| `REFERENCE_DISAGREEMENT` | Oráculos divergem em ponto relevante |
| `VALIDATION_INCOMPLETE` | Conteúdo ou funcionalidade não suficientemente exercitados |
| `TARGET_PERFORMANCE_FAILED` | Critério de desempenho não atingido |
| `EXTERNAL_DEPENDENCY` | Serviço ou periférico exigido não disponível no contrato |
| `RESOURCE_BUDGET_EXCEEDED` | Busca ou build excedeu orçamento |
| `UNSEEN_CODE` | Execução alcançou versão não coberta pelo pacote |

Cada resultado inclui causa, escopo, evidência e próxima obrigação técnica. Um retorno não aprovado não conta como compatibilidade.

## 30. Organização de implementação

A estrutura a seguir é proposta, não um inventário de diretórios existentes.

| Diretório | Responsabilidade |
|---|---|
| `spec/isa/` | Semânticas EE, IOP e VU |
| `spec/devices/` | Protocolos, memória e eventos de dispositivos |
| `spec/contracts/` | Contratos, perfis e obrigações formais |
| `ingest/` | Imagens, setores, arquivos e inventário |
| `analysis/` | Fluxo, aliasing, materializadores e fechamento |
| `ir/` | IET e ligações entre suas vistas |
| `decompile/` | Hipóteses estruturais e recuperação de abstrações |
| `synthesis/` | Famílias, invariantes e candidatos |
| `verify/` | Certificados, provas, contraprovas e limites |
| `codegen/` | Backends e geração conservadora |
| `runtime/` | Estado, eventos, serviços e despacho AOT |
| `graphics/` | Memória GS e backends acelerados |
| `lab/` | Oráculos, exploração, snapshots e redução |
| `platform/` | Integração Linux, Android e outros alvos |
| `pipeline/` | Jobs, orçamento, cache e empacotamento |
| `tests/` | Microtestes, fixtures, integração e regressões |
| `schemas/` | Formatos canônicos e migrações de versão |

### 30.1 Linguagens e fronteiras

Uma implementação viável pode manter C++ no runtime existente, usar Rust em parsers e orquestração sensíveis à memória, LLVM no backend e uma linguagem de especificação/prova nas semânticas.

Essa seleção é substituível. Não se propõe reescrever tudo antes de obter o primeiro resultado. As fronteiras devem usar formatos explícitos, ownership e ABIs pequenos, evitando propagar estruturas internas por todo o projeto.

### 30.2 Construção incremental

Artefatos serão endereçados por conteúdo e dependências semânticas. Alterar a regra de uma instrução invalida regiões que a utilizam e seus certificados dependentes; alterar um header genérico desnecessariamente não deve reconstruir todo o jogo.

Unidades frias usam otimização barata. Regiões quentes recebem otimização adicional somente quando o benefício previsto justifica compilação e prova. Builds de laboratório e release mantêm flags e identidades distintas.

### 30.3 Replay portátil

O formato usa campos de largura fixa, endianness definida, versões, checksums e referências a blobs. Não contém ponteiros host ou dumps opacos de ABI.

O replay inclui efeitos externos necessários e estado oculto. A execução em x86-64 e ARM64 deve reconstruir o mesmo estado convidado canônico antes de comparar resultados.

## 31. Migração do PS2Native existente

| Etapa | Mudança | Gate de saída |
|---|---|---|
| M0 | Congelar evidências e separar build de aprovação | Manifests sem estados ambíguos |
| M1 | Introduzir estado canônico e eventos portáteis | Replay equivalente em duas arquiteturas |
| M2 | Formalizar memória, entradas e semântica EE prioritária | Diferenciais e regressões sem stubs de sucesso |
| M3 | Migrar overlays para banco de versões e famílias | Detectar escritas, aliases e mudanças de fetch |
| M4 | Produzir IOP AOT | Módulos e serviços exercitados sem interpretação |
| M5 | Produzir VU AOT com pipeline | Estado completo e continuações validados |
| M6 | Acelerar GS mantendo memória e efeitos | Imagem, readbacks e transferências aprovados |
| M7 | Fechar primeiro jogo integral | Progressão, áudio, controles e saves em build normal |
| M8 | Aprovar Android físico | Instalação, execução sustentada e ciclo de vida |
| M9 | Implementar fechamento e cápsulas avançadas | Famílias e fusões demonstradas em casos reais |
| M10 | Automatizar reparo e validação | Submissões inéditas sem correção humana |
| M11 | Avaliar catálogo congelado | Taxas conjuntas e falhas publicadas |

As etapas representam dependências e gates, não um cronograma linear obrigatório. O trabalho de semântica, backend e infraestrutura pode ocorrer em paralelo quando suas interfaces estiverem estabelecidas.

### 31.1 Primeiro incremento concreto

O primeiro incremento deve extrair do caso Monster House um replay portátil que contenha entrada VIF, microcódigo VU, estado oculto necessário e saída GIF/GS.

Esse caso será executado por:

1. Implementação de referência escolhida e identificada.
2. Caminho atual, preservado para regressão.
3. Novo VU AOT conservador.
4. Variante otimizada, quando houver prova e benefício.

A entrega desse incremento inclui o frontend mínimo, a serialização, o mapa de divergências e a medição em CPU física. Ele resolve uma fronteira concreta sem exigir que a descompilação universal esteja pronta.

### 31.2 Preservação do trabalho existente

Ingestão, CLI, fixtures, redução do registro, geração incremental e isolamento headless permanecem úteis. O driver Linux de overlays passa a ser ferramenta de laboratório.

Hacks diagnósticos ficam identificados e fora do perfil fiel. Relatórios antigos continuam disponíveis com data e escopo; a migração não converte retrospectivamente evidência parcial em aprovação completa.

## 32. Experimentos que podem refutar a proposta

| Hipótese | Experimento | Resultado que a enfraquece |
|---|---|---|
| H1: famílias de materializadores cobrem parcela relevante do código latente | Classificar ISOs inéditas e tentar fechamento sem rastros prévios | Predomínio de famílias não fecháveis no orçamento |
| H2: IET reduz erros de fronteira | Comparar diagnósticos e reparos com pipeline separado | Custo maior sem redução de divergências |
| H3: fusão causal melhora execução | Ablation com mesmas semânticas e backends | Ganho eliminado por guardas, sincronização ou reconstrução |
| H4: VU AOT elimina overhead importante | Comparar V0, V1 e V2 em corpus distinto | Código maior ou regressões sem ganho sustentado |
| H5: GS acelerado permanece fiel | Comparar VRAM, efeitos e frames sob feedback e aliasing | Divergências persistentes ou serialização dominante |
| H6: reparos generalizam | Aplicar regra a títulos e microtestes retidos | Melhora local acompanhada de regressões externas |
| H7: automação alcança conteúdo profundo | Avaliar rotas inéditas com orçamento fixo | Exploração presa em menus ou ausência de aprovação confiável |
| H8: 60 nativo pode ser ampliado automaticamente | Transformar cadências em conjunto diverso | Quebras temporais ou necessidade frequente de ajuste manual |

### 32.1 Ablations obrigatórias

Comparar a arquitetura completa com variantes sem fechamento por materializadores, sem fusão multicomponente, sem propostas aprendidas, sem otimização VU avançada e sem reuso entre módulos.

Manter corpus, hardware, critérios e orçamento comparáveis. Medir taxa de sucesso, custo de conversão, tamanho, tempo de prova, desempenho e falhas. Um aumento de compatibilidade obtido com cem vezes mais orçamento precisa ser apresentado como tal.

### 32.2 Testes adversariais de correção

O conjunto inclui:

- Automodificação que atinge a próxima instrução ou outro alias.
- Código lido como dados depois de uma otimização.
- Entrada direta em delay slot e exceção durante o slot.
- Mudança de microprograma entre início e continuação VU.
- VIF/GIF fragmentados em todas as fronteiras relevantes.
- DMA concorrente com leituras e publicação de código.
- Wrap de contadores e alarmes durante suspensão.
- Feedback gráfico com vistas diferentes da mesma memória.
- Callbacks que observam buffers intermediários.
- Saves interrompidos e retomada com estado externo alterado.

Os casos devem tentar falsificar propriedades, não apenas reproduzir a estrutura da implementação.

## 33. Recursos, escala e serviço de conversão

### 33.1 Orçamento multidimensional

Cada job recebe limites separados para:

```text
tempo de parede e CPU acumulada
memória e armazenamento temporário
estados de exploração e checkpoints
consultas de solver e tamanho de prova
candidatos de reparo e tentativas de build
uso de inferência e custo financeiro, quando aplicável
```

O esgotamento de uma dimensão gera resultado parcial reproduzível. Não reduz silenciosamente a qualidade exigida.

### 33.2 Reuso

Compartilhar semânticas, runtime, contratos de bibliotecas e IR comprovadamente idêntica pode reduzir custo entre títulos. Igualdade de nome, engine presumida ou semelhança de bytes não basta.

Reuso por família inclui o predicado de validade e suas dependências. Uma nova versão do modelo semântico invalida certificados que dependiam do comportamento alterado.

### 33.3 Execução distribuída

Análise de módulos independentes, provas locais, backends por ABI e testes distintos podem ser distribuídos. Fechamento global e integração causal continuam exigindo coordenação.

O serviço terá armazenamento de artefatos, filas duráveis, workers isolados, cancelamento, checkpoints, limites e trilhas de versão. O relatório não identificou esse serviço implementado no projeto atual.

A nuvem aumenta capacidade de trabalho; não resolve ausência de semântica, falta de oráculo ou uma obrigação não demonstrada.

### 33.4 Isolamento

ISOs são entradas binárias potencialmente defeituosas. Parsers, decodificadores e execuções exploratórias devem operar em ambientes isolados, sem acesso implícito a credenciais ou arquivos externos.

Saves e dados de usuário permanecem separados dos artefatos compartilhados. Pacotes derivados de imagens não são publicados automaticamente; reutilização técnica e distribuição de conteúdo são operações distintas.

## 34. Riscos fundamentais e respostas

| Risco | Resposta de arquitetura | Limite remanescente |
|---|---|---|
| Semântica incorreta | Modelos explícitos e evidência independente | Hardware mal caracterizado continua sendo risco |
| Explosão de estados | Abstração, famílias e composição | Pode impedir fechamento no orçamento |
| Provas caras | Provas locais, reuso e baseline nativa | Regiões complexas podem ficar sem otimização |
| Guardas caras | Guardas amortizadas e dependências incrementais | Podem consumir o ganho de desempenho |
| Código AOT excessivo | Compartilhamento e parametrização | Famílias irregulares podem crescer muito |
| GS difícil de paralelizar | Dependências e caminhos exatos | Fidelidade pode limitar velocidade |
| Causalidade temporal incompleta | Portas de observação e agenda conservadora | Precisão exige caracterização suficiente |
| Autorreparo enganoso | Avaliador independente e critérios imutáveis | Não garante encontrar reparo válido |
| Exploração insuficiente | Rotas, estados e análise conservadora | Validação de conteúdo pode ficar incompleta |
| Android heterogêneo | Certificados por configuração e medições físicas | Não há desempenho universal |
| Dependência externa ausente | Contrato explícito e modelo quando possível | A ISO sozinha pode ser insuficiente |
| Meta conjunta não alcançada | Medição de interseção e causas de falha | Arquitetura não substitui demonstração empírica |

O resultado aceitável de pesquisa pode ser descobrir que alguma combinação de cobertura, custo, AOT estrito e cadência não é atingível com esta abordagem. O sistema deve tornar essa conclusão mensurável, em vez de protegê-la por terminologia.

## 35. Referências e proveniência

As referências delimitam componentes conhecidos e obrigações técnicas. Elas não atestam o ineditismo do NEXO nem demonstram suas metas de compatibilidade.

- **[R0] Relatório fornecido:** `RELATORIO_PS2NATIVE_ESTADO_ATUAL.md`, auditoria de 30/09/2026, registro consolidado consultado em `2026-09-30T13:24:04.546896+00:00`. Fonte das observações sobre o protótipo e Monster House; resultados anteriores, não reexecutados neste documento.
- **[R1] PS2Recomp:** [repositório do projeto](https://github.com/ran-j/PS2Recomp). Documenta tradução R5900 para C++, configuração e runtime; sua versão pública não substitui a auditoria da árvore local descrita no relatório. genui{"citation":{"ref":"turn3view0"}}
- **[R2] Sail:** [linguagem de especificação de ISAs](https://github.com/rems-project/sail). Referência para modelos executáveis, semântica de instruções e integração com ferramentas de prova.
- **[R3] Alive2:** [artigo dos autores sobre validação limitada de tradução](https://users.cs.utah.edu/~regehr/alive2-pldi21.pdf) e [implementação](https://github.com/AliveToolkit/alive2). O limite de desenrolamento de loops impede interpretar toda aprovação como garantia irrestrita.
- **[R4] egg:** [documentação de e-graphs e equality saturation](https://egraphs-good.github.io/egg/egg/tutorials/_01_background/index.html). Base para busca de alternativas equivalentes, com antecedentes explicitamente reconhecidos pelo projeto.
- **[R5] CompCert:** [manual sobre preservação semântica e escopo de verificação](https://compcert.org/man/manual001.html). Referência metodológica; não cobre automaticamente dispositivos PS2 nem toda a cadeia proposta.
- **[R6] LLVM:** [Language Reference Manual](https://llvm.org/docs/LangRef.html). Semântica normativa da IR a respeitar ao usar esse backend.
- **[R7] PCSX2:** [repositório oficial](https://github.com/PCSX2/pcsx2). Implementação primária para estudo e comparação diferencial, sem atribuir-lhe infalibilidade.
- **[R8] PS2SDK:** [repositório oficial do SDK homebrew](https://github.com/ps2dev/ps2sdk). Referência de interfaces e fonte de programas de teste.
- **[R9] Vulkan:** [Synchronization and Cache Control](https://docs.vulkan.org/spec/latest/chapters/synchronization.html) e [Memory Model](https://docs.vulkan.org/spec/latest/appendices/memorymodel.html). Regras de dependência, disponibilidade e visibilidade de memória no backend.
- **[R10] Android:** [suporte a páginas de 16 KiB](https://developer.android.com/guide/practices/page-sizes). Requisito de portabilidade de aplicações com bibliotecas nativas.

Implementações devem fixar commits e versões dos materiais efetivamente utilizados. Links para branches atuais servem para consulta, não como identidade reproduzível de dependência.

## 36. Condições normativas de aceitação

Um pacote só recebe `ACCEPTED_STRICT` se todas as condições exigidas pelo contrato solicitado forem verdadeiras:

1. A identidade da entrada e do domínio está completa.
2. As entradas iniciais e sucessores possíveis estão cobertos pelo fechamento declarado.
3. Todo código EE, IOP e VU alcançável no domínio usa implementações AOT presentes no pacote.
4. Nenhum caminho alcançável introduz interpretação ou recompilação da ISA convidada no runtime final.
5. Mudanças de código, aliases e visibilidade de fetch respeitam o modelo.
6. Otimizações têm justificativa apropriada e reconstrução válida nos pontos observáveis.
7. Semântica, serviços e dispositivos não dependem de stubs que simulam aprovação.
8. Critérios de conteúdo, áudio, controles, persistência e plataforma passaram no escopo anunciado.
9. A avaliação de autonomia não contém intervenção ou artefato específico proibido pelo protocolo.
10. O desempenho exigido foi medido no alvo e no perfil temporal correspondente.
11. Obrigações abertas, dependências externas e limites não foram omitidos.
12. Os artefatos entregues correspondem aos que receberam as evidências e certificados.

Formalmente, para contrato \(c\), domínio \(d\), plataforma \(p\) e pacote \(a\):

\[
\operatorname{Accept}(a,c,d,p)=
\operatorname{Identity}(a)
\land\operatorname{Closed}(a,d)
\land\operatorname{NativeAOT}(a,d,p)
\land\operatorname{Semantics}(a,c,d)
\land\operatorname{Content}(a,c,d,p)
\land\operatorname{Autonomy}(a,c)
\land\operatorname{Performance}(a,c,p)
\land\operatorname{EvidenceBoundToArtifact}(a).
\]

Qualquer termo `unknown`, `unmeasured`, `bounded_only` fora do escopo exigido ou `failed` impede a aprovação correspondente. A taxa de 90–100% só é atingida quando essa conjunção passa na fração exigida do catálogo e nas plataformas declaradas.

`INVARIANTE FINAL: nenhuma hipótese, observação parcial ou ausência de contraprova pode ser promovida silenciosamente a garantia de equivalência, fechamento, autonomia, compatibilidade ou desempenho.`
<!-- PS2NATIVE_SOURCE_END:README.md -->

---

<a id="anexo-02"></a>

# ANEXO 02 — README_AUTOMACAO_UNIVERSAL.md

Origem: [README_AUTOMACAO_UNIVERSAL.md](README_AUTOMACAO_UNIVERSAL.md). Linhas originais: **2756**. Bytes: **146446**. SHA-256: `378861bf8d2ad2308eaf69b278073d0fe0e2ab4dbf51071fafecaf60da1ee793`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:README_AUTOMACAO_UNIVERSAL.md -->
# PS2 Native Recompiler — plano mestre de automação universal

> **Meta:** uma pessoa fornece sua ISO de PS2, escolhe PC e/ou Android e recebe um pacote nativo instalável, com preparação automática e evidências de compatibilidade.
>
> **Autonomia final:** o próprio sistema descobre, diagnostica, propõe reparos e valida títulos novos, sem exigir agentes externos ou edição humana por jogo. As correções manuais da fase inicial devem virar capacidades reutilizáveis. A meta de 90% é intermediária; a direção de expansão é toda a biblioteca PS2.
>
> **Ambição de cobertura:** alcançar pelo menos 90% de sucesso conjunto nos destinos definidos, dentro de um catálogo versionado e representativo. Esse número é uma meta de produto; ainda não é um resultado do projeto.
>
> **Estado deste arquivo:** plano de engenharia e pesquisa. As propostas abaixo não foram implementadas apenas por estarem documentadas.

**Auditoria:** 28–29 de setembro de 2026.\
**Repositório:** `/home/pedrohs/Downloads/ps2-native-recompiler`.\
**Origem:** [ran-j/PS2Recomp](https://github.com/ran-j/PS2Recomp).\
**Commit base local:** `75d729ce40d7eed9649fd4bb05628dee520f3d0c`, com alterações locais anteriores a este documento.\
**Primeiro jogo de referência:** Monster House, executável `SLUS_214.00`, revisão fornecida pelo usuário.\
**Destinos iniciais:** Linux x86-64, Windows x86-64 e Android ARM64.\
**Forma de execução deste planejamento:** análise individual; nenhuma nova delegação após a instrução de trabalhar sem subagentes.

Este documento concentra visão, auditoria, decisões, arquitetura, experimentos, tarefas e critérios de entrega. Ele amplia a visão de [PROJECT_SPEC.md](PROJECT_SPEC.md) para incluir processamento em nuvem e um destino final sem interpretação de instruções de jogo. Onde a especificação anterior admite fallback no produto, este plano o restringe ao laboratório de diagnóstico. A mudança de contrato ainda precisa ser refletida no software.

## Índice

1. [O produto e seu contrato](#s01)
2. [O que realmente existe](#s02)
3. [Como medir os 90%](#s03)
4. [Conhecimento e ferramentas a reaproveitar](#s04)
5. [Arquitetura geral](#s05)
6. [Artefatos, identidades e contratos](#s06)
7. [Entrada, disco e descoberta de código](#s07)
8. [Compilador e semântica do EE](#s08)
9. [IOP, VU e serviços do sistema](#s09)
10. [Gráficos, áudio, vídeo e tempo](#s10)
11. [Diagnóstico e primeiro port: Monster House](#s11)
12. [Técnicas experimentais propostas](#s12)
13. [Compilação rápida e uso correto da GPU](#s13)
14. [Motor autônomo de correção e pipeline de nuvem](#s14)
15. [Isolamento, dados e cadeia de distribuição](#s15)
16. [Pacotes para PC e Android](#s16)
17. [Validação de correção e jogo completo](#s17)
18. [Catálogo e crescimento da compatibilidade](#s18)
19. [Observabilidade e classificação de falhas](#s19)
20. [Marcos e ordem de execução](#s20)
21. [Backlog executável](#s21)
22. [Riscos previstos e respostas](#s22)
23. [Custos, capacidade e prazo](#s23)
24. [Organização do código e decisões de arquitetura](#s24)
25. [Comandos que existem hoje](#s25)
26. [Critérios finais e próximos passos](#s26)
27. [Fontes e glossário](#s27)
28. [Pesquisa avançada e experimentos de fronteira](#s28)
29. [Mais três ISOs depois de Monster House](#s29)
30. [Execução imediata e compromisso de qualidade](#s30)

<a id="s01"></a>
## 1. O produto e seu contrato

### 1.1 Experiência desejada

O usuário seleciona uma ISO, escolhe destinos e recebe um resultado compreensível:

1. A imagem é identificada, verificada e associada à revisão exata.
2. O sistema informa se aquela revisão já tem compatibilidade validada.
3. A preparação ocorre localmente ou em um ambiente privado de nuvem.
4. O pipeline descobre executáveis e módulos, traduz código, compila e valida.
5. São entregues um pacote para PC e/ou um pacote Android, além de controles e dados necessários.
6. O jogo instalado funciona offline, salvo funcionalidades que originalmente dependam de rede.
7. Uma revisão ainda não suportada recebe um diagnóstico reproduzível e não ganha o selo “jogo completo”.

A experiência pode ser simples mesmo quando o processamento interno é complexo. Perfis de compatibilidade podem ser selecionados automaticamente por identidade forte. O usuário final não deve editar TOML, informar endereços de funções nem corrigir C++.

### 1.2 O significado de “nativo”

O contrato final proposto é:

- Código do jogo para EE/R5900 convertido antecipadamente em código de máquina do host.
- Código específico do jogo para IOP convertido antecipadamente quando for necessário executá-lo.
- Microprogramas VU conhecidos convertidos antecipadamente; funções reconhecidas podem usar implementações nativas equivalentes verificadas.
- Serviços de sistema, memória, dispositivos, gráficos, áudio e sincronização implementados por uma camada nativa de compatibilidade.
- Nenhum interpretador genérico de instruções EE/IOP/VU ou JIT desses processadores no caminho de execução do pacote qualificado.
- Alvos desconhecidos e versões inesperadas de código produzem diagnóstico explícito; não ativam silenciosamente um emulador.

Essa camada continua reproduzindo comportamentos do PS2. Recompilar a CPU não elimina a necessidade de implementar GS, VIF, GIF, DMA, SPU2, CDVD e temporização.

**O protótipo atual não satisfaz esse contrato estrito:** seu EE usa C++ recompilado, mas o IOP possui um interpretador R3000A e há interpretação de microprogramas VU. Isso deve aparecer no relatório técnico até a migração estar concluída.

Durante o desenvolvimento, emuladores e interpretadores de referência são ferramentas importantes de observação e comparação. Executá-los no laboratório ou no builder não equivale a entregá-los como o mecanismo do jogo recompilado.

### 1.3 O significado de “descompilar tudo em C++”

A saída inicial deve ser **C++ semanticamente equivalente ao código de máquina identificado**, com estado explícito do processador e metadados de origem.

Não é necessário recuperar nomes originais de classes, comentários, tipos de alto nível ou a organização original do projeto. Uma ISO não contém necessariamente essas informações. Transformar essa recuperação em requisito atrasaria o caminho até o jogo executável.

Distinguir três produtos:

| Produto | Finalidade |
|---|---|
| C++ gerado | Compilar e preservar a execução do jogo |
| Relatório de análise | Explicar funções, módulos, relações e incertezas |
| Reimplementações de alto nível | Substituir componentes reconhecidos quando houver equivalência comprovada |

A ferramenta e as contribuições ao runtime podem ser abertas. A geração de C++ a partir de um jogo não atribui automaticamente uma nova licença ao conteúdo de entrada. Preservar a origem e a separação entre infraestrutura, perfis e artefatos privados.

### 1.4 Limites de promessa

“Qualquer ISO” é a direção de expansão da ferramenta. Não é possível afirmar hoje que toda imagem arbitrária será transformada automaticamente em um port completo.

Entradas inválidas, formatos ainda não suportados, executáveis compactados, código gerado durante a execução, periféricos específicos e dependências de serviços externos podem impedir uma entrega automática.

Para um catálogo conhecido, é possível desenvolver suporte progressivo, perfis automáticos e um ciclo de descoberta assistida. A cobertura observada precisa ser publicada com seu denominador, suas revisões e seus dispositivos.

### 1.5 Definição proposta de concluído

Uma revisão só recebe a classificação completa quando:

- Instala e inicia do zero nos destinos declarados.
- Passa por menus, gameplay, cenas, áudio e transições.
- Salva, fecha, reabre e carrega o progresso corretamente.
- Permite completar a campanha ou os modos centrais definidos para aquele jogo.
- Atende aos critérios de fidelidade e desempenho na matriz de hardware declarada.
- Não executa instruções de jogo por interpretação/JIT.
- Pode ser reconstruída por um pipeline limpo usando entradas e versões registradas.
- Não depende de edições manuais perdidas em `build/`.
- Tem evidências associadas ao hash do pacote entregue.

<a id="s02"></a>
## 2. O que realmente existe

### 2.1 Inventário auditado

| Componente | Evidência local | Situação real |
|---|---|---|
| Inspeção e extração de ISO | [tools/iso_inspect](tools/iso_inspect/README.md) | ISO9660/Joliet, BOOT2, hashes e inventário ELF; formatos especiais têm limites explícitos |
| Orquestração local | [pipeline.py](tools/ps2native/pipeline.py), [cli.py](tools/ps2native/cli.py) | Inspeciona, extrai, analisa, recompila e prepara pacote |
| Analisador | [ps2xAnalyzer/src](ps2xAnalyzer/src) | Símbolos, heurísticas, padrões, classificação e TOML |
| Recompilador EE | [ps2xRecomp/src/lib](ps2xRecomp/src/lib) | R5900 → C++, com relatórios e caminhos de instrução não tratada |
| Registro de módulos | [function_table_emitter.cpp](ps2xRecomp/src/lib/function_table_emitter.cpp), [ps2_runtime.cpp](ps2xRuntime/src/lib/ps2_runtime.cpp) | Tabelas por módulo, aliases e ativação associada ao carregamento |
| Descoberta de ELF secundário | [pipeline.py](tools/ps2native/pipeline.py) | Heurística baseada em ELF/MIPS III; não cobre todos os overlays ou IRX |
| Runtime | [ps2xRuntime/src/lib](ps2xRuntime/src/lib) | Memória, escalonamento, serviços, gráficos, áudio, VFS e integração IOP parciais |
| IOP | [ps2xIOP/README.md](ps2xIOP/README.md), [iop_cpu.cpp](ps2xIOP/src/emulator/core/iop_cpu.cpp) | Interpretador R3000A, kernel virtual, imports e serviços HLE |
| VU | [ps2_vu1_core.cpp](ps2xRuntime/src/lib/vu/ps2_vu1_core.cpp), [ps2_runtime.cpp](ps2xRuntime/src/lib/ps2_runtime.cpp) | Execução de microprogramas presente; AOT integral não demonstrado |
| GS | [gs_cpu_backend.cpp](ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp), [gs_backend.h](ps2xRuntime/include/runtime/gs/gs_backend.h) | Backend de rasterização em CPU e interface útil para evolução |
| Desktop | [template CMake](tools/ps2native/templates/desktop/CMakeLists.txt) | Executável do ABI do host; não há produto multiplataforma certificado |
| Android | [app/build.gradle](android/app/build.gradle), [Activity](android/app/src/main/java/com/ps2x/runner/Ps2PackageActivity.java) | Projeto ARM64 e preparação de assets; APK do jogo não validado |
| Controles de toque | [ps2_android_runtime.cpp](ps2xRuntime/src/lib/ps2_android_runtime.cpp) | O arquivo ainda contém TODO para overlay de controles |
| Testes | [ps2xTest](ps2xTest/CMakeLists.txt), [test_cli.py](tools/ps2native/tests/test_cli.py) | Existem suítes de componentes; não demonstram campanha completa |
| CI | [build.yml](.github/workflows/build.yml) | Builds/testes Linux e Windows; não é o serviço de conversão em nuvem |
| Serviço de nuvem | Árvore e workflows auditados | Não foi encontrado API/filas/storage/workers que implementem o produto descrito |
| Compatibilidade de Monster House | Logs privados da execução local | Boot parcial; nenhuma tela jogável comprovada |

A estrutura planejada `profiles/`, uma representação intermediária semântica própria e o serviço cloud não devem ser apresentados como componentes já implementados apenas porque aparecem em documentos anteriores.

### 2.2 Monster House: números com contexto

Workspace auditado:

```text
build/ps2native/
  monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/
    run-49a313b6d817/
```

Identidade do executável:

```text
boot: SLUS_214.00
entrypoint: 0x00100008
ELF SHA-256:
e279c66114a1c1e98c94f69048a2fa5dbd4479d2a031e512b0e7d630d4070b5d
```

O log mais recente de geração `logs/recompiler-vtable-batch.log` registra:

| Medida | Valor observado | Interpretação correta |
|---|---:|---|
| Funções descobertas/processadas | 7.224 | Resultado do analisador, não total verdadeiro conhecido do jogo |
| Funções recompiladas | 7.033 | Contador de processamento; não prova correção |
| Funções ligadas a stubs | 191 | É preciso auditar se cada handler implementa o comportamento necessário |
| Funções geradas | 7.234 | Não é percentual de jogo portado |
| Entradas adicionais | 402.358 | Pode haver grande expansão de pontos de retomada |
| Entradas de fallback indireto | 765.944 | Fallback de despacho/análise; não prova interpretação funcional de código desconhecido |
| Ocorrências de instruções não tratadas | 13.706 | Exigem deduplicação por endereço e classificação código/dados |
| Warnings | 2.280 | Pendências de análise não desaparecem porque o link terminou |
| `register_functions.cpp` | 78.809.054 bytes | Aproximadamente 78,8 MB decimais; gargalo concreto de compilação |

Há registros com palavras como `0xefefefef` e padrões que podem ser dados dentro de faixas classificadas como função. Isso sustenta uma **hipótese de erro de delimitação**, não a conclusão de que todas as 13.706 ocorrências sejam falsas. Também há endereços repetidos no log: não contar ocorrências como instruções únicas.

O `manifest.json` salvo contém contadores anteriores, incluindo 7.233 funções geradas e 401.678 entradas adicionais. O pacote foi alterado após a produção original desse manifest. Logo, **o manifest não é atualmente uma atestação fiel da última build**. Corrigir essa associação é uma prioridade.

O último teste de aproximadamente 25 segundos:

- Inicializou janela, áudio e o runtime.
- Recebeu o caminho da ISO.
- Registrou o carregamento do ELF e mensagens de reboot do IOP.
- Terminou seu log em `IOP syncing...`.
- Produziu captura preta/magenta.
- Foi encerrado pelo teste; não houve evidência de gameplay.

O renderer informado nesse teste foi `llvmpipe`, sob Xvfb. Ele não mede o desempenho da GPU física.

### 2.3 Diagnósticos anteriores que precisam de precisão

A última linha do log não identifica necessariamente a função em que o processo ficou preso.

Em particular, [SIF.cpp](ps2xRuntime/src/lib/Kernel/Stubs/SIF.cpp) implementa `sceSifSyncIop` retornando `1`. Antes de modificar esse retorno, é preciso capturar o PC convidado, a pilha nativa e o estado do escalonador. O travamento pode estar depois da mensagem, em uma chamada diferente ou em uma espera sem progresso.

A ausência de novas mensagens de `sceCdRead unresolved LBN` no teste curto também não demonstra que todo o CD está correto. O teste pode simplesmente não ter alcançado a mesma leitura.

### 2.4 Dívida concreta criada durante a exploração

1. A configuração de argumentos de Monster House foi aplicada em fonte gerada; deve virar perfil/hook persistente com identidade exata.
2. Uma correção de `isnan` foi reaplicada após geração; deve entrar no emissor ou helper apropriado.
3. O launcher do pacote conhece `PS2NATIVE_CD_IMAGE`, mas o gerador de launchers em `pipeline.py` ainda não reproduz essa conveniência.
4. Alternar IPO no mesmo diretório provocou recompilação ampla; houve reaproveitamento manual de objetos/timestamps. Esse histórico deve ser substituído por builds limpas e cache com identidade.
5. O registro de funções é enorme e repete bindings em estruturas distintas.
6. O pipeline desativa unity build e PCH mesmo existindo opções no runtime.
7. `_run()` captura stdout inteiro em memória e só grava depois do término; isso dificulta progresso e limites de log.
8. Um log antigo de smoke ocupa aproximadamente 689 MB. Repetição sem limite pode ocupar disco e mascarar o primeiro erro.
9. `status: complete` significa conclusão do pipeline de build; o próprio manifest mantém gameplay não verificado.
10. Android configura `release` com assinatura de debug. Não tratar esse artefato como release distribuível sem revisar assinatura.

Esses pontos são oportunidades de acelerar o trabalho com correção. Não justificam descartar o código existente.

<a id="s03"></a>
## 3. Como medir os 90%

### 3.1 Fixar o denominador

Criar um catálogo versionado `C` de revisões distintas, identificado por hashes. O catálogo deve incluir jogos de diferentes motores, anos, regiões, mídias, formatos de código e tipos de carga.

Para cada revisão `g` e destino `t`, definir:

```text
success(g, t) =
    preparação automática
    AND build reproduzível
    AND pacote instalável
    AND AOT estrito
    AND funcionamento completo no protocolo da revisão
    AND desempenho aprovado no hardware declarado
```

Publicar separadamente:

```text
PC_Linux = sucessos Linux / total do catálogo
PC_Windows = sucessos Windows / total do catálogo
Android = sucessos Android / total do catálogo

Conjunto_PC_Android =
    revisões aprovadas simultaneamente em todos os destinos obrigatórios
    / total do catálogo
```

A meta principal é `Conjunto_PC_Android >= 0,90`. Se o conjunto de destinos mudar, criar outra versão da métrica.

Um título que passou no PC e falhou no Android não entra no numerador conjunto. Um título não tentado permanece no denominador do catálogo congelado.

### 3.2 Separar compatibilidade de automação

Duas taxas respondem a perguntas diferentes:

- **Compatibilidade automática:** quantos jogos do catálogo conseguem a entrega completa, sem intervenção naquela submissão.
- **Novos títulos sem ajuste:** quantos jogos nunca usados no desenvolvimento funcionam sem criar um novo perfil ou corrigir a ferramenta.

Um perfil previamente produzido e selecionado automaticamente pode contar para experiência plug-and-play. Isso não demonstra que um título desconhecido seja resolvido sem engenharia adicional.

Registrar tempo de intervenção por título e por família de engine. Reduzir esse tempo é uma meta tão relevante quanto reduzir minutos de compilação.

### 3.3 Níveis de evidência

| Nível | Critério | Pode anunciar jogo completo? |
|---|---|---|
| E0 — inventariado | Disco e executáveis identificados | Não |
| E1 — compilado | Artefato nativo produzido | Não |
| E2 — inicializado | Boot e menu reproduzidos | Não |
| E3 — jogável | Gameplay representativo, controles e áudio | Não |
| E4 — completo no protocolo | Campanha/modos centrais, saves, transições e destinos aprovados | Sim, com escopo publicado |
| E5 — manutenção validada | Regressões, sessões prolongadas e atualizações aprovadas | Fortalece a entrega |

Nenhum teste finito demonstra todas as sequências possíveis de ações. “Completo” significa cumprir um protocolo público suficientemente exigente, com rotas, exceções e limitações explícitas.

### 3.4 Métricas técnicas auxiliares

- Cobertura de bytes de código classificados, com categorias separadas para dados e desconhecidos.
- Cobertura dinâmica de blocos/arestas nos cenários executados.
- Número de destinos indiretos sem resolução.
- Quantidade de epochs/versões de código conhecidas e desconhecidas.
- Instruções EE/IOP/VU interpretadas em builds de diagnóstico, separadamente.
- Stubs classificados como implementação completa, parcial, retorno artificial ou desconhecido.
- Divergências de registradores, memória, eventos, frames e áudio.
- Tempo de build frio, build incremental e consulta de cache.
- P50/P95 de duração por etapa; pico de RAM e disco.
- Tempo de frame, velocidade de simulação, latência de áudio e temperatura no celular.
- Taxa de sucesso sem intervenção em revisões mantidas fora do desenvolvimento.

Cobertura de execução alta pode significar repetir o menu muitas vezes. Por isso, publicar também cenas, transições e rotas visitadas. Uma proporção de 90% de instruções AOT não satisfaz o contrato de jogo completo AOT.

### 3.5 Avaliação sem favorecer o próprio resultado

Começar com corpus pequeno e estratificado, mas manter um conjunto separado de títulos/revisões que não orienta os patches.

A divisão deve ocorrer por família/engine quando possível. Colocar duas revisões quase idênticas em desenvolvimento e avaliação pode produzir uma taxa artificialmente alta.

Quando houver apenas amostragem, publicar intervalo de confiança e método de seleção. Uma amostra conveniente de sucessos não permite extrapolar para a biblioteca inteira.

<a id="s04"></a>
## 4. Conhecimento e ferramentas a reaproveitar

A estratégia é selecionar componentes que eliminem trabalho real. Integrar muitas ferramentas sobrepostas pode consumir mais tempo do que reutilizar um conjunto pequeno e confiável.

| Projeto/ferramenta | Uso proposto | Limite e decisão |
|---|---|---|
| [PS2Recomp](https://github.com/ran-j/PS2Recomp) | Base já integrada para análise, tradução e runtime | Preservar a base; corrigir lacunas observadas e manter rastreio do upstream |
| [PCSX2](https://github.com/PCSX2/pcsx2) | Referência diferencial, debugger e estudo de subsistemas maduros | Não embutir um emulador completo e classificar isso como port AOT |
| [Play!](https://github.com/jpd002/Play-) | Referência para HLE e operação em Android/ARM | Compatibilidade e interfaces diferem; reaproveitamento exige auditoria técnica e de licença |
| [ps2sdk](https://github.com/ps2dev/ps2sdk) | Programas sintéticos, contratos de bibliotecas e módulos de teste | SDK homebrew não implementa automaticamente todo comportamento de SDKs comerciais |
| [ps2autotests](https://github.com/unknownbrackets/ps2autotests) | Casos pequenos com resultados de hardware | Incorporar casos relevantes e registrar a origem da expectativa |
| [ps2tek](https://psi-rockin.github.io/ps2tek/) | Referência de hardware para formular e revisar hipóteses | Resolver ambiguidades por testes; documentação isolada não é certificado |
| [Rabbitizer](https://github.com/Decompollaborate/rabbitizer) | Decodificação R5900 já usada | Decoder identifica instruções; a semântica continua sendo responsabilidade do projeto |
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | Análise headless, mapas de funções, xrefs e investigação | Validar o processor/language e extensões PS2; pseudocódigo não é compilador correto por si só |
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) | Aprender organização de recompilação e overlays/relocações | N64 e PS2 têm diferenças de ISA, VU e hardware; não é backend substituto |
| [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) | Referência para hooks e otimizações sob contratos de ABI | PPC/Xbox 360 não é R5900; só transportar a ideia com validação |
| [angr](https://github.com/angr/angr) | Experimentos limitados de exploração e resolução de alvos | Não assumir semântica completa de R5900/MMI/VU; adaptador e custo podem inviabilizar |
| [Z3](https://github.com/Z3Prover/z3) | Equivalência de operações inteiras e precondições em blocos pequenos | Timeout é desconhecido; não prova jogo inteiro |
| [Alive2](https://github.com/AliveToolkit/alive2) | Verificar transformações LLVM em experiências futuras | Não verifica automaticamente o levantamento PS2 nem todo LTO interprocedural |
| [Clang ThinLTO](https://clang.llvm.org/docs/ThinLTO.html) e [LLVM DTLTO](https://www.llvm.org/docs/DTLTO.html) | LTO incremental e distribuição futura de trabalho de backend | Fixar versão de Clang/LLD e testar suporte do alvo antes da adoção |
| [sccache](https://github.com/mozilla/sccache) | Cache local/remoto de compilação; já existe integração opcional | Cache não acelera um miss por má identidade ou fonte sempre reescrita |
| [Bazel remote caching](https://docs.bazel.build/versions/main/remote-caching.html) | Referência para cache por ação e execução distribuída posterior | Manter CMake inicialmente; migração só com benefício medido |
| [Firecracker](https://github.com/firecracker-microvm/firecracker) | Opção de isolamento de jobs CPU em microVM | Avaliar KVM/host; GPU não deve ser pressuposta nessa arquitetura |
| [Android NDK e ferramentas de publicação](https://developer.android.com/studio/publish/app-signing) | Compilar, alinhar, assinar e verificar o pacote mobile | A disponibilidade da toolchain não prova fidelidade nem desempenho do jogo |

### 4.1 O exemplo de Simpsons: Hit & Run

O [repositório do port Android citado](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android) se apresenta como continuação de ports anteriores e pede os arquivos da **versão PC**. É uma referência útil de integração Android, controles e mídia.

Ele não demonstra um conversor genérico de ISO PS2. Usá-lo como referência de produto e comparar sua arquitetura é válido; assumir que resolve a descoberta e recompilação de qualquer binário PS2 seria uma inferência incorreta.

### 4.2 Ordem de preferência para reutilização

1. Corrigir e usar módulos já existentes e testados.
2. Estudar implementações maduras e reproduzir testes observáveis.
3. Integrar código compatível quando a fronteira técnica e sua licença estiverem claras.
4. Criar adaptadores pequenos ao redor de APIs estáveis.
5. Implementar algo novo somente onde os caminhos anteriores não resolvem o problema.
6. Registrar a alternativa descartada e a medição que motivou a escolha.

Reutilizar um GS maduro pode economizar muito trabalho, mas extrair um subsistema de um emulador pode exigir memória, DMA, threads e estado global desse projeto. Fazer um experimento de integração delimitado antes de assumir que é uma biblioteca pronta.

<a id="s05"></a>
## 5. Arquitetura geral

### 5.1 Fluxo de entrega

```mermaid
flowchart TD
    U[ISO fornecida pelo usuario] --> I[Identificacao e integridade]
    I --> C[Consulta de compatibilidade por revisao]
    C --> D[Inventario de disco e executaveis]
    D --> A[Analise estatica e classificacao]
    A --> E[EE IOP VU e contratos de servicos]
    E --> G[Geracao C++ e registros]
    G --> B[Build nativa por destino]
    B --> V[Validacao]
    V -->|Aprovado| P[Pacote privado e relatorio]
    V -->|Divergencia| R[Reprodutor e diagnostico]
    R --> A
    A -->|Alvos ou versoes desconhecidas| O[Laboratorio de observacao]
    O --> A
    C -->|Artefato compativel em cache privado| V
```

O laboratório pode usar execução de referência. O pacote final qualificado contém apenas o resultado AOT e os serviços nativos aceitos.

### 5.2 Separação de responsabilidades

| Camada | Responsabilidade | Evitar |
|---|---|---|
| Cliente | Seleção de ISO, destino, progresso e download | Pedir endereços internos ao usuário |
| API | Identidade, autorização, orçamento e jobs | Executar ferramentas pesadas dentro de requests HTTP |
| Orquestrador | Dependências, retries, cancelamento e publicação | Decidir correção de tradução por mensagem textual de sucesso |
| Builder | Análise, geração e compilação | Acesso amplo a contas, segredos ou diretórios do host |
| Laboratório | Replays, snapshots, comparação e reprodutores | Tratar referência única como verdade infalível |
| Runtime | Serviços PS2, despacho de AOT e adaptação ao host | Descoberta remota obrigatória durante o jogo |
| Catálogo | Revisões, perfis, estado e evidência | Marcar todos os discos de um serial como equivalentes |
| Assinatura | Assinar artefato aprovado | Disponibilizar chaves privadas ao processo que analisou a ISO |

### 5.3 Caminho curto e caminho longo

**Caminho curto, revisão já suportada:** verificar identidade, obter análise/perfil/objetos válidos do cache autorizado, construir o destino necessário, validar checks mínimos e entregar.

**Caminho longo, revisão desconhecida:** inventariar, analisar, executar laboratório com orçamento, identificar divergências e produzir diagnóstico. Esse caminho pode exigir trabalho de engenharia e não deve prometer entrega automática em minutos.

Um sucesso no caminho longo pode gerar um perfil reutilizável que torne as próximas submissões do mesmo título rápidas.

<a id="s06"></a>
## 6. Artefatos, identidades e contratos

### 6.1 Identidade forte

O hash da ISO é necessário, mas não basta como chave única de todos os caches.

```text
DiscIdentity:
  hash da imagem original
  formato e geometria de setores
  SYSTEM.CNF e caminho de boot
  serial e região inferida
  manifest de extents e hashes de arquivos

CodeIdentity:
  hash do módulo
  ISA e variante
  mapa de carga e relocações
  versão de código/epoch
  perfil semântico
  versão do analisador e decoder

BuildIdentity:
  CodeIdentity
  fontes geradas
  runtime ABI e hash do runtime
  compilador/linker e flags efetivas
  target triple e CPU baseline
  dependências e toolchain
  perfil de compatibilidade
```

ISO traduzida, modificada ou com outra ordem de setores recebe identidade própria. Duas ISOs com o mesmo ELF podem compartilhar determinadas análises, mas não necessariamente layout de disco, áudio, vídeo ou dados.

### 6.2 Artefatos por etapa

| Artefato proposto | Conteúdo |
|---|---|
| `disc-manifest.json` | Identidade, layout, arquivos, extents e dependências |
| `code-inventory.json` | EE, IOP, VU, candidatos, origem e confiança |
| `code-map.json` | Blocos, dados, alvos, limites, relocações e desconhecidos |
| `semantic-report.json` | Instruções únicas, semântica suportada e falhas |
| `module-registry.json` | Identidade e ativação das versões recompiladas |
| `build-manifest.json` | Entradas exatas, ferramentas, flags e resultados |
| `validation-report.json` | Rotas executadas, divergências e critérios |
| `compatibility.json` | Nível aprovado por destino e revisão |
| `provenance.json` | Relação entre pacote, build e evidências |
| `failure-bundle` | Reprodutor mínimo privado e contexto do primeiro erro |

Esses nomes descrevem contratos propostos. O pipeline atual possui `manifest.json`, `source_manifest.json`, `function_coverage.csv` e `address_coverage.csv`; a migração deve preservar compatibilidade com consumidores existentes.

### 6.3 Estados que não confundem build com jogo pronto

```text
submitted
  -> validating_input
  -> analyzing
  -> generating
  -> compiling
  -> build_complete
  -> validating_runtime
  -> playable_unverified_completion
  -> qualified_complete
  -> packaging
  -> delivered
```

Estados alternativos:

```text
unsupported_input
needs_analysis
semantic_gap
runtime_divergence
budget_exhausted
infrastructure_failed
cancelled
```

Armazenar o estado da build e a classificação do jogo em campos separados. `build_complete` pode coexistir com `semantic_gap` e compatibilidade não verificada.

### 6.4 Escrita e publicação

- Cada etapa recebe entradas imutáveis e escreve em um diretório temporário próprio.
- Só publicar o manifest de conclusão após hashes e outputs estarem persistidos.
- Jobs repetidos devem produzir efeitos idempotentes.
- Um worker antigo não pode sobrescrever o resultado de uma tentativa mais recente; usar lease e token de geração.
- O manifesto final registra o hash do binário realmente distribuído.
- Alteração manual de um artefato invalida sua atestação.
- Pacotes assinados podem variar por fatores da assinatura; distinguir reprodutibilidade do payload de reprodutibilidade byte a byte do pacote final.

### 6.5 Exemplo de relatório desejado

Exemplo de esquema futuro, não uma declaração de sucesso atual:

```json
{
  "schema_version": 2,
  "revision_id": "sha256:...",
  "target": "aarch64-linux-android",
  "build": {
    "status": "build_complete",
    "artifact_sha256": "..."
  },
  "execution_contract": {
    "mode": "strict_aot",
    "unknown_code_policy": "stop_with_report"
  },
  "validation": {
    "level": "E2",
    "campaign_complete": false,
    "unexpected_code_versions": 0,
    "evidence_id": "..."
  },
  "compatibility": {
    "status": "not_qualified"
  }
}
```


<a id="s07"></a>
## 7. Entrada, disco e descoberta de código

### 7.1 Preservar o disco como dispositivo

Extrair arquivos resolve caminhos por nome, mas não preserva sozinho o comportamento de leituras por setor.

O pipeline precisa manter:

- Imagem original ou representação equivalente dos setores.
- Relação entre LBN/LBA, extents, arquivos e regiões sem nome.
- Geometria e tamanho de setor usados pela API.
- Metadados de mídia e diferenças entre caminhos CD/DVD relevantes.
- Identidade de todos os arquivos e módulos referenciados.

A correção recente que aceita uma ISO no runner confirma a necessidade de distinguir `file_open(path)` de `read_sector(lbn, count)`.

No futuro, um pacote pode transportar um arquivo de dados indexado que preserve a visão de disco sem carregar uma cópia desnecessária de todo arquivo extraído. Isso exige demonstrar que todos os setores acessíveis estão representados. Remontar uma ISO com outra ordem de arquivos pode quebrar jogos que usam endereços fixos.

### 7.2 Formatos e classificação

Primeiro estabilizar o formato já suportado. Depois acrescentar, em etapas independentes:

1. Variações de ISO9660/Joliet e arquivos multi-extent.
2. Discos grandes e leituras próximas às bordas dos extents.
3. Mídias de múltiplos discos com troca explícita.
4. Contêineres comprimidos ou faixas brutas, por adaptadores específicos.
5. Arquivos internos compactados que possam conter executáveis ou microcódigo.

Uma extensão `.iso` não identifica o console nem garante consistência. O inspector deve produzir `unsupported_input` para layouts que não entende.

### 7.3 Inventário de executáveis

Para cada candidato, preservar:

- Offset e container de origem.
- Cabeçalho ELF, segmentos, seções, flags e símbolos disponíveis.
- Evidência de ISA: EE, IOP, VU ou desconhecida.
- Imports, exports, relocações e rotina de carregamento.
- Endereço de entrada e mapa de memória esperado.
- Possível compressão, relocação ou sobreposição de regiões.
- Grau de confiança e contradições da classificação.

`EM_MIPS` e flags isoladas não bastam para declarar que um módulo é EE. IRX e executáveis secundários precisam de classificação própria. Nomes de arquivos ajudam a investigar, mas não devem decidir o decoder.

### 7.4 Descoberta em camadas

Aplicar análise do mais confiável ao mais incerto:

1. Segmentos, símbolos e relocações explícitos.
2. Mapas externos verificáveis associados ao hash correto.
3. Alvos diretos de controle.
4. Propagação de constantes e análise de fluxo de valores.
5. Jump tables com limites e evidência de uso.
6. Tabelas de ponteiros, vtables e trampolins.
7. Funções próximas, thunks e prólogos conhecidos.
8. Observações de execução de referência.
9. Investigação manual das regiões restantes.

Cada conclusão deve ter origem. Um endereço “encontrado porque parece ponteiro” não tem a mesma força de um salto executado e capturado.

### 7.5 Separar código de dados

A representação precisa admitir:

```text
instruction
embedded_data
padding
vu_payload
relocation_data
mixed_region
unknown
```

Não usar o fim da próxima função como prova de que todos os bytes intermediários são instruções. Isso pode inflar o registro e gerar milhares de erros sobre texto, constantes e preenchimento.

Na auditoria de Monster House, a função estimada `sub_00355F80` aparece associada a erros muito adiante no binário. Uma tarefa prioritária é conferir seus limites contra fluxo real, xrefs e conteúdo. O objetivo não é silenciar o relatório; é torná-lo semanticamente correto.

### 7.6 Alvos indiretos e pontos de retomada

Distinguir:

- Entrada de função.
- Label interno.
- Retorno de chamada.
- Continuação após yield/interrupção.
- Destino de jump table.
- Entrada alternativa de uma função.
- Endereço inválido ou pertencente a outra versão do módulo.

Não mapear um PC para a “função mais próxima” sem garantir que o código gerado pode retomar exatamente naquele ponto.

A chave conceitual do despacho deve incluir:

```text
address_space + module_identity + load_epoch + guest_pc
```

A geração de centenas de milhares de aliases pode ser uma solução conservadora útil, mas precisa ser medida. Reduzir aliases exige preservar todos os pontos de retomada realmente necessários.

### 7.7 Overlays e código modificado

Implementar eventos explícitos de:

- Carga, relocação, ativação e descarga de módulo.
- Escrita em região executável.
- Publicação de uma nova versão executável.
- Invalidação de traduções ou bindings antigos.
- Mudança de mapeamento que altera a identidade do código.

Detectar writes em páginas ajuda, mas não é suficiente quando há alias, DMA ou cópia indireta. Instrumentar o caminho central de memória e carregadores; proteção de páginas do host é uma ferramenta complementar de diagnóstico.

Uma escrita que só muda constantes dentro do código pode, em casos delimitados, ser transformada em parâmetro da versão AOT. Isso exige análise da semântica exata. Uma alteração arbitrária de opcode precisa de outra versão recompilada ou permanece fora do suporte estrito.

### 7.8 Descoberta assistida por execução

O builder de diagnóstico pode:

1. Executar uma referência com entradas reproduzíveis.
2. Registrar destinos de salto e eventos de carga.
3. Capturar bytes de regiões executáveis após descompressão.
4. Associar cada captura a hash, origem, relocação e estado.
5. Recompilar novas regiões fora da execução do pacote final.
6. Repetir as rotas e comparar os resultados.

Esse método aproxima a análise de um ponto fixo observado. Não demonstra que todas as possibilidades futuras de código foram enumeradas. Rotas desconhecidas, cheats, conteúdo opcional e saves diferentes precisam continuar sendo considerados.

<a id="s08"></a>
## 8. Compilador e semântica do EE

### 8.1 Evoluir o emissor existente

Manter o caminho C++ atual como base. Evitar uma reescrita imediata para LLVM ou outro IR.

A evolução sugerida é:

```text
decoder
  -> instrução com metadados
  -> operação semântica explícita
  -> bloco com efeitos
  -> C++ conservador
  -> otimizações sob contratos
  -> experimento opcional com LLVM IR
```

O IR semântico inicial pode ser pequeno e interno. Ele deve resolver duplicação de semântica, rastreio de efeitos e validação; não precisa começar como um framework de compilador completo.

### 8.2 Modelo de estado

O estado convidado deve representar tudo que pode ser observado pelo jogo ou pelo runtime:

- Registradores inteiros e suas porções relevantes.
- HI/LO e bancos associados às operações específicas.
- PC, próximo PC, estado de delay slot e motivo de saída.
- Registradores de ponto flutuante, acumuladores e flags necessárias.
- Estado de COP0, exceções e interrupções modeladas.
- Ligações com VU0 e memória especial.
- Contadores de tempo/eventos usados na sincronização.

Eliminar campos ou convertê-los em variáveis locais só depois de demonstrar que nenhuma saída, callback, interrupção ou chamada observa o valor intermediário.

### 8.3 Checklist semântico por família

| Família | Risco a testar |
|---|---|
| Inteiros de 32/64 bits | Extensão de sinal, truncamento, preservação de lanes e registrador zero |
| Shifts | Máscaras de contagem e comportamento nas bordas |
| Multiplicação/divisão | HI/LO, overflow, divisão por zero e variantes |
| Loads/stores | Alinhamento, acessos parciais, aliases e endianness |
| Branches | Delay slot, branch likely, condições e destino efetivo |
| JAL/JALR/JR | Valor do link, entradas alternativas e retorno não estruturado |
| Exceções | Estado observável, contexto de delay slot e retomada |
| COP0/cache/sync | Interação com mapeamento, tempo, memória e publicação de código |
| COP1/FPU | Arredondamento, flags, valores extremos e diferenças em relação ao host |
| MMI/SIMD | Saturação, ordem das lanes, packing e operações especiais |
| COP2/VU0 | Intertravamento, transferência de registradores e execução macro/micro |
| Syscalls | Convenção de argumentos, retornos, erros e efeitos de memória |

O conjunto de instruções decodificáveis não equivale ao conjunto de semânticas corretas.

### 8.4 Evitar comportamento indefinido do C++

Gerar operações com largura e conversões explícitas:

- Aritmética modular via tipos unsigned apropriados.
- Extensão de sinal definida e testada.
- Máscaras de shift explícitas.
- Acesso a memória sem violações de alinhamento/aliasing do host.
- Tratamento explícito para divisões excepcionais.
- Conversões bit a bit para floats quando necessárias.
- Limites e efeitos MMIO fora de ponteiros diretos arbitrários.
- Proibição de otimizações que pressuponham propriedades que o convidado não oferece.

Sanitizers ajudam a encontrar UB do host, mas não provam semântica do PS2.

### 8.5 Ponto flutuante

Não assumir que SSE, NEON, `float` do C++ e PS2 produzem os mesmos bits para todas as entradas.

Criar um conjunto de vetores por operação, incluindo extremos e dependências de flags. Separar o modelo exigido pelo EE do modelo do VU. Quando houver diferenças, usar helpers de semântica ou sequências específicas do host.

O arquivo [ReleaseMode.cmake](ps2xRuntime/cmake/ReleaseMode.cmake) habilita `/fp:fast` no MSVC. Isso merece auditoria antes de promover a build Windows. Uma opção aceitável para apresentação gráfica pode ser incorreta no código que determina física e estado do jogo.

Evitar `fast-math` global. Qualquer relaxamento deve ter escopo, contrato e evidência de que não altera estados observáveis.

### 8.6 Fluxo não estruturado e escalonamento

Chamadas C++ simples funcionam para muitos casos, mas o convidado pode usar:

- Tail calls.
- Saltos para labels internos.
- Retornos manipulados.
- Threads com stacks próprias.
- setjmp/longjmp ou mecanismos equivalentes.
- Callbacks e interrupções.
- Mudança de módulo no mesmo endereço.

A implementação conservadora deve permitir blocos AOT com saídas explícitas, por exemplo `continue`, `call`, `return`, `yield` e `exception`. Um dispatcher entre blocos já compilados continua sendo AOT; ele não precisa decodificar instruções em tempo de jogo.

Otimizar trechos comprovadamente estruturados para chamadas nativas diretas. Preservar o caminho conservador para regiões complexas.

### 8.7 Um contrato mínimo de bloco

Pseudocódigo arquitetural:

```cpp
struct GuestBlockIdentity {
    uint64_t moduleId;
    uint64_t codeEpoch;
    uint32_t entryPc;
};

struct BlockExit {
    uint32_t nextPc;
    ExitReason reason;
    uint64_t guestCycles;
};

BlockExit executeCompiledBlock(
    GuestState& state,
    RuntimeServices& services);
```

Os nomes e tipos são propostas. A fronteira deve permitir medir efeitos e retomar execução sem depender da stack nativa como representação exclusiva do controle convidado.

### 8.8 Validação da tradução

Comparar:

```text
estado inicial + bytes convidados + eventos controlados
                    |
           execução de referência
                    |
                 estado A

estado inicial + bloco nativo correspondente + mesmos eventos
                    |
                 estado B

comparar A e B na fronteira semântica definida
```

Começar com blocos sem MMIO e aumentar complexidade. Quando há dispositivos, comparar também a sequência ordenada de efeitos, não apenas registradores finais.

Usar minimização automática para reduzir um erro a poucas instruções e entradas. Guardar o caso reduzido como teste de regressão.

### 8.9 Otimizações após correção

Ordem sugerida:

1. Remover geração redundante e aliases comprovadamente desnecessários.
2. Reduzir dependência de headers pesados.
3. Manter registradores locais entre fronteiras sem efeitos externos.
4. Resolver chamadas diretas com identidade estável.
5. Especializar acessos a RAM quando não podem ser MMIO.
6. Agrupar blocos quentes preservando pontos de saída.
7. Aplicar PGO em rotas diversas.
8. Avaliar LLVM IR direto somente com baseline diferencial estável.

Não usar ausência de execução de um bloco em um replay como prova de que ele pode ser apagado.

<a id="s09"></a>
## 9. IOP, VU e serviços do sistema

### 9.1 IOP: a lacuna entre o estado atual e AOT estrito

O componente existente executa IRX por interpretação, oferece um kernel virtual e combina servidores reais de módulos com serviços HLE genéricos.

Migrar sem perder esse conhecimento:

1. Usar o interpretador atual como uma das referências de regressão.
2. Separar carregamento/relocação de módulo da execução de instruções.
3. Criar um frontend R3000A próprio, sem reutilizar automaticamente regras R5900.
4. Recompilar módulos IRX conhecidos para funções/blocos nativos.
5. Manter imports e exports resolvidos por identidade e versão.
6. Preservar o comportamento de threads, semáforos, callbacks e interrupções.
7. Executar testes de RPC/DMA e áudio com implementações AOT.
8. Remover a dependência do interpretador no alvo qualificado.

Um IRX relocável não deve ser recompilado como se seus endereços fossem fixos. Modelar relocações e bases de carga, inclusive relações HI16/LO16 quando aplicáveis.

### 9.2 Serviços HLE

Bibliotecas identificadas podem ser substituídas por serviços nativos se o contrato for suficiente para os usos do jogo.

Cada serviço deve declarar:

- Identidade e versões reconhecidas.
- ABI de parâmetros e retorno.
- Memória lida/escrita.
- Sincronia ou assincronia.
- Eventos, callbacks e interrupções gerados.
- Códigos de erro.
- Dependências de estado e ordem de chamada.
- Casos suportados e casos explicitamente incompletos.

Um retorno constante que permite avançar no boot é um recurso de triagem, não uma implementação completa.

### 9.3 SIF/RPC

Preservar:

- Memórias EE e IOP como espaços distintos.
- Cópia real dos dados transmitidos.
- Disponibilidade e ciclo de vida de cada SID/servidor.
- Ordem de bind, envio, resposta e callback.
- Reboot/reset como transição de estado.
- Bloqueio e desbloqueio de threads.
- Cancelamentos e erros quando um módulo desaparece.

O runtime já tem testes úteis nessa fronteira. Expandir testes com situações de atraso e interleaving, e não apenas sucesso imediato.

### 9.4 VU0 e VU1

Separar ISA macro do EE de programas micro VU.

Plano AOT:

1. Inventariar microprogramas a partir de ELF, transferências e observação.
2. Registrar hash dos bytes, endereço de entrada e estado relevante.
3. Traduzir operações upper/lower com seus efeitos.
4. Representar dependências, flags, branches e latências necessárias.
5. Modelar relações com VIF e transferência ao GS.
6. Compilar CPU inicialmente para obter comparação precisa.
7. Avaliar especialização SIMD e GPU apenas em programas adequados.
8. Invalidar a seleção ao mudar o microcódigo.

Um programa VU pode ser reutilizado entre cenas com dados diferentes. A chave de código não deve incorporar dados irrelevantes, mas precisa incorporar tudo que muda a semântica.

### 9.5 Sincronização entre processadores

Uma conversão rápida pode continuar errada se executar subsistemas na ordem errada.

Adotar inicialmente um escalonador determinístico de eventos:

```text
tempo convidado
  -> execução EE até próxima fronteira
  -> progresso IOP
  -> progresso VU
  -> conclusão DMA
  -> interrupções e callbacks
  -> áudio/vídeo e vblank
```

A ordem exata depende do modelo validado. Não impor essa sequência textual como regra universal do hardware; usá-la como organização da fila de eventos.

Depois introduzir paralelismo do host com relações explícitas de dependência. “Mais threads” não deve mudar o resultado observável.

### 9.6 Estados que devem sobreviver a pausas

Para suspender/retomar corretamente:

- Registradores e PC de todos os contextos.
- Filas de eventos e deadlines convidados.
- Transferências DMA em andamento.
- Estado de RPC e callbacks.
- Microprograma ativo e estado VU.
- Áudio pendente e posição de reprodução.
- Estado de disco/streams.
- Memória de vídeo e caches deriváveis.
- Estado de input e threads.

Saves normais do jogo e snapshots internos são produtos distintos. Não depender de conversão de save states de outro emulador como mecanismo de progresso.

<a id="s10"></a>
## 10. Gráficos, áudio, vídeo e tempo

### 10.1 GS: melhorar uma fronteira existente

O projeto já separa frontend e backend de GS. Usar essa fronteira para criar testes de pacotes e replay gráfico.

Rota proposta:

1. Capturar lotes e estados do frontend.
2. Reproduzir no backend CPU atual.
3. Comparar com referência independente nos casos necessários.
4. Definir uma interface estável de memória, transferências e apresentação.
5. Desenvolver ou adaptar backend GPU.
6. Validar em GPUs de PC e de Android.

A escolha inicial sugerida para evolução é Vulkan, com um backend conservador de referência. A decisão deve passar por um protótipo medido; substituir o renderer inteiro antes do primeiro gameplay aumenta a quantidade de problemas simultâneos.

### 10.2 Casos gráficos que costumam invalidar uma tradução simplista

- Formatos de pixel e endereçamento de VRAM.
- Texturas paletizadas e mudanças de CLUT.
- Render target usado posteriormente como textura.
- Leitura e escrita sobrepostas.
- Blending dependente do destino.
- Alpha test, depth test, máscaras e condições de escrita.
- Regras de rasterização e coordenadas.
- Transfers host→local, local→host e local→local.
- Feedback e invalidação de caches.
- Interlace, resolução e apresentação.
- Sincronização de GIF paths e XGKICK.
- Ordem de comandos que parece redundante, mas produz efeito observável.

Copiar a imagem final para uma textura OpenGL não transforma uma rasterização CPU em renderização GS acelerada por GPU.

### 10.3 Reaproveitamento de renderers maduros

Comparar dois experimentos:

| Experimento | Benefício possível | Custo a medir |
|---|---|---|
| Backend novo atrás da interface atual | Menor acoplamento ao projeto externo | Mais implementação de comportamento gráfico |
| Adaptador para renderer maduro | Muitos casos especiais já conhecidos | Dependência de estado, memória e sincronização do projeto de origem |

Escolher pelo número de casos gráficos aprovados por semana de trabalho e pela manutenção da fronteira, não pelo tamanho do código importado.

### 10.4 Áudio e SPU2

Preservar:

- Vozes e envelopes.
- Pitch, loops e formatos comprimidos usados.
- Transfers de memória e DMA.
- Mixagem, volumes e efeitos relevantes.
- Streaming, buffers e notificações.
- Sincronização com vídeo e tempo convidado.

A validação deve medir underruns, continuidade e sincronização além de “saiu som”.

O backend do host deve manter o jogo sincronizado sem usar o callback de áudio para executar operações pesadas ou bloquear filas de jogo.

### 10.5 Vídeo, IPU e codecs

Monster House possui arquivos `.BIK` no pacote extraído. A existência de um handler MPEG e FFmpeg no desktop não prova suporte ao caminho real de reprodução desses arquivos.

Investigar como o jogo consome a mídia:

- Através de uma biblioteca reconhecida.
- Via código recompilado que decodifica os dados.
- Via IPU ou outro serviço.
- Com áudio separado ou intercalado.
- Com streams por setores.

Uma substituição por decoder nativo precisa preservar entrega de frames, callbacks, timestamps, formato de saída e comportamento de pausa/skip. O suporte FFmpeg desativado por padrão no Android é uma lacuna explícita de integração, não razão para remover cinematics da definição de completo.

### 10.6 Tempo convidado e tempo de parede

O tempo da simulação deve ser independente da velocidade de compilação, do escalonamento do sistema operacional e de quanto a GPU demorou a apresentar um frame.

Definir:

- Relógio convidado.
- Relógio de apresentação.
- Relógio de áudio.
- Política de atraso e recuperação.
- Comportamento em background e após suspensão.

Acelerar um busy loop pode mudar efeitos de temporização. A otimização de esperas exige identificar o evento que o encerra.

### 10.7 Desempenho no celular

Medir velocidade de simulação em relação à referência do jogo, não impor 60 FPS a todo título.

Proposta inicial de teste sustentado:

- Resolução interna documentada.
- Rota de pelo menos 30 minutos em gameplay representativo.
- Tempo de frame P50/P95/P99.
- Áudio sem interrupções persistentes.
- RAM, temperatura e throttling.
- Entrada e resposta visual.
- Reinício após pausa e troca de foco.

A meta numérica final depende do jogo e do aparelho de referência. Declarar um modelo mínimo de celular somente depois das medições.

<a id="s11"></a>
## 11. Diagnóstico e primeiro port: Monster House

### 11.1 Congelar uma base reproduzível

Antes de mais correções de comportamento:

1. Registrar hash do runner, fontes geradas, configuração e runtime.
2. Guardar o log atual como evidência histórica.
3. Migrar patches de argc/argv e geração FPU para fontes persistentes.
4. Incluir configuração de disco no pipeline, não só no launcher já gerado.
5. Criar build de diagnóstico em diretório separado, com flags fixas.
6. Regenerar e comparar as alterações esperadas.
7. Corrigir o manifest para descrever exatamente esse executável.

A verificação precisa começar de workspace limpo ou de cache verificável. Timestamps forçados não são identidade de build.

### 11.2 Descobrir o bloqueio atual

O próximo experimento deve responder onde a execução está e quem deveria fazê-la avançar.

Capturar, com orçamento de tempo:

- PC convidado e últimos blocos executados.
- Registradores relevantes e endereço de retorno.
- Pilhas nativas das threads.
- Estado/runnable threads do EE.
- Contadores de ciclo/instrução do IOP.
- Módulos carregados, imports não resolvidos e servidores RPC.
- Transfers SIF/DMA pendentes.
- Últimos acessos a CD e erro de leitura, se houver.
- Contadores de progresso do renderer e áudio.

Classificar o resultado:

| Sinal | Hipótese inicial | Próxima verificação |
|---|---|---|
| PC repete sem eventos | Busy wait ou condição nunca atualizada | Identificar produtor da flag e ordem de memória |
| Thread EE bloqueada | Evento/RPC/IOP não conclui | Traçar fila e wakeup esperado |
| Pilha nativa cresce | Recursão por tradução de salto/retorno | Conferir CFG e links convidados |
| IOP não avança | Falta de accounting/scheduling ou módulo | Comparar ciclo de vida e chamadas de progresso |
| Retorna de sync, trava depois | Último log não localiza o erro | Breakpoints no retorno e chamadas seguintes |
| PC em região ambígua | Descoberta/registro incorreto | Comparar bytes, epoch e mapa de código |
| Ausência de flush de log | Observação incompleta | Coleta estruturada e buffer circular |

Nenhuma dessas hipóteses está confirmada pela captura preta/magenta.

### 11.3 Corrigir com evidência

Para cada bloqueio:

1. Criar um reprodutor pequeno ou snapshot privado.
2. Observar o comportamento de referência.
3. Formular a correção no componente responsável.
4. Executar o teste reduzido.
5. Reexecutar do boot frio.
6. Confirmar que a execução alcança um marco novo sem invalidar os anteriores.

Evitar adotar `ret1` ou ignorar leitura como solução permanente apenas para avançar a tela.

### 11.4 Marcos próprios do jogo

- MH0: build limpa e identidades fiéis.
- MH1: reboot/serviços IOP com progresso demonstrado.
- MH2: entrada e carregamento inicial completos.
- MH3: primeira apresentação legítima de jogo, logo ou menu.
- MH4: menu controlável, áudio e início de partida.
- MH5: primeira área jogável com eventos e transição.
- MH6: save/load e pausa/retomada.
- MH7: campanha completa no PC, incluindo cenas relevantes.
- MH8: mesmo protocolo no Android físico.
- MH9: execução estritamente AOT nos subsistemas de código exigidos.
- MH10: submissão limpa da ISO produz o pacote aprovado sem edição manual.

É aceitável usar o IOP/VU atual para investigar MH1–MH8. A classificação final do projeto continua pendente até MH9; o uso de interpretação deve permanecer visível.

### 11.5 Perfis específicos e correções gerais

Uma correção de semântica ou serviço entra no runtime/compilador e ganha regressão geral.

Uma configuração própria dessa revisão, como argumentos de inicialização, entra num perfil identificado por hash. O mecanismo existente usa nome, entrypoint e CRC32; ampliar para SHA-256 e precondições de bytes.

Cada hook deve explicar:

- O comportamento necessário.
- A evidência.
- O hash/revisão a que se aplica.
- O ponto exato de aplicação.
- Como detectar que o hook deixou de ser necessário.

Não transferir endereços de Monster House automaticamente para outro jogo ou tradução.

<a id="s12"></a>
## 12. Técnicas experimentais propostas

Estas são propostas para este projeto, baseadas em combinações de ideias conhecidas de compilação, análise e testes. Não foi feita uma busca de anterioridade capaz de afirmar que são invenções inéditas no mundo.

Cada técnica precisa competir com uma alternativa simples. Se não melhorar correção, cobertura ou custo, deve ser descartada.

### 12.1 Descoberta AOT orientada por evidência

**Hipótese:** juntar análise estática e alvos observados reduz entradas ausentes sem compilar todo byte como código.

**Implementação inicial:**

- Cada bloco recebe evidências de origem.
- Um replay registra destinos e versões desconhecidas.
- O builder acrescenta somente regiões justificadas.
- A geração é incremental e preserva o histórico das hipóteses.
- Um diagnóstico distingue “não observado” de “provado inalcançável”.

**Experimento:** comparar com a estratégia atual em Monster House e fixtures com jump tables, thunks e dados intercalados.

**Métrica:** alvos ausentes, bytes gerados, tamanho da tabela, custo de compilação e divergências.

**Descartar se:** exigir percorrer manualmente todo o jogo sem reduzir o trabalho de análise ou perder destinos existentes.

### 12.2 Catálogo de versões de código materializadas

**Hipótese:** muitos jogos que parecem dinâmicos usam um conjunto finito de overlays ou código descompactado, que pode ser capturado e recompilado antes da entrega.

**Mecanismo:** registrar `hash do código + entrada + relocações + epoch` e selecionar a versão AOT quando a mesma região for materializada.

**Experimento:** fixture com duas versões no mesmo endereço, seguida de um módulo real que se comporte assim.

**Critério:** alternância correta entre versões, nenhuma execução com binding antigo e pacote final sem compilação dinâmica.

**Limite:** conteúdo arbitrário gerado conforme entrada pode produzir versões não enumeráveis na prática. Guardar hashes de alguns exemplos não resolve esse caso.

### 12.3 Biblioteca de assinaturas semânticas com validação

**Hipótese:** bibliotecas compartilhadas entre títulos permitem resolver imports e substituir rotinas com menos trabalho.

**Mecanismo:**

- Normalizar operandos relocáveis e estrutura de CFG.
- Usar assinatura como geradora de candidatos.
- Confirmar com ABI, efeitos e testes.
- Aplicar HLE apenas quando o contrato reconhecido é suficiente.
- Propagar perfis com identidade e versão.

**Métrica:** correspondências confirmadas, falsos positivos e quantidade de títulos beneficiados.

**Descartar se:** o ganho depende de aceitar correspondências aproximadas sem validação. Um falso positivo pode corromper um save muito depois do boot.

### 12.4 Comparação pela primeira divergência causal

**Hipótese:** encontrar o primeiro evento diferente é mais produtivo do que analisar a tela errada no fim.

**Mecanismo:** checkpoints de estado e uma sequência ordenada de eventos permitem localizar o intervalo da primeira divergência; uma segunda execução coleta detalhes apenas nessa janela.

**Experimento:** injetar erros conhecidos de branch, DMA e retorno RPC em fixtures.

**Critério:** diagnóstico aponta a região responsável antes que o erro se propague por milhares de frames.

**Limite:** relógios, callbacks e fontes de não determinismo precisam ser normalizados.

### 12.5 Priorização de exploração por novidade útil

**Hipótese:** replays escolhidos para visitar novos módulos, transições e serviços descobrem mais incompatibilidades do que tempo equivalente em inputs aleatórios.

**Mecanismo:** pontuar estados por novidade de código, transfers, SID, microprograma, cena e transição; escolher a próxima rota por ganho esperado e custo.

**Experimento:** mesmo orçamento em replay guiado, aleatório e sequência fixa.

**Critério:** mais falhas distintas e estados relevantes encontrados por hora.

**Limite:** não supor que isso zera automaticamente qualquer jogo; puzzles, combate e dependências longas podem exigir rotas humanas.

### 12.6 ABI estável entre jogo gerado e runtime

**Hipótese:** separar código do jogo e implementação de serviços reduz rebuilds gigantes a relinks pequenos.

**Mecanismo:** estado e interface versionados, biblioteca de serviços e callbacks com layout estável; nos builds de desenvolvimento, avaliar biblioteca compartilhada para o runtime.

**Experimento:** alterar um handler SIF e comparar build incremental antes/depois.

**Critério:** não recompilar milhares de funções do jogo quando uma implementação interna de serviço muda.

**Limite:** mudanças de layout ou semântica do ABI invalidam objetos; a chave de cache deve capturá-las. O custo de indireção também precisa ser medido.

### 12.7 Registro compacto com dados e sharding determinístico

**Hipótese:** a tabela textual de aproximadamente 79 MB pode ser substituída por representação menor e mais barata de compilar.

**Alternativas:**

- Arrays constantes de pares endereço/função.
- Índice paginado por endereço.
- Intervalos apenas quando todos os pontos do intervalo têm a mesma semântica de retomada.
- Separação de bindings por grupos determinísticos.
- Tabela principal de função mais tabela de labels internos.

**Experimento:** comparar igualdade dos bindings, PCs de borda, módulos sobrepostos, lookup, startup, tamanho de objeto e tempo de link.

**Critério:** mesma execução e queda medida no custo total.

**Limite:** agrupar endereços incorretamente pode fazer um salto para o meio de uma função reiniciar sua entrada.

### 12.8 Especialização verificada de microprogramas VU

**Hipótese:** microprogramas recorrentes podem gerar kernels nativos rápidos com interfaces estáveis.

**Mecanismo:** identificar o programa, determinar entradas/saídas e compilar uma especialização CPU/SIMD. Candidatos GPU só entram quando o volume de trabalho cobre custos de transferência e sincronização.

**Experimento:** comparar estados completos antes/depois em dados adversariais e capturas reais.

**Critério:** equivalência suficiente ao contrato e aceleração sustentada em aparelho real.

**Limite:** flags, latências e efeitos sobre o resto do sistema podem impedir a transformação em shader independente.

### 12.9 Perfis que carregam sua própria evidência

**Hipótese:** um perfil deixa de ser uma coleção de hacks quando inclui precondições verificáveis e teste de regressão.

**Conteúdo:** hash, bytes esperados, causa, efeito, fixture e critério de aplicação/remoção.

**Experimento:** testar revisão correta, revisão errada e revisão com patch parcial.

**Critério:** nunca aplicar silenciosamente em executável incompatível; rejeição informa qual precondição falhou.

### 12.10 Planejador de build por custo observado

**Hipótese:** agrupar unidades pelo custo previsto é melhor do que agrupar sempre 32 funções.

**Mecanismo:** medir duração e RAM por unidade; formar shards por custo, manter nomes estáveis e isolar outliers.

**Experimento:** mesmos fontes e hardware, com arquivos individuais, unity fixo e shards por custo.

**Critério:** reduzir duração e memória de pico sem destruir taxa de acerto do cache incremental.

**Limite:** reorganizar todos os shards a cada alteração pode custar mais que a otimização. Usar particionamento estável com correções locais.

### 12.11 Substituição de serviços guiada por efeitos

**Hipótese:** é possível reconhecer que uma rotina realiza um serviço conhecido sem depender apenas de seu nome ou bytes exatos.

**Mecanismo:** comparar traces de chamadas, memória e eventos com um contrato de serviço; gerar candidato HLE; validar em múltiplas revisões.

**Experimento:** rotinas de cópia, acesso a disco e inicialização controlada, começando pelas mais simples.

**Critério:** mesmas saídas e efeitos sob casos normais e falhas.

**Limite:** traces finitos não provam equivalência geral. Não aceitar contratos parciais como prova para funções complexas de gameplay.

### 12.12 Reparo assistido por modelos com confirmação executável

**Hipótese:** um modelo pode acelerar triagem e propor patches úteis quando recebe um reprodutor pequeno e contratos explícitos.

**Uso:** resumir divergências, sugerir classificação de dados, escrever candidatos e explicar dependências.

**Confirmação:** compilar, executar fixture, comparar referência e reexecutar o cenário do jogo. Candidatos rejeitados ficam registrados para evitar repetição.

**Limites:** o modelo não escolhe sozinho o resultado esperado do teste, não valida seu próprio patch apenas por texto e não publica pacotes. Não enviar ISOs ou código privado para serviços externos implicitamente.

### 12.13 Orçamento de pesquisa

Reservar inicialmente a maior parte do esforço para o caminho crítico comprovado. Uma proposta de divisão é 80% em correção/integração e 20% em experimentos delimitados.

Cada experimento deve produzir uma decisão, incluindo “não vale a pena”. Uma técnica sofisticada sem resultado mensurável não deve bloquear Monster House.

<a id="s13"></a>
## 13. Compilação rápida e uso correto da GPU

### 13.1 Medir o que está demorando

Instrumentar:

```text
inspeção
extração
análise
geração
configuração
compilação por unidade
arquivamento
link e LTO
empacotamento
validação
upload/download
```

Registrar duração, CPU, RSS máximo, bytes lidos/escritos e cache hits por etapa. O percentual que CMake mostra não representa tempo restante proporcional.

### 13.2 Três perfis de build

| Perfil proposto | Uso | Política |
|---|---|---|
| Diagnóstico | Corrigir bloqueios e obter stack/PC | Símbolos, otimização controlada, LTO desligado |
| Iteração rápida | Replays repetidos | Otimização moderada, cache, shards estáveis |
| Release | Desempenho e distribuição | Otimização validada, PGO/ThinLTO quando compensar |

Usar diretórios separados. A opção atual `PS2X_ENABLE_RELEASE_IPO` ajuda, mas precisa ser exposta coerentemente pelo pipeline e auditada nos geradores de build de múltiplas configurações.

Nunca reutilizar um objeto só porque o timestamp parece recente. Reutilizar apenas quando entradas e flags compatíveis estiverem demonstradas.

### 13.3 Prioridades imediatas

1. Parar de reescrever fontes cujo conteúdo não mudou.
2. Remover paths absolutos e ruído não necessário das saídas cacheáveis.
3. Preservar headers estáveis.
4. Dividir o registro gigante e validar igualdade.
5. Testar unity em lotes pequenos nas funções geradas, isolando registradores e unidades especiais.
6. Comparar PCH com headers mínimos; não assumir que ambos sempre ajudam.
7. Ativar cache com chave correta e métricas.
8. Separar objetos do runtime dos objetos do jogo.
9. Limitar paralelismo pelo orçamento de RAM, não apenas pelos núcleos.
10. Rodar LTO caro só depois que o cenário de correção passou.

A implementação atual já exclui `register_functions.cpp` do LTO e do unity em determinados builds GNU/Clang. Evoluir a partir dessa medida existente.

### 13.4 Paralelismo CPU e distribuição

Compilações de unidades independentes podem ser distribuídas. Dependências, configuração, thin link e link final continuam impondo etapas sequenciais.

Estimativa de paralelismo seguro:

```text
workers_compilacao <= min(
  capacidade CPU útil,
  RAM disponível / pico observado por unidade,
  capacidade de I/O,
  limite de custo
)
```

Usar uma margem de memória. Shards muito grandes podem reduzir o número de jobs e causar picos.

Para LLVM, [ThinLTO](https://clang.llvm.org/docs/ThinLTO.html) oferece um modelo incremental; [DTLTO](https://www.llvm.org/docs/DTLTO.html) permite distribuir backends sob condições específicas. Fixar Clang e LLD compatíveis e medir a troca antes de migrar do GCC.

### 13.5 O papel da GPU

| Trabalho | GPU é boa candidata? | Razão |
|---|---|---|
| GCC/Clang compilando C++ genérico | Não neste pipeline | As etapas atuais são executadas pela CPU |
| Renderização GS | Sim | Há trabalho gráfico massivamente paralelo, condicionado à semântica |
| Comparação de muitos frames | Possivelmente | Operações de imagem em lote |
| Execução de modelos locais | Possivelmente | Depende do modelo e do custo |
| Busca de assinaturas em lotes grandes | Experimento | Transferências e preparação podem dominar |
| Microprogramas VU independentes e grandes | Experimento | Sincronização e dependências podem anular ganhos |
| Uma espera sequencial de IOP/RPC | Geralmente não | O gargalo pode ser comportamento incorreto |

Acelerar a compilação com GPU exigiria construir ou integrar outro sistema de compilação especializado. Não é o caminho mais curto para este projeto.

### 13.6 Otimização por custo total

Uma build rápida que entrega código errado aumenta o custo de depuração. Uma build extremamente otimizada que leva muito tempo reduz a quantidade de hipóteses testadas por dia.

A métrica principal de produtividade deve ser:

```text
tempo até confirmar ou rejeitar uma correção
```

Medir também custo por revisão compatível nova. Isso impede que otimizações de infraestrutura escondam estagnação de compatibilidade.


<a id="s14"></a>
## 14. Motor autônomo de correção e pipeline de nuvem

### 14.1 A meta final é resolver títulos novos

O objetivo final não é manter uma equipe editando cada jogo. É construir um programa que faça descoberta, tradução, diagnóstico, correção e validação automaticamente a partir de qualquer ISO **de PS2** apresentada, dentro dos formatos e comportamentos que consiga reconhecer ou aprender a tratar.

A meta de 90% é um marco intermediário de cobertura. A direção de expansão continua sendo a biblioteca inteira. Autonomia e cobertura são medidas separadas: o sistema pode operar sem intervenção e ainda encontrar um caso que não consegue resolver.

O critério central para títulos inéditos será:

```text
ISO nunca usada para ajustar a ferramenta
  -> descoberta automática
  -> falha reproduzida e classificada, se ocorrer
  -> reparo automático quando houver solução verificável
  -> validação independente
  -> build nativa
  -> pacote qualificado

intervenções humanas nessa submissão = 0
```

Perfis já conhecidos aceleram o sistema, mas um catálogo de patches manuais não satisfaz sozinho essa meta. O próprio pipeline deve poder gerar novos metadados e perfis a partir de evidências.

### 14.2 Como transformar o trabalho inicial em capacidades

Cada correção feita agora deve deixar quatro produtos:

1. **Diagnóstico identificável:** quais sinais detectam a classe do problema.
2. **Transformação reutilizável:** qual regra, análise ou implementação resolve essa classe.
3. **Verificação independente:** como demonstrar que o resultado é correto.
4. **Caso de regressão:** como impedir o reaparecimento do problema.

Exemplos:

| Problema inicial | Capacidade que deve permanecer |
|---|---|
| Ponteiro para função não registrada | Recuperação de alvos indiretos e registro com evidência |
| Dados tratados como instruções | Refinamento de limites e mapa código/dados |
| ELF carregado durante o jogo | Descoberta e ativação de módulos por identidade/epoch |
| Argumentos de boot incompletos | Contrato de inicialização e perfil verificado |
| Leitura por LBN falha | Visão de disco por setores e diagnóstico de extents |
| Instrução traduzida incorretamente | Helper semântico correto e teste diferencial |
| RPC não conclui | Modelo de ciclo de vida, scheduling e transações |
| Link demora demais | Particionamento estável, cache e separação de ABI |

Uma correção inevitavelmente específica de revisão pode permanecer em perfil. O sistema precisa reconhecer quando aplicá-la e, na meta avançada, conseguir derivar equivalentes em novas revisões. Nunca transportar um endereço fixo sem conferir sua identidade.

### 14.3 Componentes do motor de reparo

| Componente proposto | Responsabilidade |
|---|---|
| Coletor | Capturar primeiro erro, estado convidado, eventos e versões |
| Classificador | Distinguir falha de build, descoberta, semântica, runtime, dispositivo ou infraestrutura |
| Redutor | Produzir o menor cenário que ainda reproduz o erro |
| Planejador | Selecionar transformações compatíveis com as evidências |
| Gerador de candidatos | Aplicar regras, síntese limitada ou propostas de modelo |
| Executor isolado | Compilar e testar cada candidato |
| Verificador | Comparar com referência e invariantes que o candidato não controla |
| Gestor de regressão | Reexecutar casos que podem ser afetados |
| Promotor | Publicar perfil/artefato elegível com proveniência |
| Memória de resultados | Evitar tentativas repetidas e reutilizar soluções validadas |

Começar com regras determinísticas para as falhas frequentes. Modelos são auxiliares opcionais para propor candidatos complexos; a operação do serviço não depende de alguém abrir um chat e editar o projeto.

### 14.4 Ciclo automático

```text
1. Executar cenário com orçamento.
2. Detectar falha ou ausência de progresso.
3. Capturar estado e criar assinatura do incidente.
4. Reproduzir; se não reproduzir, classificar como não determinístico.
5. Reduzir o cenário e preservar resultado de referência.
6. Consultar reparos já validados para aquela classe.
7. Gerar candidatos pequenos, cada um com precondições.
8. Compilar somente o necessário.
9. Comparar estados, efeitos e invariantes.
10. Executar regressões locais e de títulos afetados.
11. Aplicar o candidato aprovado numa revisão isolada.
12. Voltar ao cenário original e ao boot frio.
13. Repetir até cumprir o protocolo ou esgotar o orçamento.
14. Entregar artefato aprovado ou relatório inconclusivo.
```

Um timeout, o silêncio do log ou a remoção de uma exceção não são critérios de aprovação.

### 14.5 Hierarquia de reparos

Aplicar primeiro os reparos de menor risco e maior poder de verificação:

1. Corrigir caminhos, aliases e metadados com base no disco.
2. Corrigir classificação, limites e entrypoints com evidência.
3. Selecionar versão conhecida de módulo/microprograma.
4. Aplicar contrato HLE já validado e reconhecido.
5. Selecionar implementação semântica testada para uma instrução.
6. Inferir parâmetros de um modelo conhecido.
7. Sintetizar transformação pequena e verificar equivalência.
8. Propor alteração nova no compilador/runtime, executar bateria ampliada e mantê-la isolada até aprovação automática suficiente.

Não promover uma alteração global porque ela fez um único jogo avançar. Correções de escopo incerto podem ficar restritas à revisão, com motivo e precondições, até haver evidência para generalização.

### 14.6 O verificador não pode ser controlado pelo reparo

Separar as superfícies de escrita:

- O candidato altera a implementação ou metadados explicitamente permitidos.
- Referências, resultados esperados e critérios de sucesso são somente leitura para esse candidato.
- Mudanças em dados de teste exigem outro fluxo de validação, com origem independente.
- O motor não pode reduzir cobertura, aumentar tolerância ou desligar um teste para fazer sua própria proposta passar.
- A aprovação associa a evidência ao hash exato da implementação testada.

Manter casos reservados e perturbações de entrada. Um patch que “decora” o replay observado precisa falhar em entradas vizinhas.

### 14.7 Memória de reparos e aprendizado acumulado

Armazenar:

```text
assinatura da falha
precondições
classe de transformação
versão das ferramentas
candidato e hash
casos positivos e negativos
resultado diferencial
regressões executadas
títulos/revisões afetados
custo e duração
motivo de aprovação ou rejeição
```

Compartilhar regras e conhecimento de infraestrutura conforme sua licença. Manter bytes de jogo, snapshots, saves e artefatos derivados em escopo privado.

Usar similaridade apenas para sugerir. A aplicação depende de precondições verificadas.

### 14.8 Contenção de loops e erros novos

- Número máximo de candidatos e ciclos por incidente.
- Orçamento de CPU, memória, tempo e armazenamento.
- Assinatura de tentativas já feitas.
- Rollback automático quando aparecer regressão.
- Snapshot antes de cada alteração.
- Promoção atômica; nunca deixar metade de um patch ativa.
- Cancelamento que encerra subprocessos e descendentes.
- Quarentena para artefatos inconsistentes.
- Retomada por checkpoint após falha de infraestrutura.

Esgotar o orçamento significa `budget_exhausted` ou `needs_analysis`. Esse caso não entra nos sucessos dos 90%. A meta é reduzir esses resultados com a evolução da ferramenta.

### 14.9 A dificuldade que continua sendo pesquisa

Não existe aqui uma demonstração de que reparo automático consegue resolver todo programa possível ou certificar ausência absoluta de bugs. O projeto deve buscar cobertura crescente e evidência forte sem transformar a meta em uma garantia matemática.

Especialmente difíceis:

- Código gerado em quantidade ou forma imprevisível.
- Referência que também falha.
- Diferenças que só aparecem depois de horas.
- Gameplay que o explorador não consegue completar.
- Correções que passam numa rota e falham em outra.
- Hardware/periféricos sem modelo suficiente.

Esses problemas precisam aparecer nos resultados. Ocultá-los produz uma ferramenta que aparenta sucesso e entrega ports quebrados.

### 14.10 Nuvem mínima antes de uma plataforma grande

Usar a CLI atual como executor de etapas. Acrescentar inicialmente:

- API pequena de submissão e consulta.
- PostgreSQL para jobs, tentativas, leases e resultados.
- Armazenamento de objetos privado para entradas e artefatos.
- Workers CPU descartáveis.
- Worker de laboratório separado.
- Serviço de assinatura separado.
- Canal de progresso e logs com limites.

[PostgreSQL](https://www.postgresql.org/docs/current/sql-select.html) oferece operações de locking úteis para filas; a proposta é usá-las com leases, idempotência e fencing. Isso não fornece “execução exatamente uma vez” automaticamente.

Não iniciar com Kubernetes, múltiplas filas e vários orquestradores simultaneamente. Adotar complexidade adicional quando capacidade, confiabilidade ou isolamento exigirem.

### 14.11 Grafo de tarefas

```text
validar input -> inventariar -> analisar -> gerar código
                                         |
                    +--------------------+---------------------+
                    |                    |                     |
               Linux x86-64        Windows x86-64        Android ARM64
                    |                    |                     |
               validar alvo         validar alvo          validar alvo
                    +--------------------+---------------------+
                                         |
                               qualificar e empacotar
                                         |
                                   assinar/entregar
```

A análise independente do host pode ser compartilhada. Objetos nativos, ABI, flags, runtime e resultados de dispositivos permanecem específicos do destino.

O laboratório de reparo pode devolver novas entradas para o grafo. Cada iteração usa nova identidade; resultados antigos não devem ser confundidos com a versão corrigida.

### 14.12 API proposta

Estas rotas ainda não existem:

| Rota | Efeito |
|---|---|
| `POST /v1/uploads` | Criar upload privado com limites e retomada |
| `POST /v1/jobs` | Criar job idempotente com input, destinos e orçamento |
| `GET /v1/jobs/{id}` | Consultar etapas, compatibilidade e custo |
| `GET /v1/jobs/{id}/events` | Receber progresso resumido |
| `POST /v1/jobs/{id}/cancel` | Solicitar cancelamento persistente |
| `GET /v1/jobs/{id}/artifacts` | Obter links autorizados e temporários |
| `GET /v1/compatibility/{revision}` | Consultar evidências permitidas dessa revisão |

Hashes identificam conteúdo; não substituem autorização de acesso.

### 14.13 Planejamento de workers

- CPU: análise e compilação, com memória dimensionada pelos dados.
- CPU/microVM: execução de artefatos e parsing não confiável.
- GPU isolada: testes gráficos e experimentos que precisam de aceleração.
- Windows: build/validação em ambiente apropriado.
- Android físico: instalação, input, áudio, lifecycle e desempenho sustentado.

Não assumir passagem de GPU para Firecracker. Usar outro isolamento apropriado ou worker dedicado quando o experimento exigir GPU.

<a id="s15"></a>
## 15. Isolamento, dados e cadeia de distribuição

O serviço processará arquivos arbitrários e executará binários gerados. Essa característica torna isolamento parte da arquitetura funcional.

### 15.1 Fronteiras dos jobs

- Workers sem privilégios administrativos e sem acesso a segredos de produção.
- Disco temporário com quota e limpeza verificável.
- Toolchain previamente materializada e fixada por versão/hash.
- Rede desativada durante análise e execução, salvo necessidades explicitamente previstas.
- Nenhuma execução de scripts encontrados dentro da ISO.
- Metadados tratados como dados, nunca como shell ou código C++ sem escaping.
- Limites de recursão, expansão, arquivos, tamanho de logs e subprocessos.
- Validação de offsets, overflow, extents e tamanho de memória antes de leitura/cópia.
- Job não pode ler arquivos de outro usuário.
- Host não deve executar pacotes não validados fora do isolamento.

O inspector atual já contém validações úteis. Preservá-las ao ampliar formatos.

### 15.2 Política de conteúdo

Manter três grupos separados:

1. Código e dependências públicas da ferramenta.
2. Perfis e evidências publicáveis sem conteúdo privado.
3. ISO, extrações, código gerado do jogo, saves e snapshots privados.

O objetivo de processamento cloud está autorizado como **plano** nesta tarefa. Este documento não representa upload já realizado nem implantação de infraestrutura.

O usuário deve conhecer retenção, exclusão e acesso antes de enviar arquivos ao produto. O modo local continua útil e utiliza os mesmos contratos.

### 15.3 Cache sem vazamento entre usuários

O cache público pode guardar toolchains, runtime e fixtures públicas.

Caches de objetos derivados do jogo e de dados privados precisam respeitar isolamento por proprietário ou mecanismo explícito de autorização. Saber o hash de um arquivo não dá acesso ao seu conteúdo.

Se houver deduplicação física entre contas, ela não pode expor existência, metadados ou downloads sem autorização. Começar com cache privado por usuário é mais simples.

### 15.4 Assinatura e atualização

- Assinar somente depois dos gates.
- Chaves não entram no worker de parsing/compilação.
- Registrar identidade do aplicativo, versão, certificado e hash do payload.
- Manter estratégia estável de atualização e de compatibilidade dos saves.
- Evitar que uma nova build use outro certificado e impeça atualização.
- Armazenar símbolos e proveniência para diagnosticar crashes.
- Ter rollback da versão distribuída e revogação de links comprometidos.

A documentação oficial de [assinatura Android](https://developer.android.com/studio/publish/app-signing) orienta esse fluxo. O `signingConfigs.debug` atual é adequado para experimentação, não para estabelecer a identidade definitiva do produto.

### 15.5 Dependências e licença

O repositório contém licença GPL-3.0. Registrar origem, versão e licença de cada componente integrado e preservar os avisos.

Antes de distribuir uma combinação de runtime, renderer, codec e bibliotecas, conferir seus termos específicos e a forma de vinculação. Não assumir que todos os projetos listados no plano podem ser copiados indistintamente.

O inventário de dependências deve acompanhar a build. Não publicar jogos, firmware ou artefatos privados junto ao código da infraestrutura.


<a id="s16"></a>
## 16. Pacotes para PC e Android

### 16.1 Desktop

Linux e Windows precisam de artefatos próprios.

No Linux, registrar baseline de libc/CPU, dependências, paths relativos e localização de saves. Testar em máquina limpa, sem depender das bibliotecas do computador de desenvolvimento.

No Windows, validar MSVC ou clang-cl, DLLs, SIMD mínimo, ponto flutuante e empacotamento. O `/arch:AVX2` atual deve virar escolha consciente de baseline ou variante, com detecção adequada; não anunciar compatibilidade com todo PC x86-64.

O pacote final deve oferecer:

- Executável/launcher simples.
- Dados com integridade verificada.
- Input configurado.
- Saves fora de diretórios substituídos em atualizações.
- Logs de diagnóstico com limite.
- Identidade da revisão e da build.
- Desinstalação e atualização que preservem progresso.

### 16.2 Android ARM64

O projeto local configura SDK 34, NDK `28.2.13676358`, CMake `3.22.1`, min SDK 28 e AGP 8.6.1. Esses são valores observados, não uma recomendação de versões mais recentes.

Falta concluir ou validar:

1. Toolchain reproduzível, incluindo wrapper Gradle funcional.
2. Compilação ARM64 de todo código gerado e dependências.
3. Semântica SSE→NEON e implementação de operações específicas.
4. Carregamento e integridade de dados grandes.
5. Controles de toque, gamepad e reconexão.
6. Áudio, cinematics e dependências de codec.
7. Pause/resume, foco, chamadas, bloqueio de tela e retorno do background.
8. Saves persistentes e importação/exportação.
9. Assinatura estável.
10. Testes em aparelho real e diferentes drivers.
11. Compatibilidade de bibliotecas nativas com páginas de 16 KB.
12. Tratamento de falta de espaço e instalação interrompida.

A documentação de [páginas de 16 KB](https://developer.android.com/guide/practices/page-sizes) exige atenção ao alinhamento e a suposições do código nativo. Separar tamanho de página **convidado** de tamanho de página do **host**: um pode ser fixo por definição; o outro não deve ser presumido indevidamente.

### 16.3 Dados grandes e inicialização

Hoje a Activity copia os assets antes de iniciar o código nativo. Para jogos grandes, isso pode bloquear a inicialização, duplicar armazenamento e tornar atualizações lentas.

Evolução proposta:

- Preparação assíncrona com progresso.
- Copiar ou mapear somente o necessário, conforme o formato.
- Retomada de cópia e validação por hashes.
- Ativação atômica de uma versão de dados.
- Contagem de espaço para dados atuais, temporários e rollback.
- Separar saves de dados reinstaláveis.
- Manter visão de disco por setores quando exigida.

O destino pode usar APK com dados privados associados ou outra estratégia de distribuição apropriada. Um APK único gigantesco não deve ser requisito inflexível. “Sem complicações” significa o instalador resolver o fluxo.

Distribuição direta e publicação em loja têm requisitos distintos. Verificar políticas e limites vigentes na implementação; não fixar aqui um tamanho universal supostamente válido para todas as formas de entrega.

### 16.4 Compatibilidade de revisões e saves

Usar identidade de revisão para código, mas não necessariamente criar um aplicativo completamente diferente a cada rebuild.

Definir:

- ID estável do aplicativo por título/linha de compatibilidade.
- Versão de pacote.
- Hash da revisão de jogo.
- Formato e namespace do save.
- Migrações explicitamente suportadas.
- Backup antes de atualização de formato.

Não converter ou reinterpretar saves de outra revisão silenciosamente. Disponibilidade de arquivos não prova compatibilidade binária.

<a id="s17"></a>
## 17. Validação de correção e jogo completo

### 17.1 Pirâmide de evidências

| Camada | Teste | O que resolve |
|---|---|---|
| Entrada | ISOs/ELFs sintéticos válidos e malformados | Parsing, limites e identificação |
| Instrução | Vetores e comparação independente | Semântica EE/IOP/VU |
| Bloco | Estado e efeitos em fronteiras | Fluxo, delay slots e memória |
| Módulo | Carga/relocação/imports/descarga | Overlays, IRX e identidade |
| Subsistema | Replay DMA, RPC, GS, áudio e disco | Comportamento observável |
| Integração | Boot e cenários sintéticos completos | Contratos entre componentes |
| Título | Rotas, saves, transições e campanha | Compatibilidade real |
| Plataforma | Instalação, lifecycle e hardware | Entrega utilizável |
| Produto | ISO inédita submetida sem edição | Automação efetiva |

A suíte atual é uma base importante. O número de testes aprovados não deve ser convertido em porcentagem de jogos suportados.

### 17.2 Referências

Usar resultados de hardware quando disponíveis e apropriados, incluindo [ps2autotests](https://github.com/unknownbrackets/ps2autotests). Emuladores maduros ajudam a ampliar cenários, mas divergências entre referências precisam de investigação.

Registrar versão, configuração, origem do programa, entradas e hardware. Um replay capturado com configuração diferente de temporização pode não ser comparável.

Não assumir que save states binários de outro runtime podem ser carregados diretamente. Replays desde boot e formato próprio de snapshots são mais controláveis.

### 17.3 Determinismo e replay

Controlar ou registrar:

- Input por frame/tempo convidado.
- Relógios visíveis ao jogo.
- Estado inicial e save de origem.
- Seed ou fontes de aleatoriedade relevantes.
- Resultado e ordem de I/O.
- Eventos de interrupção e scheduler.
- Identidade de código e dados.

Quando não for possível reproduzir exatamente, comparar invariantes e intervalos explicitamente justificados. Nunca ampliar tolerância sem explicar a causa.

### 17.4 Imagem e áudio

Frames:

- Comparação exata onde o pipeline permitir.
- Métricas perceptuais e máscaras documentadas onde houver variação legítima.
- Capturas em transições, efeitos, UI e cenas difíceis.
- Estado de GS/VRAM para localizar diferenças.
- Verificação de frame realmente novo: apresentar a mesma imagem repetida não é progresso.

Áudio:

- Eventos e posições de reprodução.
- Continuidade, canais, latência e sincronização.
- Comparação de amostras quando os caminhos forem determinísticos.
- Ausência de underruns persistentes.

Captura bonita isolada não comprova interação nem progresso.

### 17.5 Exploração automática do jogo

Para a meta sem intervenção, desenvolver um explorador que combine:

- Replays existentes associados à revisão.
- Estados salvos controlados e marcos reconhecíveis.
- Novidade de código, cena e evento.
- Regras de navegação de menus.
- Planejamento de ações em tarefas delimitadas.
- Reconhecimento visual e sinais internos de estado.
- Verificação de avanço real e de softlocks.

O explorador deve distinguir “não conseguiu vencer o desafio” de “o port está errado”. Um sistema que joga mal pode gerar falso defeito; um sistema que evita uma área pode perder um defeito real.

A geração universal de uma campanha completa de teste para um jogo desconhecido é um subprojeto de pesquisa. Não esconder essa dependência atrás da palavra “automático”.

### 17.6 O que “perfeito” significa para entrega

Adotar um padrão operacional exigente:

- Nenhum bloqueio conhecido que impeça concluir o protocolo.
- Nenhuma corrupção de save observada.
- Nenhuma divergência sem explicação nos testes determinísticos obrigatórios.
- Áudio, cenas e controles funcionando.
- Desempenho sustentado aprovado no hardware declarado.
- Nenhum fallback de instruções oculto.
- Nenhum patch de triagem pendente no caminho qualificado.
- Relatório reprodutível das rotas e limitações.

Isso é verificável. Ausência absoluta de qualquer bug em qualquer input e qualquer celular não é uma propriedade que esta bateria finita consiga certificar.

<a id="s18"></a>
## 18. Catálogo e crescimento da compatibilidade

### 18.1 Crescimento por capacidade

Após Monster House, selecionar títulos que exercitem classes novas de comportamento:

- Outro compilador ou convenção de funções.
- Outros módulos IOP.
- Outro conjunto VU.
- Uso mais intenso de framebuffer/feedback.
- Outro formato de streaming.
- Overlays e relocações diferentes.
- Região, vídeo e tradução modificada.
- Casos de saves e transições mais complexos.

A correção de um subsistema deve beneficiar uma classe de jogos. Isso tem maior valor que acrescentar muitos hooks desconectados.

### 18.2 Catálogo não é dependência obrigatória de suporte manual

O catálogo registra conhecimento e evidência. Ele não deve se tornar uma lista fechada em que todo jogo desconhecido exige um engenheiro.

Para cada revisão nova, medir:

```text
sucesso direto
sucesso após autorreparo
falha após orçamento
intervenção humana durante desenvolvimento
tempo até solução geral
```

Somente os dois primeiros são sucessos do caminho autônomo. Contabilizar um reparo manual como automático falsearia a meta.

### 18.3 Estratégia de corpus

Etapas propostas:

1. Fixtures sintéticas públicas e resultados de hardware.
2. Monster House como piloto profundo.
3. Pequeno grupo contrastante de títulos privados disponíveis.
4. Conjunto estratificado maior, congelado antes da avaliação.
5. Revisões/títulos nunca usados para orientar reparos.
6. Expansão contínua com regressão das versões anteriores.

Não é necessário publicar ISOs para publicar metodologia, hashes permitidos e métricas.

### 18.4 Generalização e promoção

Uma regra nova deve passar por:

- Casos em que deve aplicar.
- Casos semelhantes em que deve recusar aplicação.
- Regressões das capacidades relacionadas.
- Pelo menos um teste reservado quando houver dados suficientes.
- Comparação com a regra anterior.
- Rollback se a compatibilidade líquida piorar.

O objetivo de médio prazo é aumentar o conjunto de **classes de problema resolvidas automaticamente**, não apenas contar perfis.

<a id="s19"></a>
## 19. Observabilidade e classificação de falhas

### 19.1 Eventos estruturados

Cada evento relevante deve carregar:

```text
job_id, attempt_id, stage, target
ISO/ELF/module hash
guest_pc, code_epoch, subsystem
event_kind, guest_time, host_time
error_class, evidence_ref
```

Não colocar bytes de jogo ou segredos em logs públicos. Capturas detalhadas ficam privadas.

### 19.2 Ausência de progresso

Usar watchdog com vários sinais:

- PCs/blocos diferentes.
- Ciclos e eventos convidados.
- Progresso de I/O.
- Frames novos.
- Respostas a input.
- Mudança de estado/marco do jogo.

Um vídeo pode avançar sem gameplay, e um loading legítimo pode ficar visualmente parado. O watchdog precisa classificar o contexto, não matar qualquer tela estática.

Quando houver suspeita de hang, capturar evidência antes do encerramento.

### 19.3 Logs limitados e úteis

- Streaming de subprocessos em vez de stdout inteiro na RAM.
- Limite por job e rotação.
- Agregação de eventos repetidos por chave.
- Contador de repetições e primeira/última ocorrência.
- Buffer circular de detalhes anteriores à falha.
- Coleta detalhada sob demanda.
- Barra de progresso por etapa com duração observada.

O log de aproximadamente 689 MB é um caso concreto para validar essa melhoria.

### 19.4 Taxonomia mínima

| Classe | Retry automático? | Ação |
|---|---|---|
| Rede/worker perdido | Sim, limitado | Retomar etapa idempotente |
| OOM | Uma tentativa ajustada, se orçamento permitir | Reduzir paralelismo ou escolher memória adequada |
| Input inválido | Não | Relatório de formato/offset |
| Build incorreta | Pelo motor de reparo | Redutor de compilação e transformação |
| Alvo ausente | Pelo motor de análise | Descobrir origem e epoch |
| Divergência semântica | Pelo verificador/reparo | Primeiro evento diferente e fixture |
| Timeout sem progresso | Após classificação | Snapshot e análise de espera |
| GPU/driver | Conforme evidência | Reproduzir e comparar backend/dispositivo |
| Orçamento esgotado | Não silenciosamente | Resultado explícito e checkpoints |

Retries cegos de falhas determinísticas desperdiçam tempo e dinheiro.


<a id="s20"></a>
## 20. Marcos e ordem de execução

### 20.1 Caminho crítico até o primeiro jogo completo

```text
G0: base reproduzível e relatórios fiéis
  -> G1: diagnóstico do bloqueio atual
  -> G2: boot, menu e primeira área
  -> G3: serviços, gráficos, áudio, vídeo e saves corretos
  -> G4: campanha e rotas obrigatórias no PC
  -> G5: pacote Android e protocolo em aparelho real
  -> G6: EE/IOP/VU necessários executados em AOT estrito
  -> G7: ISO limpa gera os dois pacotes sem edição manual
  -> G8: novas falhas das classes aprendidas são reparadas automaticamente
  -> G9: escala de catálogo e meta dos 90%
```

A instrumentação do motor autônomo começa em G0. G8 é a demonstração de autonomia, não o momento de começar a pensá-la.

O trabalho de IOP/VU AOT pode começar assim que as fronteiras estiverem testáveis. Não esperar toda a campanha para descobrir que o modo estrito é inviável para um componente obrigatório.

### 20.2 Critérios por marco

| Marco | Saída obrigatória | Impedimento para avançar a classificação |
|---|---|---|
| G0 | Configuração persistente, hashes, geração e build repetíveis | Patches só em `build/` ou manifest desatualizado |
| G1 | PC/espera/causa do bloqueio com reprodutor | Suposição baseada apenas na última linha do log |
| G2 | Menu e gameplay controlável, cold boot reproduzível | Retornos artificiais escondendo falhas |
| G3 | Subsistemas usados pelo jogo aprovados | Áudio/mídia/saves ausentes |
| G4 | Campanha PC registrada, regressões e cenas obrigatórias | Softlock, corrupção ou caminho principal incompleto |
| G5 | Instalação e campanha Android, lifecycle e desempenho | Só compilar ARM64 ou testar em ambiente simulado |
| G6 | Contrato AOT cumprido, versão desconhecida diagnosticada | Interpretação/JIT de jogo no pacote qualificado |
| G7 | Submissão do zero reproduz resultado e evidência | Ajuste manual por submissão |
| G8 | Falhas novas de classes cobertas resolvidas sem edição | Motor só repete patches por endereço |
| G9 | Catálogo congelado, taxa conjunta e generalização publicadas | Numerador sem denominador ou exclusão silenciosa de falhas |

É possível trabalhar em infraestrutura e perfis de build enquanto uma falha é investigada, mas mudanças demais no mesmo experimento dificultam localizar a causa.

### 20.3 A ordem para ganhar velocidade

1. Tornar o erro observável.
2. Reduzir o ciclo de build.
3. Corrigir a classe do erro.
4. Guardar fixture e reparo.
5. Repetir o jogo desde boot.
6. Aumentar cobertura.
7. Qualificar o título.
8. Automatizar a entrega em nuvem.
9. Expandir a descoberta e reparo para títulos inéditos.
10. Aumentar hardware apenas quando o trabalho for paralelizável e medido.

Uma interface cloud pode ser prototipada cedo, mas escalar submissões de um runtime que não completa o piloto apenas multiplica falhas.

<a id="s21"></a>
## 21. Backlog executável

Cada item abaixo precisa ser dividido em alterações pequenas quando ultrapassar uma sessão focada. “Concluído” requer evidência, não apenas implementação.

### 21.1 Primeira sequência: tornar Monster House depurável e reproduzível

| ID | Dependência | Entrega | Verificação | Áreas principais |
|---|---|---|---|---|
| T01 | Nenhuma | Snapshot de fontes/configuração/artefato e manifest fiel | Hashes correspondem ao runner testado | `tools/ps2native`, workspace |
| T02 | T01 | Args de boot persistentes por revisão | Regenerar do zero e observar os mesmos argumentos | overrides, config, inicialização |
| T03 | T01 | Correção FPU no emissor/helper | C++ regenerado compila sem edição manual | `fpu_translator.cpp`, helpers |
| T04 | T01 | Configuração de disco reproduzida pelo pipeline | Launcher novo passa a ISO e leituras por setor funcionam | `pipeline.py`, `main.cpp`, VFS/CD |
| T05 | T01 | Build de diagnóstico separada, com flags fixas | Mesma fonte, símbolos e hash registrados | CMake, pipeline |
| T06 | T05 | Snapshot de hang e progresso | Capturar PC, stack, filas, IOP e transfers sem log ilimitado | runtime, scheduler, IOP |
| T07 | T06 | Reprodutor do primeiro bloqueio | Falha reproduz em cenário menor | fixture e subsistema identificado |
| T08 | T07 | Correção de causa, detector e regressão | Teste reduzido + boot frio alcançam novo marco | Componente responsável |
| T09 | T01 | Relatório único por endereço e classificação de dados | Revisar ocorrências em `sub_00355F80`; manter desconhecidos visíveis | parser, análise, reporter |
| T10 | T09 | Alvos indiretos e continuations com evidência | Casos de vtable, label interno e overlay | CFG, emitter, registry |

**Prioridade operacional imediata:** T01–T08. T09/T10 fecham uma fonte grande de incerteza e custo, mas não devem atrasar a captura do bloqueio real.

### 21.2 Reduzir o tempo de cada tentativa

| ID | Dependência | Entrega | Verificação |
|---|---|---|---|
| T11 | T01 | Métricas de tempo/RAM por etapa | Baseline frio e incremental, mesmas flags/hardware |
| T12 | T11 | Geração que preserva arquivos idênticos | Reexecução sem alterações não recompila o jogo inteiro |
| T13 | T10/T11 | Registro dividido/compacto | Mesmos bindings e comportamento em todos os casos de borda |
| T14 | T11 | Experimento unity/PCH/shards | Comparação de custo total, memória e cache |
| T15 | T12 | Cache privado por identidade completa | Hit correto; mudança de ABI/flags invalida |
| T16 | T11 | Separação runtime/jogo | Alterar serviço não recompila milhares de fontes estáveis |
| T17 | T11 | Logs em streaming e quotas | Processo verboso não esgota RAM/disco |
| T18 | T14–T16 | Experimento ThinLTO/PGO | Ganho de runtime medido sem regressão semântica |

Nenhum ganho percentual dessas tarefas foi medido nesta auditoria. Registrar antes/depois e descartar mudanças sem benefício.

### 21.3 Do boot à campanha

| ID | Dependência | Entrega | Verificação |
|---|---|---|---|
| T19 | T08/T10 | Serviços IOP e RPC usados no boot | Imports, binds, callbacks e erros corretos |
| T20 | T19 | Menu e primeira área | Cold boot, input, carregamento e primeira transição |
| T21 | T20 | Gráficos corretos nos caminhos observados | Replay GS e cenas de referência |
| T22 | T20 | Áudio e cinematics reais do jogo | Reprodução, pausa, skip quando permitido e sincronização |
| T23 | T20 | Save/load e estabilidade | Salvar, fechar, reabrir, carregar e continuar |
| T24 | T21–T23 | Campanha PC e rotas obrigatórias | Registro dos marcos e ausência de bloqueios conhecidos |
| T25 | T19 | Frontend/execução AOT de IRX usados | Comparação com referências e zero instruções IOP interpretadas |
| T26 | T20 | Microprogramas VU usados em AOT | Estados/efeitos iguais e versões conhecidas |
| T27 | T25/T26 | Alvo estrito sem interpretadores convidados necessários | Inventário de execução, linkage e cenários completos |
| T28 | T20 | Build e bootstrap ARM64 | ELF nativo correto, instalação e inicialização em aparelho |
| T29 | T28 | Input, lifecycle, mídia, dados e saves Android | Roteiro de dispositivo e casos de interrupção |
| T30 | T24/T27/T29 | Monster House completo nos destinos | Protocolo E4 e desempenho sustentado |

As tarefas T25/T26 são projetos relevantes, não correções de poucas linhas. Podem revelar que a semântica observada ainda está incompleta.

### 21.4 Autonomia e poucos minutos

| ID | Dependência | Entrega | Verificação |
|---|---|---|---|
| T31 | T06/T08 | Assinaturas de falha e reprodutores automáticos | Erros injetados conhecidos são classificados |
| T32 | T09/T10/T31 | Reparos automáticos de descoberta | Alvos/dados corrigidos em fixtures não usadas no ajuste |
| T33 | T25/T26/T31 | Seleção/síntese limitada de reparos semânticos | Verificador independente rejeita candidatos errados |
| T34 | T32/T33 | Loop com orçamento, rollback e memória | Não repete tentativas e não aprova regressões |
| T35 | T15/T17 | Serviço cloud mínimo | Job retomável, cancelável e isolado |
| T36 | T30/T35 | Conversão limpa PC+Android | Nenhum arquivo editado manualmente durante submissão |
| T37 | T18/T36 | Caminho rápido com cache | P50/P95 medidos no cenário declarado |
| T38 | T34/T36 | Jogo/revisão inédita por autorreparo | Zero intervenção e evidência E4 independente |
| T39 | T38 | Corpus estratificado e reservado | Taxas de generalização, falhas e custo publicadas |
| T40 | T39 | Marco 90% conjunto | Critérios da seção 3 cumpridos |

### 21.5 Evidência mínima por tarefa

Cada tarefa deve produzir: problema, hipótese, alteração, entradas exatas, comando de verificação, resultado e regressões consideradas.

Não encerrar uma tarefa de correção apenas porque desapareceu o erro original. Conferir que o comportamento esperado realmente aconteceu.

<a id="s22"></a>
## 22. Riscos previstos e respostas

| Risco | Sinal inicial | Resposta planejada | Condição de bloqueio |
|---|---|---|---|
| Dados confundidos com código | Muitos opcodes estranhos e funções enormes | Mapa código/dados, CFG e evidência dinâmica | Região executada continua desconhecida |
| Alvo indireto ausente | Salto para PC sem binding | Descoberta de valor, tabela e epoch | Alvo não pode ser associado com segurança |
| Binding antigo após overlay | Erro ao trocar área/módulo | Registro por identidade e versão | Código diferente usa tradução antiga |
| FPU/MMI diferente no ARM | Física/flags divergem só no Android | Vetores, helpers exatos e comparação cruzada | Divergência semântica não explicada |
| UB do C++ gerado | Resultado muda com otimização | Operações definidas e sanitizers em fixtures | Release depende de UB |
| HLE reconhecida incorretamente | Boot avança, dados corrompem depois | Assinaturas com contrato e testes negativos | Correspondência ambígua |
| Espera infinita no IOP/RPC | Flag não muda, threads bloqueadas | Captura de produtor/consumidor e eventos | Não há evento válido para desbloqueio |
| VU microcódigo novo | Hash desconhecido em nova área | Inventário de versões e compilação offline | Pacote estrito precisaria interpretar |
| GS incompleto | Efeitos/transparências incorretos | Replay de comandos e backend de referência | Divergência relevante persistente |
| Áudio aparentemente presente, mas errado | Underruns/desync/vozes ausentes | Testar streams, envelopes e tempo | Cenas ou gameplay dependem de áudio ausente |
| Disco extraído sem semântica de setores | LBN sem arquivo correspondente | Imagem/dispositivo de setores | Setores necessários não representados |
| Vídeo não coberto | Filme preto ou bloqueio de transição | Investigar caminho real de codec | Cinematics essenciais não funcionam |
| Save corrompido | Progresso não retorna após reinício | Fixtures, escrita atômica e backups | Corrupção observada |
| Log e build esgotam RAM/disco | Crescimento sem limite | Streaming, quotas e shards | Job ameaça estabilidade do worker |
| Cache devolve código incompatível | Falha após troca de flags/runtime | Chave completa e atestação | Proveniência não verificável |
| Perfil aplicado à revisão errada | Tradução/mod muda comportamento | SHA-256 e bytes esperados | Precondições falham |
| Autorreparo aprende o replay | Passa caso observado, falha variante | Casos reservados e perturbações | Não generaliza ao contrato |
| Referência também tem bug | Emuladores discordam | Reduzir e conferir hardware/documentação | Não há expectativa confiável |
| Exploração não conclui o jogo | Repetição sem novos marcos | Planejamento, estados e novas rotas | Não há evidência de conclusão |
| Otimização altera timing | Erro intermitente no release | Tempo convidado e testes de ordem | Resultado depende de corrida do host |
| APK instala, mas não sustenta jogo | Throttling ou falta de RAM | Teste prolongado em aparelho real | Hardware mínimo anunciado não atende |
| Correção global quebra outro título | Regressão no corpus | Escopo restrito e rollback | Ganho local causa perda não aceita |
| Infraestrutura cresce antes do runtime | Muitos jobs, poucos jogos completos | Gates de compatibilidade | Escala não reduz custo por sucesso |
| Promessa de poucos minutos omite rede | Upload domina duração | Mostrar tempo ponta a ponta e por etapa | SLO anunciado não corresponde à medição |

A lista não é exaustiva. Cada incidente novo deve acrescentar uma classe observável, um teste e uma resposta reutilizável.

<a id="s23"></a>
## 23. Custos, capacidade e prazo

### 23.1 Meta de poucos minutos

Metas iniciais propostas, ainda sem benchmark:

| Cenário | Meta de engenharia | O que está incluído |
|---|---|---|
| Revisão e artefatos já qualificados em cache autorizado | P95 de processamento ≤ 5 minutos | Verificação de identidade, seleção, checks rápidos, empacotamento/entrega preparada |
| Revisão conhecida, objetos parciais em cache | Buscar P95 ≤ 10 minutos | Compilar mudanças necessárias e regressões apropriadas |
| Revisão conhecida, build fria | Primeiro medir; depois buscar redução para poucos minutos | Geração/compilação por destinos e validação exigida |
| Revisão inédita com falha desconhecida | Sem prazo garantido | Descoberta, autorreparo e qualificação |
| Campanha completa nova | Orçamento separado | Exploração, correção e evidência de conclusão |

A meta final inclui reduzir também o custo da primeira conversão, por generalização das regras e modelos de hardware. Cache sozinho não prova conversão universal rápida.

### 23.2 Quando reutilizar evidência

Evidência pode ser reutilizada quando identidade do código/dados, runtime, perfil, destino e condições relevantes correspondem ao resultado qualificado.

Se muda a semântica de um componente, invalidar os cenários afetados. Não reutilizar um selo completo apenas porque o nome do jogo é igual.

Uma campanha já comprovada não precisa ser repetida integralmente para cada download do **mesmo artefato**. Uma correção nova pode exigir nova campanha ou conjunto de regressões justificado pelo seu escopo.

Acelerar testes pela divisão em checkpoints só é válido após demonstrar que os checkpoints preservam o estado e as dependências necessárias. Pular cutscenes para reduzir duração não comprova que elas funcionam.

### 23.3 Orçamento de latência do caminho rápido

Distribuição ilustrativa do orçamento de cinco minutos, a validar:

```text
identidade e consulta:             30 s
análise/geração reutilizável:      30 s
build incremental por destinos:  120 s
checks e empacotamento:            90 s
margem de variação:                30 s
total de processamento:           300 s
```

Fila, upload e download devem aparecer separadamente e também no total ponta a ponta. A divisão não é medição do protótipo atual.

Por exemplo, transmitir 4 GiB a 20 Mbit/s leva cerca de 28,6 minutos mesmo sem overhead. Nenhum cache de compilação elimina esse limite físico quando o upload completo é necessário. Retomada e reaproveitamento autorizado de blocos podem reduzir transferências repetidas.

### 23.4 Custo por job

```text
custo =
  CPU-horas por tipo de worker
  + GPU-horas realmente usadas
  + armazenamento × retenção
  + operações e tráfego
  + minutos de dispositivo/laboratório
  + inferência opcional de modelos
```

Não há preços de provedor fixados nem infraestrutura contratada por este documento.

Medir custo por:
- Build aprovada.
- Revisão nova qualificada.
- Classe de falha resolvida.
- Minuto de evidência útil.
- Conversão repetida em cache.

Acompanhar desperdício com reexecuções, logs gigantes, cache misses e tentativas de reparo repetidas.

### 23.5 Planejamento de prazo

Uma data confiável para Monster House depende primeiro de localizar o bloqueio atual e medir a quantidade de comportamento ausente. As 13.706 ocorrências do relatório não podem ser convertidas diretamente em dias de trabalho.

Depois de G2, estimar separadamente:
- Correções semânticas.
- IOP/VU AOT.
- Gráficos e mídia.
- Android e hardware.
- Campanha e regressões.
- Automação e infraestrutura.

Usar histórico de tarefas concluídas e variação observada para produzir uma faixa de previsão. Revisá-la a cada marco.

A otimização da build reduz espera entre experimentos. Ela não transforma automaticamente meses de comportamento desconhecido em minutos de engenharia.


<a id="s24"></a>
## 24. Organização do código e decisões de arquitetura

### 24.1 Estrutura proposta, preservando a base

```text
ps2xAnalyzer/          descoberta e classificação
ps2xRecomp/            semântica, IR incremental e emissão
ps2xIOP/               serviços, loader e futura execução AOT
ps2xRuntime/           memória, eventos, dispositivos e host
tools/iso_inspect/     disco e inventário
tools/ps2native/       pipeline local e executor de etapas
android/              integração e empacotamento Android

Novos diretórios propostos:
profiles/             metadados e precondições, sem assets
tools/lab/            captura, replay, redução e comparação
tools/repair/         regras, candidatos, orçamento e promoção
services/             API e coordenação cloud
schemas/              contratos versionados
tests/corpus/         fixtures publicáveis e descrições de cenários
```

Os diretórios novos não foram criados nesta tarefa. Este README é o único arquivo novo do planejamento.

### 24.2 Decisões principais

| Decisão | Motivo | Revisitar quando |
|---|---|---|
| Evoluir PS2Recomp | Já há pipeline e runtime úteis | Evidência mostrar que uma fronteira específica custa mais que alternativa |
| C++ primeiro | Menor distância até o protótipo atual | IR direto trouxer ganho mensurável e semântica validada |
| Estado explícito e AOT por blocos onde necessário | Preservar controle não estruturado | Otimização demonstrar contrato mais restrito |
| AOT estrito no produto qualificado | Atender a execução nativa solicitada | Só mediante mudança explícita do requisito |
| Referência de execução no builder | Descoberta e correção verificáveis | Referência insuficiente exigir hardware/novo método |
| Reparo baseado em evidência | Autonomia sem depender de edição por jogo | Expandir classes de reparo, mantendo verificador independente |
| Perfis automáticos por identidade forte | Suportar diferenças legítimas de revisão | Generalizar regras quando evidência permitir |
| CMake e pipeline Python existentes | Aproveitar o trabalho atual | Escala justificar migração medida |
| Cloud mínima e jobs idempotentes | Evitar infraestrutura dominar compatibilidade | Capacidade/isolamento exigirem orquestração maior |
| Certificação por revisão e destino | Impedir afirmações genéricas sem teste | Novos destinos/catálogos criarem novas versões da métrica |

### 24.3 Disciplina de alteração

- Corrigir a camada responsável pelo comportamento.
- Evitar patches repetidos em fontes geradas.
- Manter semântica convidada independente do host.
- Preservar interfaces e versionar mudanças.
- Todo reparo reutilizável deve ter precondições.
- Toda otimização deve ter baseline.
- Todo resultado de compatibilidade deve apontar para artefato e evidência.
- Separar fatos observados, inferências e propostas.
- Não misturar atualização de dependências com diagnóstico de um bug sem necessidade.
- Nenhuma tarefa é concluída por passar apenas no caso que inspirou o patch.

<a id="s25"></a>
## 25. Comandos que existem hoje

Os comandos abaixo refletem interfaces presentes no repositório. São instruções para execução futura; **não foram executados como uma nova build ou nova suíte durante a escrita deste plano**.

### 25.1 Ferramentas de host

Executar na raiz do repositório:

```sh
cmake -S . -B out/host-tools \
  -DCMAKE_BUILD_TYPE=Release \
  -DPS2X_BUILD_RUNTIME=OFF \
  -DPS2X_BUILD_TEST=OFF \
  -DPS2X_BUILD_STUDIO=OFF

cmake --build out/host-tools \
  --target ps2iso-inspect ps2_analyzer ps2_recomp \
  --parallel 4
```

O caminho `out/host-tools` não é um dos defaults documentados do descobridor da CLI. Para usar esses executáveis com o pipeline, passar `--inspector`, `--analyzer` e `--recompiler` com seus caminhos de saída efetivos, ou configurar as variáveis `PS2NATIVE_*` correspondentes.

### 25.2 Inspeção e preparação de pacote

Com ferramentas já descobertas/configuradas:

```sh
python3 -m tools.ps2native inspect \
  --iso "/caminho/jogo.iso" \
  --json-output

python3 -m tools.ps2native build \
  --iso "/caminho/jogo.iso" \
  --target desktop \
  --out "/caminho/novo-pacote-pc" \
  --build-jobs 4

python3 -m tools.ps2native build \
  --iso "/caminho/jogo.iso" \
  --target android \
  --out "/caminho/novo-pacote-android"
```

O destino `--out` deve ser novo. O alvo desktop usa o host atual; não existe hoje um argumento implementado que gere todos os sistemas de PC de uma vez.

O alvo Android pode parar por falta de toolchain. Mesmo um APK produzido precisa passar pelos gates de execução.

### 25.3 Verificação existente

```sh
python3 -m unittest discover \
  -s tools/ps2native/tests \
  -p 'test_*.py'

cmake -S . -B out/dev \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPS2X_BUILD_STUDIO=OFF \
  -DPS2X_ENABLE_DEBUG_UI=OFF

cmake --build out/dev --target ps2x_tests --parallel 4
./out/dev/ps2xTest/ps2x_tests
```

IOP isolado:

```sh
cmake -S ps2xIOP -B out/iop-tests \
  -DPS2X_IOP_BUILD_TESTS=ON

cmake --build out/iop-tests --parallel 4
ctest --test-dir out/iop-tests --output-on-failure
```

As suítes têm escopos distintos. Passar no CTest de uma subárvore não implica executar todos os testes do projeto.

### 25.4 Artefatos atuais de Monster House

A entrada fornecida foi:

```text
/home/pedrohs/Downloads/Monster House (BR-USA) (T2.0) (www.romsportugues.com).iso
```

Dentro do workspace descrito na seção 2:

```text
analysis/ps2native.toml
analysis/output/
logs/recompiler-vtable-batch.log
logs/cmake-build-vtable-batch.log
logs/desktop-smoke-iso.log
logs/desktop-smoke-iso-25s.png
desktop-project/build/
package/bin/ps2EntryRunner
package/run-ps2native.sh
manifest.json
```

Esses arquivos são evidência experimental. O runner atual não é uma entrega jogável qualificada.

<a id="s26"></a>
## 26. Critérios finais e próximos passos

### 26.1 Entrega do primeiro jogo

- [ ] Monster House regenerado sem edições manuais em fontes produzidas.
- [ ] Bloqueio atual explicado e corrigido com teste.
- [ ] Código/dados, alvos e módulos relevantes identificados.
- [ ] Campanha PC concluída no protocolo.
- [ ] Saves, áudio, cinematics, controles e transições aprovados.
- [ ] Android instalado e aprovado em aparelho real.
- [ ] Desempenho sustentado documentado.
- [ ] EE/IOP/VU necessários em AOT estrito.
- [ ] Pacotes vinculados aos seus manifests e evidências.
- [ ] ISO limpa reproduz a entrega sem intervenção durante a submissão.

### 26.2 Entrega do produto autônomo

- [ ] Falhas são classificadas e reduzidas automaticamente.
- [ ] Reparos reutilizáveis têm precondições e validação.
- [ ] Candidatos não podem alterar seus critérios de aprovação.
- [ ] Regressões provocam rollback.
- [ ] Novas revisões passam pelo fluxo sem edição humana.
- [ ] O sistema produz perfis automaticamente quando necessário.
- [ ] O catálogo reservado mede generalização.
- [ ] Custos, tentativas e tempo têm limites.
- [ ] Caminho rápido atende à meta medida de minutos.
- [ ] Cobertura conjunta de 90% foi demonstrada no catálogo definido.
- [ ] Falhas restantes permanecem contadas e orientam expansão.

### 26.3 Próxima ação concreta

Retomar em **T01–T08**, com foco em obter um diagnóstico reproduzível do bloqueio de Monster House.

O primeiro resultado útil deve ser um pacote de evidências com PC, pilha, estado de espera, módulos/serviços e o menor reprodutor possível. A partir dele, corrigir a causa e incorporar a capacidade de detectar/verificar essa classe no motor automático.

A cada bloqueio resolvido, perguntar operacionalmente: **qual parte desta solução permite que a ferramenta resolva o próximo caso sozinha?**

### 26.4 Estado ao concluir este documento

Foi realizada análise de fontes, configuração, manifests e logs existentes, além de pesquisa em fontes primárias dos projetos citados. Não foi executada uma nova campanha, nova build do jogo ou implantação cloud durante a redação.

O plano está escrito. A execução da arquitetura proposta, o port completo e a demonstração dos 90% continuam sendo trabalho a realizar.

### 26.5 Estado da execução em 29 de setembro de 2026

Este checkpoint sucede a redação do plano e registra trabalho já iniciado:

- T01 continua **parcial**: o pipeline calcula fingerprint de fontes/configuração, hashes de ferramentas, inventário integral do pacote e oferece `ps2native verify`. Uma build desktop limpa pelo pipeline está em andamento para vincular manifesto, fontes e runner recém-gerados.
- O workspace histórico ainda é evidência histórica: seu inventário observado cobre os arquivos atuais daquele pacote, sem provar quais bytes entraram na build original. `ps2native verify` conferiu seus 2.314 arquivos imutáveis.
- Os 17 testes Python do pipeline e os quatro executáveis de teste C++ do IOP passam. A suíte C++ de compatibilidade contém dois testes novos para registrar LOADFILE no reset, responder a versão e carregar um módulo HLE.
- T07 localizou o bloqueio reproduzível: Monster House espera o servidor SIF `LOADFILE`, SID `0x80000006`; `client->server` fica nulo em `0x3cbce4`. O loop `_lf_bind` em `0x119788` repete o bind enquanto o servidor não aparece. O código de `IopSubsystem::reset()` apagava os servidores do IOP, mas não iniciava LOADFILE.
- T08 está **parcial**: o runtime agora ativa um serviço LOADFILE HLE genérico no reset, com versão e carga de módulo por RPC usando o carregador existente. O teste reduzido falhava antes da mudança e passa depois. Ainda falta o boot frio da ISO e registrar qual chamada LOADFILE vem em seguida.
- A pilha em `EeScheduler::processDueDeadlines()` representava a thread esperando entre consultas guest; não era a causa do bloqueio. O relatório [monster-house-diagnostic-capture.md](build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-49a313b6d817/logs/monster-house-diagnostic-capture.md) separa essa observação inicial da captura posterior de PC, registradores e memória válida.
- A build limpa de PC está compilando 7.235 arquivos C++ gerados pela ISO. O próximo marco é atingir o bind LOADFILE, descobrir as chamadas seguintes e qualificar a primeira tela jogável. Campanha completa, áudio, controle, saves e Android continuam sem validação.

<a id="s27"></a>
## 27. Fontes e glossário

### 27.1 Fontes locais da auditoria

- [PROJECT_SPEC.md](PROJECT_SPEC.md): visão anterior, estado do pipeline e limites.
- [README.md](README.md): base upstream e integração local.
- [tools/ps2native/README.md](tools/ps2native/README.md): contratos da CLI e outputs.
- [tools/ps2native/pipeline.py](tools/ps2native/pipeline.py): build, launchers, manifests e subprocessos.
- [tools/iso_inspect/README.md](tools/iso_inspect/README.md): formatos, extração e limites de entrada.
- [docs/RECOMPILER_GAPS.md](docs/RECOMPILER_GAPS.md): lacunas registradas; conferidas contra código relevante.
- [docs/NATIVE_HANDOFF.md](docs/NATIVE_HANDOFF.md): experimento de integração de ELF sintético.
- [ps2xRecomp/src/lib/function_table_emitter.cpp](ps2xRecomp/src/lib/function_table_emitter.cpp): registros gerados.
- [ps2xRuntime/CMakeLists.txt](ps2xRuntime/CMakeLists.txt): geração, unity, PCH, IPO e destinos.
- [ps2xRuntime/cmake/ReleaseMode.cmake](ps2xRuntime/cmake/ReleaseMode.cmake): flags de release.
- [ps2xRuntime/src/main.cpp](ps2xRuntime/src/main.cpp): entrada ELF/ISO.
- [ps2xRuntime/src/lib/Kernel/Stubs/SIF.cpp](ps2xRuntime/src/lib/Kernel/Stubs/SIF.cpp): comportamento atual de serviços.
- [ps2xRuntime/include/game_overrides.h](ps2xRuntime/include/game_overrides.h): escopo de overrides.
- [ps2xIOP/README.md](ps2xIOP/README.md): execução atual de IRX.
- [android/app/build.gradle](android/app/build.gradle): ABI, toolchain e assinatura.
- [Ps2PackageActivity.java](android/app/src/main/java/com/ps2x/runner/Ps2PackageActivity.java): preparação de assets.
- [.github/workflows/build.yml](.github/workflows/build.yml): CI existente.
- [LICENSE](LICENSE): licença da base.

Os logs do workspace são privados e podem mudar ou ser removidos. Os valores citados neste documento são um retrato da auditoria de 28–29/09/2026.

### 27.2 Fontes externas

As fontes primárias estão ligadas junto às propostas correspondentes. Principais grupos:

- Recompilação: [PS2Recomp](https://github.com/ran-j/PS2Recomp), [N64Recomp](https://github.com/N64Recomp/N64Recomp), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp).
- Referência PS2: [PCSX2](https://github.com/PCSX2/pcsx2), [Play!](https://github.com/jpd002/Play-), [ps2sdk](https://github.com/ps2dev/ps2sdk), [ps2autotests](https://github.com/unknownbrackets/ps2autotests), [ps2tek](https://psi-rockin.github.io/ps2tek/).
- Análise: [Rabbitizer](https://github.com/Decompollaborate/rabbitizer), [Ghidra](https://github.com/NationalSecurityAgency/ghidra), [angr](https://github.com/angr/angr).
- Verificação: [Z3](https://github.com/Z3Prover/z3), [Alive2](https://github.com/AliveToolkit/alive2).
- Build: [ThinLTO](https://clang.llvm.org/docs/ThinLTO.html), [DTLTO](https://www.llvm.org/docs/DTLTO.html), [sccache](https://github.com/mozilla/sccache), [Bazel remote caching](https://docs.bazel.build/versions/main/remote-caching.html).
- Infraestrutura: [Firecracker](https://github.com/firecracker-microvm/firecracker), [PostgreSQL](https://www.postgresql.org/docs/current/sql-select.html).
- Android: [assinatura](https://developer.android.com/studio/publish/app-signing), [páginas de 16 KB](https://developer.android.com/guide/practices/page-sizes).
- Comparação com port por código disponível: [Simpsons: Hit & Run Android](https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android).

Versões e capacidade dos projetos externos devem ser reconferidas e fixadas na integração. A lista não significa que todos foram instalados ou incorporados ao repositório.

### 27.3 Glossário

| Termo | Uso neste plano |
|---|---|
| AOT | Tradução e compilação antes de executar o jogo instalado |
| JIT | Tradução de instruções durante a execução |
| EE | Processador principal do PS2 |
| IOP | Processador e ambiente de serviços de entrada/saída |
| IRX | Módulo carregável associado ao IOP |
| VU | Unidades vetoriais e seus programas |
| GS | Sistema gráfico cuja semântica precisa ser reproduzida |
| HLE | Implementação nativa de um serviço em nível mais alto |
| CFG | Grafo de fluxo de controle |
| IR | Representação intermediária de operações e efeitos |
| Overlay | Código carregado sob demanda, possivelmente sobre outra região |
| Epoch | Identidade temporal de uma versão de código/mapeamento |
| LBN/LBA | Endereço lógico usado em leitura de setores |
| ABI | Contrato binário de chamadas, dados e layout |
| PGO | Otimização guiada por perfis de execução |
| LTO | Otimização que considera múltiplas unidades durante o link |
| Cache por conteúdo | Reutilização condicionada à identidade das entradas |
| Replay | Reprodução de entradas e eventos sob condições registradas |
| Qualificação | Aprovação de uma revisão/destino por critérios e evidências |
| Autorreparo | Ciclo automático de diagnóstico, candidato, verificação e promoção |
| Universalização | Expansão das classes resolvidas até cobrir a biblioteca pretendida |

**Direção final:** transformar cada descoberta do primeiro jogo em uma capacidade geral, para que a próxima ISO exija menos trabalho até o processamento ocorrer sem intervenção. A velocidade vem da reutilização correta; a qualidade vem de semântica, execução de referência e testes que o reparador não pode manipular.


<a id="s28"></a>
## 28. Pesquisa avançada: ideias de maior potencial e experimentos de fronteira

**Escopo:** propostas adicionais para acelerar generalização e execução. Algumas combinam técnicas conhecidas de formas específicas para este pipeline. Não se afirma ineditismo mundial nem superioridade ao estado da arte sem implementação e comparação publicadas.

O valor da pesquisa será medido por classes novas resolvidas, correção preservada, custo reduzido e títulos inéditos qualificados. Uma técnica interessante que atrase o piloto sem produzir evidência deve sair do caminho crítico.

### 28.1 Rastrear o caminho dos setores até o código executado

**Ideia:** quando surgirem bytes executáveis desconhecidos na memória, seguir sua origem até a leitura do disco e a rotina que os transformou.

No laboratório, associar intervalos de memória a eventos de leitura/cópia/descompressão. Se uma região recebe execução, reconstruir o grafo de produtores: setores → buffer → transformação → região executável.

Isso pode revelar executáveis dentro de containers proprietários sem exigir que alguém descubra previamente todo o formato.

**Primeiro experimento:** fixture que lê dois segmentos, descompacta um overlay e salta para ele. O sistema deve recuperar a origem, o hash materializado e a rotina produtora.

**Aceitação:** recompilar o overlay offline e reproduzir duas rotas com entradas distintas. Depois aplicar em um caso real.

**Limites:** rastreamento byte a byte pode ser caro; começar com intervalos e refinar apenas na janela relevante. Origem observada não prova que outras entradas produzam o mesmo código.

### 28.2 Especialização AOT com guardas verificáveis

**Ideia:** gerar duas formas nativas para uma região: uma conservadora e outra especializada sob hipóteses verificadas em runtime.

Exemplos de hipóteses:

- Endereço pertence à RAM comum e não a MMIO.
- Epoch do módulo é a esperada.
- Layout e alinhamento correspondem ao contrato.
- Microprograma ativo tem hash conhecido.
- Não existe evento pendente antes da próxima fronteira.

A guarda escolhe entre implementações **já compiladas**. Se a hipótese falha, executar a forma AOT conservadora; se a própria versão de código é desconhecida, gerar diagnóstico. Não introduzir interpretação disfarçada.

**Experimento:** rotina de cópia com caso RAM e caso MMIO. Validar todos os efeitos e estados de saída.

**Risco:** sair do trecho especializado exige estado convidado consistente, inclusive flags e tempo. Guardas incompletas anulam a correção.

### 28.3 Transformar esperas em avanços de eventos comprovados

**Ideia:** reconhecer loops que apenas esperam uma condição e avançar até o próximo evento capaz de alterá-la, preservando os efeitos intermediários.

Exigir prova ou contrato de que:

- O corpo não tem efeitos relevantes além dos modelados.
- Leituras não disparam efeitos de dispositivo.
- O produtor da condição está identificado.
- Contadores e interrupções intermediárias continuam contabilizados.
- Nenhuma entrada externa relevante é ignorada.

**Experimento:** duas fixtures: espera puramente passiva e espera com leitura MMIO que tem efeito. A primeira pode acelerar; a segunda deve manter execução conservadora.

**Aplicação possível:** reduzir custo de sincronização em testes e jogo. O bloqueio atual de Monster House não deve receber essa otimização antes de sua causa ser conhecida.

### 28.4 Síntese de sequências inteiras com prova local

**Ideia:** descobrir sequências menores de operações nativas para padrões R5900/MMI, verificando equivalência por bitvectors e efeitos explícitos.

[Equality saturation em egg](https://egraphs-good.github.io/egg/egg/tutorials/_01_background/index.html) oferece uma técnica existente para explorar reescritas. [Souper](https://github.com/google/souper) é uma referência de superotimização LLVM com SMT; seu repositório está arquivado, portanto não deve ser adotado como dependência central sem avaliar manutenção.

**Primeiro escopo:** blocos curtos de inteiros sem memória, exceções ou ponto flutuante. Preservar todas as saídas vivas, não apenas o registrador final aparente.

**Verificação:** prova local, testes adversariais e medição em x86-64/ARM64. O menor número de instruções não garante menor latência.

**Limites:** equivalência entre dois modelos errados não prova semântica PS2. O modelo de entrada precisa de confirmação independente.

### 28.5 Gerar automaticamente testes a partir de efeitos observados

**Ideia:** transformar uma divergência real em uma família de fixtures, em vez de guardar apenas um snapshot enorme.

O sistema identifica:
- Registradores de entrada realmente lidos.
- Memória necessária.
- Eventos e respostas de dispositivo.
- Saídas observadas.
- Precondições do bloco ou serviço.

Em seguida, produz uma fixture reduzida e perturba valores nas fronteiras: alinhamentos, comprimentos, flags, latências e códigos de erro.

**Experimento:** erro controlado numa transferência SIF. O gerador deve produzir casos normais, vazios, atrasados e com endereços inválidos segundo o contrato.

**Critério:** detectar a correção falsa que apenas satisfaz o snapshot original.

### 28.6 Grafo de dependências das evidências

**Ideia:** fazer cada evidência declarar exatamente quais versões de semântica, componentes e dados sustentam sua validade.

Uma correção em um codec não deveria necessariamente invalidar testes de aritmética inteira. Uma mudança no scheduler pode invalidar grande parte da evidência temporal.

Usar esse grafo para selecionar regressões e reduzir o tempo de requalificação.

**Experimento:** alterações artificiais em helper isolado, registro de módulo e scheduler. Conferir o conjunto de testes invalidado em cada caso.

**Regra conservadora:** observação dinâmica incompleta não prova ausência de dependência. Aliases, chamadas indiretas e efeitos globais desconhecidos ampliam a invalidação.

### 28.7 Reutilizar objetos nativos por contrato de módulo

**Ideia:** compartilhar mais do que código-fonte gerado: objetos nativos de bibliotecas/microprogramas com identidade e contratos equivalentes.

Para isso:
- Separar endereços convidados relocáveis de código do host.
- Versionar ABI e semântica.
- Associar cada objeto às hipóteses aceitas.
- Reconhecer a mesma implementação em diferentes layouts apenas com evidência suficiente.
- Respeitar o escopo privado dos artefatos derivados.

**Experimento:** a mesma biblioteca numa fixture com duas bases de carga. Compilar uma vez, resolver endereços de forma correta e executar ambas.

**Ganho esperado a medir:** reduzir trabalho em títulos que repetem bibliotecas. Identidade aproximada sem confirmação deve produzir miss, não reutilização silenciosa.

### 28.8 Otimizar também as ferramentas que compilam o jogo

**Ideia:** coletar perfis do compilador e linker processando os arquivos típicos deste projeto, e avaliar uma toolchain otimizada para essa carga.

O LLVM documenta [PGO para a própria toolchain](https://llvm.org/docs/HowToBuildWithPGO.html) e [BOLT como otimização pós-link](https://llvm.org/docs/AdvancedBuilds.html). Essa é uma técnica existente; sua utilidade específica aqui precisa ser medida.

**Experimento:** mesmos fontes, versões e flags, comparando toolchain comum com toolchain otimizada. Usar funções pequenas, tabelas grandes e casos fora do treinamento.

**Critério econômico:** o custo de construir/manter essa toolchain deve ser amortizado pelo volume de builds. Não fazer isso antes de resolver reescrita desnecessária de fontes e registros gigantes.

### 28.9 Selecionar reparos por informação produzida

**Ideia:** escolher o próximo experimento pelo quanto ele reduz a incerteza sobre a causa, além da chance de fazer o jogo avançar.

Exemplo: antes de alterar três serviços, capturar uma fronteira que distingue erro de scheduling de erro de leitura de disco. Um teste que rejeita duas hipóteses pode economizar várias compilações.

O planejador mantém causas candidatas, custo dos experimentos e resultados anteriores. Começar com regras transparentes; só aprender uma política mais complexa quando houver histórico suficiente.

**Experimento:** conjunto de falhas injetadas com causa conhecida. Comparar número de builds, custo e precisão do diagnóstico com uma ordem fixa.

**Limite:** priorização não deve substituir evidência por probabilidade. A confirmação final continua executável.

### 28.10 Tratar combinações de patches como restrições

**Ideia:** perfis automáticos devem declarar quais hipóteses e versões exigem. Um resolvedor detecta patches incompatíveis antes da build.

Exemplo: uma alteração que supõe conclusão RPC imediata não pode coexistir silenciosamente com outra que exige resposta assíncrona observável.

**Experimento:** perfis sintéticos com conflitos de epoch, ABI e política temporal.

**Aceitação:** o sistema explica a contradição, escolhe uma combinação compatível ou rejeita a composição. Testes de integração continuam necessários mesmo sem conflitos declarados.

### 28.11 Contrato de especialistas em vez de uma única tradução gigante

**Ideia:** organizar o compilador como seleção entre implementações especializadas verificadas:
- Bloco inteiro simples.
- Memória comum.
- MMIO.
- Vetores.
- Serviço reconhecido.
- Fluxo não estruturado.
- Transferência/sincronização.

O despacho da compilação escolhe o especialista pelas propriedades provadas; a saída mantém a mesma fronteira de estado.

**Experimento:** conjunto pequeno que cruza essas classes e valida transições entre elas.

**Benefício potencial:** melhorar código e limitar o alcance de um bug. **Custo:** fronteiras excessivas podem aumentar overhead; medir coalescência segura entre especialistas.

### 28.12 Regressão metamórfica para a própria ferramenta

**Ideia:** além de comparar com uma referência, construir variações de fixtures que deveriam preservar relações conhecidas.

Exemplos:
- Relocar um módulo mantendo relocações corretas.
- Inserir padding que não é executado.
- Alterar nomes de arquivos sem mudar o mapeamento explícito.
- Reordenar funções independentes.
- Mudar base de buffers mantendo alinhamento e contrato.
- Variar limites de compilação sem alterar a semântica.

**Critério:** a ferramenta conserva os comportamentos esperados e não depende acidentalmente de ordem, endereço ou nome.

**Limite:** essas relações devem ser demonstradas na fixture. Uma alteração aparentemente neutra pode ser observável em um jogo real.

### 28.13 Prioridade das propostas

| Horizonte | Experimentos prioritários | Motivo |
|---|---|---|
| Antes de mais builds longas | Diagnóstico informativo, fixtures reduzidas, registro compacto | Reduzem custo do bloqueio atual |
| Primeiro gameplay | Rastreio de origem, contratos de módulo, waits validados | Ampliam descoberta e execução |
| Primeiro jogo completo | Grafo de evidências, reparos com restrições, metamórficos | Aumentam confiança e generalização |
| Vários títulos | Objetos reutilizáveis, especialização AOT | Amortizam código compartilhado |
| Volume alto de builds | Toolchain PGO/BOLT, síntese/equality saturation | Pesquisa de desempenho com baseline sólido |

Essas 12 propostas complementam as 12 da seção 12. Não é necessário implementar todas para entregar Monster House. O plano seleciona as que resolvem o próximo gargalo demonstrado.

<a id="s29"></a>
## 29. Depois de Monster House: mais três ISOs para generalizar

O próximo ciclo deve conter **três ISOs adicionais**, fornecidas quando disponíveis. Os títulos ainda não foram escolhidos. Selecioná-los após inspeção das características técnicas, sem presumir que qualquer grupo de três representa toda a biblioteca.

### 29.1 Papéis diferentes no conjunto

| Caso | Seleção desejada | Pergunta principal |
|---|---|---|
| ISO A — transferência | Bibliotecas ou características parcialmente compartilhadas | O que aprendemos em Monster House é reaproveitado automaticamente? |
| ISO B — diversidade | Engine, VU/IOP ou streaming diferentes | O sistema resolve classes novas sem depender do perfil do piloto? |
| ISO C — avaliação reservada | Título mantido fora do ajuste das regras | A automação funciona num caso não usado para construí-la? |

Se o inventário real não oferecer essas diferenças, registrar a limitação e escolher o melhor conjunto disponível.

Uma revisão quase idêntica é útil para testar fingerprint e relocações, mas não deve substituir toda a diversidade do ciclo.

### 29.2 Protocolo das três submissões

1. Congelar versão de compilador, runtime, reparador e critérios.
2. Apresentar a ISO sem fornecer patches manuais.
3. Executar a mesma entrada de produto.
4. Medir build, descoberta, reparos, tentativas e custo.
5. Registrar qualquer intervenção humana.
6. Validar PC e Android, gameplay, saves, mídia e contrato AOT.
7. Guardar as falhas com reprodutores.
8. Transformar soluções em capacidades gerais.
9. Reexecutar Monster House e os casos anteriores.
10. Avaliar ISO C antes de usar seus problemas para ajustar a ferramenta.

Após usar ISO C para desenvolvimento, ela deixa de ser avaliação reservada. Escolher outro caso reservado na expansão seguinte.

### 29.3 Resultado esperado desse ciclo

Produzir uma matriz:

```text
                    MH      ISO A      ISO B      ISO C
build automática
boot/menu
gameplay
campanha/protocolo
AOT EE/IOP/VU
PC aprovado
Android aprovado
reparos automáticos
intervenções humanas
tempo/custo
```

Quatro jogos aprovados demonstram evolução concreta. Não demonstram 90% da biblioteca PS2. Usar as diferenças entre eles para escolher o próximo conjunto estratificado.

### 29.4 O que significa “normalizar” o pipeline

- Mesma CLI/API para todos.
- Mesmos estados e relatórios.
- Nenhum ramo do pipeline que simplesmente pergunta “é Monster House?” para contornar arquitetura.
- Perfis selecionados por identidade e precondições.
- Novas falhas entrando no mesmo motor de reparo.
- Artefatos e caches associados a contratos consistentes.
- Regressão cruzada após cada melhoria.
- Resultado inconclusivo contado como tal.

O título piloto pode ter um perfil próprio; o mecanismo que o aplica deve ser geral.

<a id="s30"></a>
## 30. Execução imediata e compromisso de qualidade

### 30.1 O que fica concluído hoje neste trabalho de planejamento

- Documento único com auditoria, arquitetura e caminho até o primeiro jogo.
- Motor de correção autônoma como objetivo central.
- Backlog de 40 tarefas com dependências e verificações.
- 24 propostas de pesquisa/otimização, com limites e experimentos.
- Metas de poucos minutos claramente associadas aos cenários medidos.
- Protocolo de três ISOs adicionais após Monster House.
- Distinção entre protótipo atual, implementação futura e evidência de conclusão.

### 30.2 A próxima sessão de implementação

Executar nesta ordem:

1. T01: preservar e tornar fiel o estado atual.
2. T02–T05: regeneração persistente e build de diagnóstico.
3. T06: capturar o ponto real de ausência de progresso.
4. T07: reduzir o primeiro erro.
5. T08: corrigir a causa e criar detector/reparo/teste reutilizáveis.
6. Repetir o ciclo até atingir o próximo marco MH, registrando evidências.

Se o bloqueio for pequeno, o avanço pode ser rápido. Se revelar semântica, módulo ou dispositivo ausente, implementar a capacidade necessária. O plano não afirma que toda essa execução terminará no mesmo dia.

### 30.3 O que não pode ser sacrificado para cumprir uma data

Não classificar tela de boot como jogo completo; não omitir cenas; não fazer saves parecerem funcionar sem reabertura; não esconder interpretação; não aprovar um reparo alterando o teste; não anunciar poucos minutos medindo apenas a cópia de um executável sem explicar o cache.

A entrega desejada é forte: jogo completo, nativo e automático. O caminho mais rápido sustentável é diminuir o tempo de cada diagnóstico e fazer cada solução beneficiar o sistema inteiro.

**Próximo objetivo operacional:** explicar e resolver o bloqueio real de Monster House, transformando o resultado em capacidade reutilizável do recompilador autônomo.
<!-- PS2NATIVE_SOURCE_END:README_AUTOMACAO_UNIVERSAL.md -->

---

<a id="anexo-03"></a>

# ANEXO 03 — RELATORIO_PS2NATIVE_ESTADO_ATUAL.md

Origem: [RELATORIO_PS2NATIVE_ESTADO_ATUAL.md](RELATORIO_PS2NATIVE_ESTADO_ATUAL.md). Linhas originais: **582**. Bytes: **52228**. SHA-256: `cb4eceb264c56f20b5bd37391f61cb17c4f4a19ffc077cf09c474691e52b5936`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:RELATORIO_PS2NATIVE_ESTADO_ATUAL.md -->
# PS2Native — relatório completo de implementação e estado atual

**Data da auditoria:** 30 de setembro de 2026.\
**Projeto:** `/home/pedrohs/Downloads/ps2-native-recompiler`.\
**Último registro consolidado de validação consultado:** `2026-09-30T13:24:04.546896+00:00`.\
**Foco:** a ferramenta universal PS2Native; Monster House aparece como caso de desenvolvimento e validação.

Este relatório descreve o código, a documentação e as evidências locais existentes. Os resultados de testes apresentados são execuções registradas anteriormente, consultadas nesta auditoria; não são uma nova rodada de testes. As mudanças de desenvolvimento continuam na árvore de trabalho local e não equivalem a uma versão publicada.

## 1. Objetivo e conclusão atual

A meta permanece: **receber uma ISO de PS2, descobrir e recompilar o código necessário, integrar os serviços do console e entregar um executável nativo para PC ou um APK para Android, automaticamente, sem alguém corrigindo cada jogo à mão.** Os 90% são uma etapa intermediária de compatibilidade medida; a ambição final é a biblioteca inteira.

Hoje temos uma ferramenta experimental com um caminho local real de **ISO → análise → C++ gerado → compilação → pacote de PC**, além da infraestrutura de preparação e build para Android. Também temos mecanismos para registrar chamadas indiretas em lote e traduzir novos blocos EE encontrados durante a execução no Linux.

O projeto ainda não demonstrou conversão universal, um jogo completo aprovado ou o motor que descobre e corrige autonomamente qualquer falha. O código EE recompilado executa como código nativo do host; IOP e VU ainda usam interpretação em partes importantes. Gráficos, áudio, dispositivos e temporização continuam dependendo do runtime de compatibilidade.

**Estado do produto:** protótipo funcional de recompilação e empacotamento, com validação de componentes e progresso em um título real. Universalidade, autonomia completa e entrega Android validada continuam em desenvolvimento.

## 2. Quadro do que já existe

| Capacidade | Estado atual | Alcance da evidência |
|---|---|---|
| Inspecionar e extrair ISO | Implementada | ISO9660/Joliet suportados; outros layouts não estão cobertos universalmente |
| Resolver o executável inicial | Implementada | Leitura de `SYSTEM.CNF` e caminho `BOOT2` |
| Inventariar executáveis | Implementada | ELF, segmentos e hashes; deduplicação de conteúdo |
| Analisar e recompilar o EE | Implementada | R5900 → C++ → código de máquina do host, com lacunas semânticas explícitas |
| Recompilar candidatos ELF secundários | Implementada | Heurística conservadora para MIPS III `ET_EXEC`; não identifica todo código possível |
| Registrar entradas indiretas estáticas em lote | Implementada e testada | Limites de instrução decodificados nas funções EE geradas não stub |
| Traduzir blocos EE ausentes em execução | Implementada para desenvolvimento Linux | Gera C++, compila biblioteca nativa, registra e retoma; casos não suportados continuam falhando |
| Cache de blocos EE nativos | Implementado | Identidade de código e ferramentas; validação dos bytes do bloco antes de reutilizar |
| Reutilizar objetos após regeneração | Implementada | Arquivos idênticos mantêm timestamps; CMake reaproveita objetos e PCH |
| Empacotar PC | Implementado e exercitado | Pacote Linux nativo para a ABI da máquina de build |
| Preparar projeto Android por ISO | Implementado | Gradle/CMake/NDK, assets, Activity e biblioteca nativa |
| APK de jogo validado em celular | Não demonstrado | Não há aprovação de instalação, boot e campanha em ARM64 físico |
| Runtime de serviços PS2 | Implementado parcialmente | Memória, scheduling, SIF/IOP, VIF/VU/GIF/GS, arquivos, pad e memory card |
| Testar sem interferir no desktop | Implementado e verificado | Xvfb privado, capturas, entrada e áudio isolados |
| Relatar build, hashes e lacunas | Implementado | Manifests, relatórios CSV, logs e verificação de integridade |
| Diagnosticar falhas complexas | Ferramentas implementadas | Captura e replay; a investigação causal ainda foi conduzida durante o desenvolvimento |
| Corrigir qualquer falha sem agente | Planejado | Não existe ainda o ciclo autônomo completo de diagnóstico, reparo e aprovação |
| Serviço de conversão na nuvem | Planejado | Nenhum serviço operacional de upload, filas e workers foi identificado nesta auditoria |
| Biblioteca PS2 inteira / 90% | Não medido | Um título incompleto não constitui um denominador de compatibilidade |
| Conversão completa em poucos minutos | Meta | Há tempos curtos de etapas e cache; não existe benchmark universal de ponta a ponta |

## 3. Base reaproveitada e trabalho construído por cima

O PS2Native usa o **PS2Recomp** como base experimental. O ponto de partida auditado nos documentos foi o commit `75d729c`. Já havia analisador, tradutores de instruções, runtime, IOP e estrutura de builds. A base não foi criada toda do zero nesta sessão.

O trabalho local acrescentou e ampliou:

- Inspeção/extração de ISO e seleção automática do boot ELF.
- CLI e orquestrador `ps2native`, workspaces por ISO e publicação de pacotes.
- Integração dos fontes gerados ao runner, substituindo a tabela vazia de exemplo.
- Inventário e recompilação de candidatos EE secundários, com registros por módulo.
- Preparação Android por título, inclusão dos dados e inicialização em armazenamento privado.
- Manifests, hashes de artefatos e relatórios de cobertura com limites explícitos.
- Registro de entradas internas e chamadas indiretas em lote.
- Registro compacto, regeneração incremental e perfil de build sem LTO.
- Tradução nativa de blocos EE materializados em RAM e cache Linux.
- Correções genéricas de instruções, ABIs, serviços e transferências encontradas com Monster House.
- Capturas, fixtures nativas, replay VU, benchmarks e execução headless isolada.

As propostas de IR independente do host, compilação nativa completa de IOP/VU, backend gráfico acelerado completo e motor autônomo permanecem objetivos. Uma seção de arquitetura em um documento não prova que seu componente já foi implementado.

## 4. Arquitetura que executa hoje

```mermaid
flowchart TD
    ISO[ISO local de PS2] --> IN[ps2iso-inspect: boot, inventário e extração]
    IN --> PIPE[ps2native: workspace, ferramentas e manifest]
    PIPE --> AN[ps2_analyzer: ELF para TOML]
    AN --> RC[ps2_recomp: R5900 para C++]
    RC --> CPP[Fontes gerados e tabela de despacho]
    CPP --> PC[CMake e compilador do host]
    CPP --> AND[Projeto Android: Gradle e NDK]
    PC --> RUN[Runner nativo de PC]
    AND --> APK[APK: saída condicionada ao toolchain; ainda sem validação de jogo]
    IN --> DATA[Arquivos e boot ELF extraídos]
    DATA --> RUN
    DATA --> AND
    RUN --> RT[Runtime compartilhado de compatibilidade PS2]
    RT --> EE[Execução de funções EE nativas]
    RT --> DEV[Memória, serviços, dispositivos, IOP, VU e GS]
    EE --> MISS[Alvo EE sem tradução conhecida]
    MISS --> OV[Driver Linux: RAM para C++ e biblioteca nativa]
    OV --> EE
```

| Módulo | Responsabilidade atual |
|---|---|
| `tools/iso_inspect/` | Ler a imagem, resolver boot, inventariar ELF e extrair o disco |
| `tools/ps2native/` | CLI, subprocessos, workspaces, manifests, pacotes, overlays e testes headless |
| `ps2xAnalyzer/` | Análise do ELF e emissão da configuração TOML |
| `ps2xRecomp/` | Decodificação R5900, descoberta de entradas e emissão de C++ |
| `ps2xRuntime/` | Contexto EE, memória, despacho, serviços e integração de dispositivos |
| `ps2xIOP/` | Núcleo R3000A interpretado, módulos IRX, kernel e serviços IOP |
| `android/` | Projeto Gradle, wrapper CMake e preparação dos dados antes do native startup |
| `ps2xTest/` | Regressões C++ de compilador, runtime e subsistemas |
| `docs/` | Evidências, limites e procedimentos |

O caminho atual emite C++20 e usa um compilador C++ para gerar código da arquitetura de destino. Não temos hoje um novo backend próprio completo que emita diretamente x86-64/ARM64 a partir de uma IR independente do host.

## 5. Processo automático da ISO ao pacote

### 5.1 Entrada e identificação

1. A CLI recebe a ISO e descobre inspector, analyzer, recompiler e ferramentas de build por argumentos, variáveis de ambiente, `PATH` e diretórios locais conhecidos.
2. O inspector retorna JSON de contrato schema-v1, identifica o filesystem e lê `SYSTEM.CNF`.
3. O pipeline registra SHA-256 da ISO e do boot ELF, caminhos e inventário dos executáveis.
4. Um workspace exclusivo é criado em `<título>-<hash-da-ISO>/run-*`; dados e fontes gerados ficam separados do código compartilhado.
5. O disco é extraído e os hashes relevantes são conferidos. A árvore extraída é preservada como fonte de dados para os serviços de disco/arquivos.

### 5.2 Análise e recompilação

6. O analisador transforma o boot ELF em configuração TOML.
7. O recompilador decodifica funções/entradas e emite fontes C++, declarações, stubs, registro de despacho e relatórios.
8. Candidatos secundários ELF32 little-endian MIPS `ET_EXEC` com flags MIPS III passam por invocações separadas e recebem namespaces e registros próprios.
9. Falhas de candidatos secundários são retidas no manifest; outros candidatos continuam sendo processados. Uma flag MIPS III é uma heurística de classificação, não prova definitiva de que o módulo pertence ao EE.
10. Normalizações locais de compatibilidade do C++ gerado são aplicadas quando necessárias, sem alterar manualmente a estrutura compartilhada para cada título.

### 5.3 Compilação e entrega

11. Para PC, um wrapper CMake conecta os fontes gerados ao runtime e compila o runner para a ABI do host. Para Android, o pipeline prepara um projeto isolado e solicita o build ARM64 ao Gradle/NDK quando seus pré-requisitos existem.
12. O pacote recebe código nativo, dados do disco, boot ELF, configuração de execução e manifest. A publicação em destino novo usa staging e rename atômico sem sobrescrever um pacote existente.

O verificador de pacote detecta arquivos alterados, ausentes ou inesperados em relação ao inventário. Logs e saves declarados como saídas mutáveis recebem tratamento próprio. **Essa verificação mede integridade dos arquivos, não qualidade gráfica ou possibilidade de terminar o jogo.** Os hashes também não constituem assinatura de procedência.

Uma ISO não é apenas o executável inicial. Ela pode conter módulos EE, IRX/IOP, microprogramas VU, código carregado de arquivos, scripts e todos os dados audiovisuais. Universalizar a entrada exige acompanhar também o código e os serviços usados depois do boot.

## 6. O que a recompilação produz

A análise combina metadados ELF, símbolos quando presentes, informações de debug/mapas, alvos J/JAL, endereços materializados, tabelas de ponteiros e heurísticas de funções. Depois vêm a decodificação R5900, análise de controle e tradutores das famílias integer, COP0, FPU, MMI e operações relacionadas a VU.

O C++ mantém explicitamente contexto de registradores, PC e memória convidada. Chamadas para serviços, MMIO, scheduling e gráficos entram no runtime. O compilador do host transforma esse C++ em instruções nativas.

Isso produz uma **tradução semântica executável**, não recupera automaticamente o projeto original do estúdio, seus nomes, comentários, organização da engine ou arquivos-fonte. Tornar a ferramenta aberta e gerar C++ de um binário são operações diferentes; os dados e o programa derivados da ISO continuam sendo os insumos e resultados daquele jogo.

Instruções não implementadas podem resultar em diagnóstico e exceção no C++ gerado. Por isso um arquivo C++ compilável pode conter caminhos que falharão somente quando forem executados. O manifest precisa preservar esse estado parcial.

## 7. Chamadas indiretas: mudança estrutural já feita

O método anterior de adicionar um endereço ao TOML, compilar o jogo inteiro e esperar o próximo endereço ausente foi substituído, para o código estático decodificado, por **registro em lote dos limites de instrução**.

- Cada limite decodificado dentro das funções EE geradas não stub pode receber entrada de retomada.
- O wrapper seleciona o ponto correto a partir de `ctx->pc`.
- Sobreposições usam o dono decodificado mais específico; os inícios das funções mantêm suas entradas.
- Entrada independente numa instrução de delay slot executa essa instrução isoladamente e devolve o próximo PC ao dispatcher. Na execução normal do branch, o delay slot continua executando uma vez.
- Branch dentro de delay slot entrado independentemente continua sendo um caso explicitamente não suportado.
- Endereços não decodificados ou sem tradução continuam aparecendo como lacuna, em vez de serem declarados cobertos.

**Medições do caso Monster House:**

| Medida | Resultado registrado |
|---|---:|
| Limites internos de instrução decodificados | 698.143 |
| Entradas de retomada | 699.283 |
| Bindings totais do despacho | 705.383 |
| Inícios de stubs | 191 |
| Bindings incorretos apontando para stub no teste registrado | 0 |

Os endereços `0x176b10`, `0x1639e0` e `0x177350` passaram a ser emitidos sem suas três entradas manuais no TOML. **705.383 bindings não significam 705.383 funções:** vários endereços compartilham a mesma função nativa e entram em labels diferentes.

## 8. Tradução nativa de código EE encontrado em RAM

Existe um segundo mecanismo para alvos EE ausentes no desenvolvimento Linux:

```text
Alvo EE sem tradução disponível
  → capturar os bytes atuais da RAM
  → descobrir blocos dentro de orçamento limitado
  → gerar C++ para esses blocos
  → compilar biblioteca nativa .so
  → carregar e registrar as entradas
  → retomar execução nativa
```

O driver é `tools/ps2native/native_overlay_driver.py`, integrado ao runtime por `PS2X_NATIVE_OVERLAY_DRIVER`. Ele usa compilador do host com `-O0 -fno-lto`; não exige reconstruir o runner inteiro ou cadastrar individualmente o endereço no TOML.

Snapshots normalmente têm 64 KiB, com janela sobreposta para fronteiras que atravessam um branch e seu delay slot. Blocos têm limite de 128 instruções e existem orçamentos de quantidade de blocos/instruções. O prefetch é especulativo e não garante descobrir todos os endereços.

O cache incorpora bytes de código, endereço de entrada, ferramentas, fonte do driver e headers do runtime. Antes de reutilizar uma tradução, o runtime confere os bytes do bloco que a originou. Traduções que falharam podem ser tentadas novamente quando o snapshot mudar, incluindo alterações além dos primeiros 16 bytes.

**Limites atuais:** Linux; dependência de compilador local; casos de instrução/controle não suportados continuam sendo erro. Não existe o mesmo driver pronto para Android, Windows ou macOS, nem acompanhamento universal completo de toda escrita em páginas de código EE.

Esse mecanismo é **compilação nativa durante a execução**, não AOT estrito de todo o programa antes de iniciar. Para a meta de pacote totalmente AOT, uma evolução necessária é usar essas descobertas na validação, congelar as versões de código e incorporá-las ao build final, com tratamento explícito para versões realmente novas.

Não foi implementado um microinterpretador EE universal que execute qualquer endereço ausente até achar código conhecido. A interpretação que existe em IOP/VU é um subsistema distinto.

## 9. Compilação incremental e redução do tempo de tentativa

### 9.1 Perfil de desenvolvimento

- `PS2X_FAST_ITERATION=ON`: fontes normais do jogo em `-O1 -fno-lto` no caminho GNU/Clang relevante.
- `PS2X_ENABLE_RELEASE_IPO=OFF`: elimina LTO do loop de desenvolvimento.
- PCH do runner reaproveita os headers efetivos daquele título.
- Registro de funções em `-O0`, sem PCH incompatível.
- Fontes gerados maiores que 4 MiB usam `-O0` no perfil rápido para limitar o custo do otimizador.
- Os três fontes privados do VU podem usar `-O2`; o restante do jogo permanece em `-O1`, sem LTO.

### 9.2 Registro compacto

Endereços consecutivos que compartilham wrapper são emitidos como intervalos. O registro expande esses intervalos em execução, preservando buracos e donos distintos. Não removemos entradas para conseguir o ganho.

| Fonte gerado de registro | Bytes |
|---|---:|
| Antes | 136.045.196 |
| Depois | 2.253.899 |

Isso representa cerca de **60 vezes menos texto C++ de registro**, aproximadamente 98,34% de redução. O número de bindings executáveis permanece preservado.

### 9.3 Regeneração que não invalida objetos sem necessidade

O gerador compara os conteúdos antes de gravar fontes e headers. Arquivos idênticos mantêm timestamps; apenas os alterados precisam voltar ao compilador.

Uma repetição de geração levou **5,66 segundos** e preservou os timestamps de **7.244 arquivos C++/headers**, com zero arquivos alterados, sob compilação concorrente. Esse conjunto de geração não é o mesmo inventário de fontes retido no manifest inicial.

Exemplos registrados de regeneração seletiva: correções MMI alteraram 12 fontes; a correção SQRT/RSQRT alterou 138. Os outros objetos puderam ser reutilizados. Alterações em headers públicos ou flags compartilhadas ainda podem invalidar mais objetos.

**Tempos curtos registrados são de etapas específicas:** relink de mudanças restritas ao runtime na ordem de 10 segundos; novo lote de overlay na ordem de 5–10 segundos; bibliotecas de overlay já em cache em cerca de 67–81 ms. Isso não é medição de conversão completa de uma ISO nova.

## 10. Correções genéricas produzidas com Monster House

Monster House tem servido para revelar erros das camadas compartilhadas. A intenção das correções abaixo é melhorar capacidades usadas por muitos títulos; sua generalização para a biblioteca precisa ser medida em outros jogos.

| Camada | Mudança feita | Efeito e limite |
|---|---|---|
| MMI/R5900 | Produtos halfword e acumuladores HI/LO; modos PMFHL, PMTHL e movimentos de 128 bits; interleaving, saturação e ordem de lanes | 21 variantes executadas contra oráculos escalares; não certifica toda a ISA MMI |
| FPU/R5900 | `SQRT.S` lê FT; `RSQRT.S` calcula FS / sqrt(FT) | Corrige erro de operandos compartilhado pelo gerador estático e overlay; flags/exceções/rounding completo continuam fora desse teste |
| COP0/clock | Count compartilhado acompanha o relógio virtual EE, trocas de thread, callbacks, writes, wrap e idle/VSync | O vídeo deixa de repetir indefinidamente o primeiro frame; não estabelece timing cycle-exact nem todos os Compare interrupts |
| ABI libm | 13 funções double passam a ler argumentos de 64 bits e devolver resultado double em V0 | Corrige cadeia de projeção/câmera que gerava FOV zero; não prova todas as variantes de toolchain PS2 ou igualdade bit a bit da libm |
| LOADFILE/IOP | Carregamento por buffer lê RAM física IOP; argumentos IRX recebem argc/argv corretos | Evita uso do espaço de endereço ou formato de argumento errado |
| IOP/kernel | Alarmes preservam GP/userdata, deadline de 64 bits, repetição e cancelamento | Regressões de callbacks e scheduling; kernel completo ainda exige mais cobertura |
| IOP/sysclib | `_wmemcopy`/`_wmemset` tratam tamanho em bytes e preservam tails | Evita corromper alocações adjacentes |
| IOP/heap | Reutilização de espaços liberados, gaps, validação de overflow/alinhamento e maior espaço livre | Melhora estabilidade de alocação; não elimina todos os limites de memória |
| IOP/IOMAN | VFS de leitura: open/close/read limitado/seek assinado/EOF/reset | Escrita e dispositivos personalizados continuam sem cobertura geral |
| SPU AutoDMA | Temporização por frame PCM estéreo de 32 bits: 768 clocks IOP | Corrige callbacks de refill excessivamente rápidos; áudio audível completo não foi aprovado |
| SIF/RPC | Adaptador de assinatura SDK, tabelas de comando visíveis na memória convidada e dispatch respeitando writes diretos | Preserva handler e contexto GP; não implementa todos os serviços RPC |
| VIF/GIF/DMA | DIRECT e IMAGE continuam entre segmentos; palavras TTE não são tratadas como pixels | Regressões de transferências; todos os modos/fragmentações ainda não estão certificados |
| VIF1 | Retenção de headers/payloads parciais STMASK/STROW/STCOL/MPG/UNPACK | Stream fragmentado reproduz o completo nos casos testados; VIF0 e timing parcial permanecem separados |
| VIF UNPACK | V2 escreve X,Y,X,Y; V4-5 expande RGB5 e alpha corretamente e mantém bypass de STMOD | Corrige formatos observados; V3 e alguns modos ROW/STCYCL ainda requerem investigação |
| VU/VI | Branch após cadeia de writes conserva o valor anterior à cadeia inteira | Um microprograma capturado deixa de esgotar 65.536 ciclos |
| VU/LOI/MIN/MAX | Preservação dos bits crus dos operandos e campos de GIF | Evita zerar valores 1, 6 e 14 usados em tags gráficas |
| Pad/ABI | Byte baixo dos botões no offset 2, alto no 3, conforme consumidor SDK | Corrige troca Start/R1; há testes de bytes e verificações reais anteriores |
| Pad/teclado | WASD → analógico esquerdo; setas → D-pad; opostos neutralizam o eixo | Entrega de pacote foi verificada; pacote correto não demonstra sozinho movimento do personagem |
| Memory card | Consultas de diretório exatas distinguem entrada de seus filhos/wildcards | Criação e leitura de save inicial observadas; campanha e saves arbitrários não aprovados |

Na libm foram corrigidas `sqrt`, `sin`, `cos`, `tan`, `atan`, `atan2`, `pow`, `exp`, `log`, `log10`, `ceil`, `floor` e `fabs`. Os testes preservam as variantes float relevantes enquanto verificam o ABI double.

Uma investigação anterior dos botões assumiu ordem de bytes errada a partir de um consumidor de menu. O consumidor de gameplay e as regressões SDK corrigiram essa conclusão. O relatório registra a conclusão atual, sem contar a tentativa anterior como uma capacidade válida.

## 11. Como Monster House foi usado no processo

O ciclo concreto de desenvolvimento foi:

1. **Inspecionar a ISO e recompilar o boot:** confirmar o executável e conectar o C++ gerado a um runner de verdade.
2. **Sair da correção por endereço:** registrar entradas internas em lote e adicionar compilação nativa de blocos em RAM.
3. **Passar do boot para os vídeos/menu:** corrigir MMI, Count, serviços IOP e transferências necessárias.
4. **Exercitar estado persistente e controles:** observar criação/leitura do save inicial e corrigir ABI de pad e diretórios de memory card.
5. **Investigar travamento em código de colisão:** identificar operandos errados de SQRT/RSQRT, reproduzir em fixture nativa e regenerar somente os fontes afetados.
6. **Investigar os pacotes gráficos:** capturar VIF/VU/GIF, reproduzir microprograma fora do boot e corrigir cadeia VI e preservação de bits.
7. **Investigar cena preta:** seguir coordenadas inválidas até a cadeia de conversão double/float da câmera; corrigir o ABI libm e confirmar em build normal novo.
8. **Medir antes de otimizar:** capturar programas VU reais, aplicar mudanças locais e comparar estado/dados e tempo em replays alternados.
9. **Isolar testes da máquina do usuário:** criar harness headless privado para screenshots e entrada sem usar o desktop.

Esse caminho gerou componentes reutilizáveis, testes e dados de diagnóstico. **A investigação causal e a implementação dessas correções ainda foram trabalho de desenvolvimento; o PS2Native não fez todo esse ciclo sozinho.**

### 11.1 Inventário e relatório inicial de tradução

A ISO consultada contém **12 conteúdos ELF únicos** no inventário: um boot ELF e 11 outros MIPS ELF. Nesse caso não foram encontrados candidatos secundários EE elegíveis pela heurística atual. Os demais não foram todos convertidos AOT; entram no inventário e nas rotas IOP/runtime aplicáveis.

O snapshot inicial retido no manifest registra:

| Contador | Valor |
|---|---:|
| Funções descobertas/processadas | 7.224 |
| Funções recompiladas | 7.033 |
| Funções stubbed | 191 |
| Funções geradas reportadas | 7.233 |
| Falhas de decode reportadas | 0 |
| Instruções não tratadas/erros reportados | 13.706 |
| Warnings/promotions reportados | 2.280 |

**Esses são contadores do relatório inicial armazenado.** Houve correções e regenerações posteriores; não devem ser apresentados como 13.706 falhas ativas atuais ou como auditoria semântica completa da versão final. O assessment estático retido continua `known_gaps`/`partial`, e não há uma certificação global atualizada que limpe todas as famílias e caminhos.

O ledger inicial atribui intervalos de funções sobre 2.885.888 bytes executáveis de arquivo, registra 296.320 bytes de zero-fill, sobreposições e oito bytes não atribuídos. Intervalos são heurísticos: um intervalo atribuído pode conter dados e tradução incorreta. Esses números não estabelecem 99,99% de compatibilidade.

### 11.2 Resultado observado, resumido

Boot, avanço de vídeos, menu, criação/leitura/carregamento do save inicial e chegada anterior a HUD/tutorial do primeiro capítulo têm evidências registradas. Um build normal corrigido também mostra um interior 3D avançando, com piso, paredes, escadas e três personagens animados, sem redirects de função, restauração de câmera ou omissão de primitivas pelo debugger.

Há defeitos de renderização dos personagens e não foi demonstrado controle causal de movimento nessa cena mais recente. Campanha completa, áudio audível correto, saves arbitrários, Android e 60 FPS permanecem sem aprovação. Monster House ainda é **caso de validação incompleto**, não um port completo certificado.

## 12. Quanto do processo já é automático

| Tipo de automação | Já temos | Ainda falta |
|---|---|---|
| Preparação e build | Inspecionar, extrair, selecionar boot, analisar, emitir C++, compilar, empacotar e registrar resultados | Universalizar formatos e tornar todo o fluxo reproduzível em todas as plataformas |
| Descoberta de entradas | Heurísticas estáticas, registro interno em lote e compilação de alvos EE novos em RAM no Linux | Código comprimido/relocado, formatos diversos, invalidação completa e inclusão final de todas as versões |
| Diagnóstico | Logs, estado, capturas EE/VU/GS, replay, fixtures e snapshots | Classificação causal automática confiável, identificação sistemática da primeira divergência |
| Correção sem pessoa | Mecanismos específicos resolvem algumas classes automaticamente, como certos alvos EE ausentes | Motor geral que escolha um reparo, gere regressão, implemente, valide, rejeite regressões e tente novamente |
| Validação de jogo | Checkpoints, screenshots e resultados locais registrados | Exploração automática de campanha, saves, áudio, controles e dispositivos com oráculos adequados |

O motor autônomo do plano precisa fechar o ciclo:

```text
Receber ISO nova
  → construir e observar
  → classificar falha
  → reproduzir a menor divergência útil
  → propor correção de compilador/runtime
  → provar a correção e executar regressões
  → incorporar apenas reparos aprovados
  → repetir dentro de orçamento
  → entregar pacote aprovado ou diagnóstico explícito
```

Esse ciclo completo **não está implementado**. Capturar uma falha automaticamente ajuda a investigar; não significa que a ferramenta já saiba consertá-la. Traduzir um endereço novo resolve uma classe de problema, mas não corrige automaticamente um ABI double errado, uma regra VIF incorreta ou um efeito GS ausente.

Uma meta central de universalização é transformar as correções encontradas no primeiro jogo em capacidades genéricas, acompanhadas de regressões, em vez de acumular endereços hardcoded e patches escolhidos manualmente por título.

## 13. Fronteira atual entre nativo, interpretação e dispositivos

| Parte | Implementação atual | Consequência para nossa meta |
|---|---|---|
| EE estático traduzido | C++ compilado para código nativo | Parte efetiva do recompiler já existe |
| Novos blocos EE no Linux | C++ compilado para `.so` durante execução | Continua execução nativa, mas não é AOT estrito prévio |
| IOP / IRX | Núcleo R3000A interpretado e serviços HLE selecionados | Ainda falta tradução nativa completa do código IOP para o contrato final desejado |
| Microprogramas VU | Cores interpretados, com correções e otimizações | Ainda falta estratégia nativa/AOT de VU e validação das pipelines |
| Serviços SDK/sistema | Implementações nativas no runtime, incluindo stubs/HLE | Precisam reproduzir corretamente comportamento e ABI, não apenas retornar sucesso |
| GS | Rasterização principalmente em software na CPU | Backend acelerado completo e fiel permanece trabalho importante |
| Apresentação da imagem | OpenGL/raylib; no Xvfb consultado, llvmpipe | Mostrar textura via OpenGL não implica rasterização GS toda na GPU |

Mesmo um port com todo código convidado compilado precisa implementar o comportamento dos dispositivos e serviços do PS2. Isso pode ser feito em código nativo e com aceleração do host. A parte ainda distante da exigência de recompilar tudo é a execução interpretada de programas IOP/VU e o código EE ainda desconhecido.

O documento inicial `PROJECT_SPEC.md` admite fallback seletivo; o plano universal posterior descreve o objetivo mais estrito. Esta auditoria registra a implementação real. A remoção do código convidado interpretado exige marcos próprios e evidência; não se resolve trocando o nome do pacote para “native”.

## 14. Android e PC: o que está conectado e o que falta

### 14.1 PC

O pacote desktop contém `bin/ps2EntryRunner`, a árvore `game/`, boot ELF de referência, manifest e wrapper de execução. O CMake recebe `PS2X_GENERATED_CODE_DIR`, inclui os fontes daquele jogo e exclui o `register_functions.cpp` vazio de exemplo.

O caminho efetivamente exercitado nesta máquina é **Linux x86-64**. O alvo `desktop` usa a ABI do host; não é um serviço pronto de cross-compilation automática para todos os sistemas. A existência de templates ou CI não certifica o jogo no Windows/macOS, e o driver de overlay vivo é Linux apenas.

### 14.2 Android

A infraestrutura implementada:

- Cria um projeto por ISO sem sobrescrever o projeto Android compartilhado.
- Passa os fontes C++ gerados para o wrapper CMake do Android.
- Solicita biblioteca nativa e APK `arm64-v8a`.
- Deriva a identidade de aplicação do hash da ISO.
- Inclui o disco extraído em `app/src/main/assets/game/`.
- `Ps2PackageActivity` copia dados para `getFilesDir()/game/` antes do native startup.
- O runtime resolve `ANativeActivity::internalDataPath/game/boot.elf`.
- Registra bloqueios de ferramentas e projeto staged; não declara APK quando o build não ocorreu.

Pré-requisitos documentados: JDK 17, Gradle 8.7+, SDK platform 34, NDK `28.2.13676358` e Android CMake `3.22.1`. O projeto usa min SDK 28. Há propriedades do wrapper Gradle, mas não um wrapper completo executável/JAR; o builder aceita Gradle instalado ou distribuição em cache.

**O que não foi demonstrado:** APK real deste caso, execução ARM64 física, paridade das operações SIMD, integração audiovisual completa, campanha e desempenho no celular. O caminho ARM usa `sse2neon`, que precisa de validação semântica própria. Controles touch continuam incompletos.

O driver atual de overlays depende de compilador Linux e de ABI/bibliotecas do host; seu cache não pode ser transplantado como código executável para ARM64. Para o APK final, precisamos incorporar traduções descobertas previamente ou fornecer um mecanismo nativo apropriado ao destino.

Dados de disco embutidos podem criar APKs enormes, com cópia adicional para armazenamento privado. Split/OBB ou outra entrega de dados grandes ainda não está implementada. O build de protótipo usa assinatura debug; um pacote distribuível exige identidade de assinatura apropriada. FFmpeg fica desabilitado por padrão no Android, portanto caminhos de vídeo dependentes dessa integração também precisam ser fechados.

## 15. Testes e evidências que já temos

| Conjunto | Resultado registrado | O que demonstra |
|---|---:|---|
| Suíte C++ principal | 484/484 | Regressões de compilador/runtime/subsistemas cobertos |
| Mesma suíte ligada ao runtime desktop real com VU `-O2` | 484/484 | Mudança de otimização privada não quebrou esses testes |
| Pipeline Python | 18/18 | Casos de CLI, staging/publicação, inventário e isolamento cobertos |
| IOP | 4/4 executáveis/grupos | Importações, compatibilidade e comportamento testado do IOP |
| Fixture nativa de despacho | 4 caminhos; 16 bindings | C++ emitido compila e retoma nas entradas do fixture |
| Fixture nativa MMI | 21 variantes × 512 = 10.752 execuções | Semântica coberta comparada a oráculos escalares |
| Fixture nativa FPU | 7 variantes × 512 = 3.584 execuções | Operand/alias de SQRT/RSQRT nos casos testados |
| ABI libm double | 66 chamadas unárias, 10 binárias e 4 round trips de câmera | Argumento/retorno double e preservação float testados |
| Replay VU de desempenho | 54.000 replays sobre 3 capturas | Estado/dados iguais antes/depois e ganho local medido |
| Harness headless | 5 verificações CLI e 4 de schema | Isolamento e rejeição de sessões inválidas nos casos registrados |

O histórico contém falhas reproduzidas antes das correções e resultados verdes depois, por exemplo em pad SDK, formatos VIF, analógico de teclado e ABI double. Isso ajuda a mostrar que a regressão detecta o erro original.

**A quantidade de testes não é um percentual de títulos compatíveis.** Os conjuntos têm escopos diferentes e não devem ser somados como se cada execução cobrisse um pedaço independente de toda a biblioteca. Também não são prova de todos os modos de exceção, arredondamento, DMA, timing ou renderização.

## 16. Desempenho: resultados medidos e limites

### 16.1 VU

Uma amostragem pequena de stacks apontou VU como alvo de otimização: 20 de 24 interrupções estavam ali, duas no GS software e duas no scan de ready threads IOP. Essa amostra não é uma medição uniforme de tempo de CPU e não estabelece “83% do custo”.

Foram feitas três mudanças:

1. Visitar somente os bits ativos das máscaras de dependência VI, preservando ordem e latência.
2. Normalizar Q/I no cálculo FMAC apenas quando a instrução os usa.
3. Compilar somente três fontes privados VU em `-O2`, mantendo jogo em `-O1` e sem LTO.

Trials alternados antes/depois: três capturas MSCAL reais, três trials, 3.000 replays por variante por trial; **54.000 replays ao todo**. Todas as capturas tiveram hashes de estado e dados VU iguais entre variantes.

| Captura | Mediana anterior | Mediana otimizada | Redução de tempo |
|---|---:|---:|---:|
| Interior, 1.650 ciclos | 433,466 µs | 290,029 µs | 33,09% |
| Cena anterior, 4.873 ciclos | 1.286,710 µs | 872,148 µs | 32,22% |
| Continuação diagnóstica, 2.062 ciclos | 485,227 µs | 334,043 µs | 31,16% |

Essas medições são de **execução VU aquecida em replay**, não de frame completo, compilação fria, VRAM final ou campanha. Os hashes mostram equivalência entre as versões nesses inputs; não provam por si sós equivalência ao hardware PS2 em todos os casos.

A mudança de flags compartilhadas agendou 48 compilações runtime/utilitários, mas zero compilações de fontes gerados do jogo/PCH naquele rebuild. Mudanças posteriores restritas a fontes privados mantêm o caminho curto de relink.

### 16.2 Taxa de apresentação observada

Uma amostra de 15,006 segundos no interior 3D contou 23 uploads de apresentação: aproximadamente **1,53 uploads por segundo** no Xvfb com llvmpipe. É uma taxa ruim, explicitamente mantida no registro. Não é benchmark da GPU física nem prova de gameplay controlável, mas impede apresentar o ganho local VU como “jogo a 60 FPS”.

### 16.3 Conversão em minutos e GPU

Já reduzimos custos desnecessários: LTO no loop de desenvolvimento, registro C++ gigantesco, gravação idêntica de fontes, rebuild de todos os objetos e cadastro individual de entradas. Cache aquecido e compilação incremental têm ganhos comprovados em etapas.

Ainda não temos uma medição de conversão fria completa que entregue um jogo aprovado em dez minutos. Extrair dados, descobrir código, traduzir, compilar, validar todos os caminhos e corrigir uma incompatibilidade desconhecida são custos diferentes.

No projeto atual, parsing, emissão C++ e compilação/link são trabalhos do toolchain no CPU. A GPU pode ajudar fortemente onde existir backend de gráficos/compute adequado; VRAM adicional não fornece automaticamente semântica R5900/IOP/VU, não corrige falhas e não substitui o compilador C++ existente. Experimentos de aceleração precisam de implementação e medição próprias.

## 17. Diagnóstico, replay e headless

Há mecanismos de observação sem recompilar milhares de fontes apenas para mudar verbosidade: controles de tracing, captura de cenas e marcadores para solicitar inputs VU.

Uma captura MSCAL guarda microcode, dados, estado de entrada/saída e um ring limitado das últimas 512 duplas de instrução. O replay pode reproduzir o microprograma sem dar boot no jogo ou abrir uma janela. Saída de sucesso exige terminação normal registrada; esgotar orçamento de ciclos não é contado como sucesso.

O estado cru capturado depende da ABI local do host; registros de trace têm 664 bytes nessa máquina. Isso não é formato portátil de save state. Pipelines ocultas de MSCNT não estão completamente serializadas. O benchmark reseta o interpreter entre execuções independentes para não acumular o relógio absoluto e falsear a medição.

O harness `headless_native_test.py` usa Xvfb em display livre de número 90 ou superior, Xauthority privado e identificação do processo dono. Screenshots e comandos de teclado são direcionados somente a essa sessão validada; sessões do desktop são rejeitadas. Key-up executa mesmo após interrupção. O áudio do runner vai para um sink temporário silencioso, sem mudar a saída padrão do usuário.

Esse isolamento permite desenvolver e observar o runtime sem movimentar o mouse, focar janelas ou capturar o desktop. Ele não resolve automaticamente equivalência de imagem, controle causal do personagem ou aprovação de áudio audível.

## 18. Manifests e significado dos estados

O pacote consultado traz:

```json
{
  "status": "complete",
  "support_tier": "experimental",
  "gameplay_compatibility": "native_chapter_one_tutorial_observed_rendering_incomplete",
  "native_translation_assessment": {
    "status": "known_gaps"
  }
}
```

**`complete` significa que aquele pacote experimental foi construído.** O campo `artifact_completion_scope` esclarece expressamente que isso não certifica um jogo completo ou jogável.

O resumo estático interno ainda conserva `gameplay_compatibility: unverified`; as observações reais posteriores estão no estado geral de compatibilidade e no registro de validação. São escopos distintos e relatórios de momentos distintos. Uma futura consolidação precisa manter essa proveniência, sem interpretar nenhum desses campos como aprovação de campanha.

Os manifests guardam identidade da entrada, ferramentas, fontes, etapas, erros, listas de artefatos, tamanhos e SHA-256. `function_coverage.csv` e `address_coverage.csv` ajudam a localizar intervalos e lacunas, mas não certificam toda instrução, código dinâmico ou serviço de dispositivo.

## 19. Plano, pesquisas e componentes ainda não entregues

O plano mestre `README_AUTOMACAO_UNIVERSAL.md` tem **2.756 linhas na versão consultada**, 30 seções, backlog e propostas experimentais. Ele cobre automação, diagnóstico, nuvem, cache, compilação rápida, primeiro jogo e generalização com mais três ISOs.

Entre as direções descritas estão descoberta AOT orientada por execução, catálogo de versões de código materializadas, primeira divergência causal, especialização VU verificada, reutilização por contrato de módulo, planejamento de build por custo e reparos validados por execução. São **propostas de pesquisa e engenharia**; não há demonstração de que todas estejam implementadas ou de que sejam inéditas na comunidade.

Já existem elementos relacionados a algumas dessas direções, como registro compacto, cache de código EE e replay de microprogramas. Isso não equivale a realizar automaticamente o desenho inteiro do plano.

Não foi identificado nesta árvore um serviço operacional de conversão com upload, autenticação, armazenamento, filas, workers, monitoramento de jobs e download de artefatos. Também não existe demonstração do motor geral de reparos autônomos. A nuvem poderá distribuir trabalho; não garante que uma tradução incorreta se torne correta.

## 20. Lacunas que bloqueiam a universalidade

| Lacuna | Por que importa | Evidência necessária para fechar |
|---|---|---|
| Identificar todo código relevante | Jogos usam módulos, arquivos próprios, overlays, relocação e materialização em RAM | Corpus com inventário e rastros mostrando que entradas executadas foram traduzidas ou explicitamente diagnosticadas |
| Semântica completa do EE | Uma instrução ou ABI errada pode corromper estado sem crash imediato | Testes diferenciais por família, flags, aliases, delay slots, memória e exceções |
| Ciclo de vida do código | Mesmos endereços podem receber novas versões | Rastreamento de load/write/unload, identidade e invalidação de traduções antigas |
| IOP nativo | IRX e R3000A ainda dependem de interpretação e imports parciais | Loader/relocações/imports corretos e execuções nativas verificadas |
| VU nativo e pipelines | Microprogramas e latências podem determinar geometria e GIF | Tradução/especialização com oráculo de estado, dados, flags, término e pipelines |
| GS fiel e rápido | Apresentação OpenGL não cobre todos os efeitos/rasterização do PS2 | Comparação de frames/VRAM e desempenho em backend acelerado real |
| DMA/VIF/GIF/SIF | Fragmentação, máscaras e ordenação alteram dados e sincronização | Fixtures de modos restantes e rastros de títulos distintos |
| Áudio/SPU2/CDVD/IPU e timing | Boot e imagem não provam som, sincronismo ou leituras corretas | Comparações de áudio/vídeo, eventos e acesso a disco |
| Android ARM64 | Build local x86 não prova a execução no celular | APK instalado, SIMD/ABI verificados, controles, áudio, storage e sessões no dispositivo |
| Autocorreção | Resolver alvos novos não repara toda falha de sistema | Ciclo sem intervenção com falhas injetadas, reparos aprovados e regressões rejeitadas |
| Validação de conteúdo completo | Uma cena não cobre campanha, saves, modos e transições | Rotas de aceitação completas e reprodutíveis por revisão |
| Generalização e 90% | Um caso pode favorecer caminhos de uma engine | Corpus diverso, denominador fixado e resultados publicados por hash/revisão |
| Conversão em minutos | Etapas rápidas não garantem baixa latência total | Tempos frios/aquecidos de ponta a ponta, custo e taxa de sucesso sem reparo humano |

Não é possível atribuir honestamente um percentual geral de conclusão com os dados disponíveis. Os contadores medem componentes, intervalos, casos de teste e checkpoints, não a fração da biblioteca que já funciona.

## 21. Sequência concreta para chegar ao produto pretendido

### Marco A — primeiro caso de aceitação completo

Usar Monster House para fechar as capacidades genéricas ainda expostas: renderização, controle real, progressão, áudio e save/reload. Aceitar apenas um build normal reproduzível; resultados obtidos com redirects de debugger continuam diagnósticos. Depois validar o caminho Android em dispositivo físico.

Esse marco testa o conjunto integrado; ele não constitui a universalidade e não exige organizar o projeto permanentemente em torno desse único jogo.

### Marco B — generalização com mais três ISOs

O ciclo proposto no plano ainda não foi concluído. Escolher títulos que exercitem características diferentes e congelar as capacidades gerais antes de cada submissão. Para cada ISO, registrar o que o pipeline resolveu sozinho, a primeira falha, a correção necessária e quais regressões passaram.

Promover correções ao compilador/runtime por comportamento e evidência. Qualquer perfil específico necessário deve ser selecionado automaticamente por identidade exata e não mascarar uma falha genérica compartilhada.

### Marco C — autonomia mensurável

Transformar as classes de diagnóstico já aprendidas em classificadores, reproduções mínimas e regras de reparo. Separar proponente de reparo e avaliador. O sistema precisa rejeitar “correções” que pulam instruções, omitem geometria, apenas mudam o critério de sucesso ou quebram outros títulos.

Aceitação: submissões novas resolvidas sem editar TOML/C++ à mão, com limite de custo, rastros do processo e regressões preservadas. Falhas não solucionadas precisam sair como diagnósticos explícitos.

### Marco D — código convidado totalmente nativo

Fechar IOP/VU nativos, versões de código dinâmico e pacotes por ABI. Converter descobertas de execução em artefatos AOT quando possível. Medir e registrar qualquer fallback restante; não declarar AOT total enquanto programas convidados ainda dependerem de interpretação.

### Marco E — conversão rápida e nuvem

Com comportamento validado, consolidar identidade forte de cache, reuso de objetos/runtime, toolchains pré-preparados, filas e workers por destino. Medir tempo total e custo por ISO fria, revisão nova e cache aquecido. A meta de minutos precisa passar nesses cenários, não apenas no tempo de carregar uma biblioteca já compilada.

### Marco F — medir 90% e ampliar a biblioteca

Definir catálogo, revisões, dispositivos e critérios de suporte. Medir separadamente:

- **Compatibilidade:** quais jogos/revisões passaram os critérios completos.
- **Automação:** quantos desses resultados foram obtidos sem intervenção.
- **Natividade:** quais processadores/caminhos ainda usam código convidado interpretado.
- **Desempenho:** taxa de frames estável, frame times, áudio, consumo e temperatura por dispositivo.
- **Latência de conversão:** tempos e custo frio/aquecido.

Só então “90%” se torna um resultado calculável. A expansão para a biblioteca inteira continua sendo objetivo, com falhas e classes não suportadas visíveis.

## 22. Comandos e localização dos artefatos

Comandos existentes, executados a partir da raiz do projeto:

```sh
python3 -m tools.ps2native inspect --iso '/caminho/jogo.iso'
python3 -m tools.ps2native inspect --iso '/caminho/jogo.iso' --json-output
python3 -m tools.ps2native build --iso '/caminho/jogo.iso' --target desktop --out '/caminho/pacote-novo'
python3 -m tools.ps2native build --iso '/caminho/jogo.iso' --target android --out '/caminho/pacote-android-novo'
python3 -m tools.ps2native verify --package '/caminho/pacote'
```

Opções atuais incluem `--work-root`, ferramentas explícitas, `--gradle`, `--build-jobs` e timeout por ferramenta. Os comandos descrevem a interface existente; não são uma promessa de que qualquer jogo passará o runtime. `--out` exige destino ainda não existente.

Workspace principal consultado:

```text
build/ps2native/
  monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/
    run-d99723de6164/
      analysis/ps2native.toml
      analysis/output/                 C++ e relatórios do recompilador
      desktop-project/build/          compilação nativa por título
      logs/                           fixtures, capturas e validação
      package/
        bin/ps2EntryRunner
        game/                         dados do disco e outputs mutáveis
        manifest.json
```

As principais fontes desta auditoria:

- [Plano mestre de automação](/home/pedrohs/Downloads/ps2-native-recompiler/README_AUTOMACAO_UNIVERSAL.md).
- [Contrato inicial do projeto](/home/pedrohs/Downloads/ps2-native-recompiler/PROJECT_SPEC.md).
- [CLI e pipeline documentados](/home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/README.md).
- [Implementação do pipeline](/home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/pipeline.py).
- [Driver de overlays nativos](/home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/native_overlay_driver.py).
- [Correções, testes e medições recentes](/home/pedrohs/Downloads/ps2-native-recompiler/docs/FAST_NATIVE_ITERATION.md).
- [Auditoria inicial das lacunas](/home/pedrohs/Downloads/ps2-native-recompiler/docs/RECOMPILER_GAPS.md).
- [Pipeline Android](/home/pedrohs/Downloads/ps2-native-recompiler/docs/ANDROID_PIPELINE.md).
- [README Android](/home/pedrohs/Downloads/ps2-native-recompiler/android/README.md).
- [Ferramenta de testes headless](/home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py).
- [Registro consolidado de validação](/home/pedrohs/Downloads/ps2-native-recompiler/build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-d99723de6164/logs/dispatch-validation.json).
- [Manifest do pacote experimental](/home/pedrohs/Downloads/ps2-native-recompiler/build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-d99723de6164/package/manifest.json).
- [Benchmark VU final](/home/pedrohs/Downloads/ps2-native-recompiler/build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-d99723de6164/logs/vu-final-benchmark-summary.json).
- [Testes ligados ao runtime desktop otimizado](/home/pedrohs/Downloads/ps2-native-recompiler/build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-d99723de6164/logs/vu-o2-desktop-root-tests.log).
- [Taxa de apresentação headless registrada](/home/pedrohs/Downloads/ps2-native-recompiler/build/ps2native/monster-house-br-usa-t2.0-www.romsportugues.com-cb596f3249f4/run-d99723de6164/logs/headless-vu-o2-interior-presentation-rate.json).

Alguns documentos são snapshots anteriores: por exemplo, a auditoria inicial de lacunas antecede o driver de overlays, e seções antigas de iteração registram totais menores de testes. Para resultados recentes, este relatório dá precedência ao registro consolidado de validação, aos logs correspondentes e às seções atualizadas de iteração.

## 23. Resumo final do PS2Native

**Já temos:** entrada de ISO, inventário e extração; análise e recompilação EE; ligação e pacote nativo de PC; preparação Android; módulos secundários heurísticos; despacho em lote; overlays EE nativos no Linux; cache e build incremental; runtime parcialmente funcional; correções compartilhadas; fixtures nativas; replay e observação headless; relatórios de integridade e lacunas.

**Aprendemos com Monster House:** vários bloqueios tinham origem em semântica de instruções, ABI, buffers e temporização compartilhados. Resolver essas causas produziu melhorias de ferramenta, em vez de apenas uma lista de endereços para um jogo.

**Ainda falta para o objetivo final:** fechar semântica e dispositivos, eliminar dependência de interpretação de programas convidados conforme o contrato desejado, validar plataformas e conteúdo completo, demonstrar generalização, construir autocorreção e serviço na nuvem, e medir conversão rápida com compatibilidade real.

O próximo salto de produto é transformar a capacidade de construir e investigar um título em **capacidade comprovada de resolver ISOs novas automaticamente**. Essa é a direção do projeto e também o ponto que ainda precisa ser demonstrado.
<!-- PS2NATIVE_SOURCE_END:RELATORIO_PS2NATIVE_ESTADO_ATUAL.md -->

---

<a id="anexo-04"></a>

# ANEXO 04 — PROJECT_SPEC.md

Origem: [PROJECT_SPEC.md](PROJECT_SPEC.md). Linhas originais: **156**. Bytes: **12572**. SHA-256: `ae42f29ca50b459bce0f9854acc41f577404f594f72bf0efbf60be7182152468`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:PROJECT_SPEC.md -->
# PS2 Native Recompiler — Project Specification

## Status

Draft baseline for the active project. The long-term objective remains: accept PS2 disc images and generate complete native builds for Android and PC with a plug-and-play workflow. This specification does not claim universal compatibility exists today; it defines the architecture and evidence required to earn that claim.

Current working-tree progress: supported ISO9660/Joliet intake, `SYSTEM.CNF` boot selection, content-deduplicated ELF inventory, boot-ELF AOT compilation, heuristic MIPS III secondary-ELF compilation, module-scoped function registration, isolated desktop packaging, and Android per-title project/asset staging are implemented. SIF loads activate matching compiled ranges with overlap ownership. Other MIPS executables and arbitrary overlays remain unresolved; Android APK generation and real-title playability remain unverified.

## Objective

Build an open-source toolchain that takes a user-supplied PS2 ISO, discovers its executable code and data, statically recompiles EE/IOP guest code to native host code, links a reusable PS2 compatibility runtime, and packages a game-specific Android APK or PC executable.

“Native” means EE/IOP instructions are translated ahead of time to ARM64 or x86-64 machine code and execute as host code. A shared runtime still implements PS2 device semantics (GS, VU, DMA, SPU2, CDVD, IOP services, timing, input, and saves). Dynamically generated or undiscovered guest code may use a selective fallback path; the normal game CPU path must not require a full-system PS2 CPU emulator.

The source project, translator, runtime, profiles, and packaging tools are open source. Game ISOs, extracted files, firmware, and generated game-derived builds remain local inputs/outputs unless separately licensed for redistribution.

## Assumptions

1. PS2 is the first and only console family in this project; `.iso` does not imply a console or executable format.
2. Initial host targets are Android ARM64-v8a, Windows x86-64, and Linux x86-64. The architecture should permit additional PC/mobile targets.
3. A desktop builder accepts a local ISO and emits an installable APK or a desktop executable. Runtime users should not need to hand-edit configuration or extract files manually for supported games.
4. User-provided game data is read locally and is not uploaded, committed, or included in public source releases.
5. The initial codebase is the public GPL-3.0 PS2Recomp project in this directory. Preserve upstream notices and keep the full derivative project GPL-compatible.
6. A title-specific compatibility profile is an allowed automated fallback when static discovery or generic hardware behavior is insufficient; the long-term product goal remains broad ISO coverage and low-touch use.

## Product Flow

```text
Local PS2 ISO
  -> inspect disc and SYSTEM.CNF
  -> locate boot ELF, IRX modules, overlays, and assets
  -> analyze code/data and discover guest entry points
  -> lower R5900/IOP/VU code into host-independent IR
  -> emit AArch64 or x86-64 native code
  -> link PS2 compatibility runtime and user assets
  -> sign/package APK or stage desktop executable
```

## Interface

Initial interface is a desktop CLI so the end-to-end pipeline can be automated and debugged. A GUI can call the same stable library/commands later.

```sh
python3 -m tools.ps2native inspect --iso "/path/game.iso" --json-output
python3 -m tools.ps2native build --iso "/path/game.iso" --target android --out out/android-package
python3 -m tools.ps2native build --iso "/path/game.iso" --target desktop --out out/desktop-package
```

The current `desktop` target builds for the host machine ABI. Separate Windows,
Linux, and macOS cross-compilation targets are future work; the current
`android` target emits an ARM64 APK only when its Gradle/SDK/NDK toolchain is
installed.

The build report must list game ID, region/revision, source hashes, discovered executables/overlays, translated instruction coverage, runtime services used, target ABI, compatibility profile, warnings, and unresolved behavior.

## Technical Stack

- C++20 and CMake for the shared runtime, translator, analysis libraries, and packager.
- Existing PS2Recomp modules are the starting point: `ps2xAnalyzer`, `ps2xRecomp`, `ps2xRuntime`, and `ps2xIOP`.
- A host-independent guest IR separates instruction semantics from ARM64 and x86-64 code generation.
- Android builds use Android NDK/Gradle and produce ARM64-v8a APKs; desktop packaging is target-specific.
- Vulkan is the preferred shared graphics backend where available, behind a GS-facing abstraction. Audio, input, files, clocks, and threading also sit behind host interfaces.

## Core Components

1. **Disc and executable intake:** ISO9660 directory reader, `SYSTEM.CNF` parser, boot ELF and IRX discovery, checksums, deterministic manifest, and clear diagnostics for malformed/unsupported images.
2. **Binary analysis:** ELF segments/relocations/symbols, function boundary discovery, CFG recovery, jump tables, overlay/code-region tracking, confidence labels, and optional user-supplied Ghidra maps.
3. **Guest semantics:** explicit R5900 MIPS, MMI, COP0/COP1, branch-delay, exception, memory, and IOP/R3000A semantics. Unsupported instructions must be diagnosed, never silently replaced with NOPs.
4. **Static recompilation:** lift to a host-independent IR, optimize without changing guest-visible behavior, emit AArch64 and x86-64, and register code for overlays/dynamic entry points.
5. **Selective dynamic fallback:** detect code writes, invalidate stale translated blocks, and route unknown or runtime-generated blocks through an interpreter/JIT fallback. Report every fallback path.
6. **PS2 compatibility runtime:** EE/IOP memory and scheduling; VU0/VU1; DMAC/VIF/GIF/SIF; GS; SPU2; IPU/CDVD; BIOS/HLE services; controller; memory card; timing; and per-game override/profile hooks.
7. **Host adapters:** Android APK lifecycle, rendering surface, audio, controller/touch mapping, scoped storage, and save storage; equivalent desktop window/audio/input/file adapters.
8. **Packaging:** private local asset staging, native library linking, app metadata, debug/release signing options, launch configuration, and reproducible build manifest.
9. **Compatibility database:** public metadata and tests keyed to exact game ID, region, revision, and executable hash. Profiles may contain code/configuration but not game content.

## Project Structure

```text
ps2xAnalyzer/             ISO/ELF analysis and code discovery
ps2xRecomp/               Guest IR and native code generators
ps2xRuntime/              Shared PS2 hardware compatibility services
ps2xIOP/                  IOP execution and service bridge
android/                  Android runner and APK packaging
desktop/                  Windows/Linux runner and packaging
tools/                    ISO inspection, build orchestration, manifests
profiles/                 Source-only compatibility profiles
docs/                     Architecture, support matrix, reverse-engineering notes
PROJECT_SPEC.md           Product contract and acceptance criteria
```

## Code Style

Keep guest state explicit and independent from host ABI. Centralize guest address translation and never cast guest addresses directly to host pointers.

```cpp
struct GuestAddress {
    std::uint32_t value;
};

Result<BootManifest> inspectDisc(const std::filesystem::path& isoPath);
```

Use descriptive lower-camel-case functions, PascalCase types, RAII for host resources, structured diagnostics with guest address/instruction context, and deterministic output ordering. All guest-visible arithmetic and flags must use named semantic helpers instead of host-dependent implicit behavior.

## Build, Run, and Verification Commands

Existing upstream baseline:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/ps2_analyzer --help
./build/ps2_recomp --help
```

Current prototype commands:

```sh
python3 -m tools.ps2native inspect --iso game.iso --json-output
python3 -m tools.ps2native build --iso game.iso --target desktop --out out/desktop-package
python3 -m tools.ps2native build --iso game.iso --target android --out out/android-package
```

Android packaging must have a documented NDK/Gradle invocation and produce a signed installable APK. Full-game compatibility claims require repeatable cold-boot gameplay runs and a public status row for the exact ISO/executable hash.

## Compatibility and Completion Criteria

The long-term success criterion is that a user supplies any supported PS2 ISO and receives a complete native PC build and/or Android APK without editing project files. Support means the exact game/revision reaches playable gameplay and correctly handles its menus, scenes/FMVs where present, audio, controls, save/load, and transitions. If a game requires a profile, the profile must be selected automatically from its identity and hash.

Every accepted ISO must produce one of three explicit outcomes: **buildable**, **buildable with an automatically selected profile**, or a diagnostic report identifying the unsupported binary/hardware behavior. Silent partial builds are failures.

“Any PS2 ISO” is not considered proven by a single demo. Completion requires a compatibility corpus covering the target library, automatic code discovery for retail stripped executables and overlays, complete hardware-service coverage for observed title behavior, and native runtime verification on Android ARM64 and PC x86-64. Compatibility reports must separate CPU translation coverage from full-game playability.

## Phased Delivery

1. **Baseline and intake:** preserve upstream build, document its gaps, parse ISO and `SYSTEM.CNF`, hash-deduplicate ELF candidates, distinguish the boot executable from MIPS III secondary candidates and IRX candidates, and emit a deterministic report. The boot path and heuristic MIPS III module handoff are implemented; retail classification coverage remains.
2. **Compiler foundation:** establish semantic IR, complete instruction coverage accounting, decoder/runtime differential tooling, ARM64 and x86-64 backends.
3. **End-to-end host build:** ISO-to-native desktop output including overlays, assets, basic boot, controller, audio, and saves.
4. **Android target:** cross-compile runtime/native code, generate APK, input/audio/rendering/storage integration, installable build.
5. **Dynamic code and hardware completion:** discover/recompile or selectively interpret dynamic blocks; close EE/IOP/VU/GS/DMA/SPU2/CDVD service gaps.
6. **Universalization:** build a broad PS2 compatibility corpus, auto-match profiles, fix regressions, publish per-title/per-region compatibility evidence, and remove manual setup for supported images.

## Boundaries

- **Always:** preserve upstream licenses; read user ISOs locally; keep game-derived files out of source releases; retain original hashes and exact revision metadata; expose incomplete coverage honestly.
- **Ask first:** change upstream project license, publish generated game-specific artifacts, or add network upload/telemetry of any user files.
- **Never:** bundle a commercial ISO, BIOS image, extracted assets, generated executable code, or unreviewed third-party game content in public releases.

## Current Known Risks

- The host recompiler/analyzer/packager are now separate from the Android runtime build, but no Android APK has been built in the current environment.
- Secondary MIPS `ET_EXEC` candidates are ambiguous between EE and IOP code from basic ELF headers alone. Compiling them as R5900 code without further classification would be incorrect.
- The boot path keeps its legacy global dense function table and registers sparse aliases for SIF loads. Generated MIPS III modules register sparse function maps per runtime; SIF loads activate path-matched ranges and newest loads own overlaps. Failed heuristic candidates remain visible in the manifest. Named partial `SifLoadElfPart` loads currently fail without writing memory. The e_flags classification is heuristic, and buffer-loaded modules and arbitrary overlays are not covered.
- Static control-flow analysis cannot guarantee discovery of indirect jumps, overlays, or runtime-generated code.
- VU microcode, GS behavior, DMA ordering, timing, IOP modules, and firmware services are substantial compatibility subsystems.
- The provided `JUYL Unlimited Codes.rar` is password-protected, so its ISO, region, and title remain unverified until an accessible archive/password is supplied.
<!-- PS2NATIVE_SOURCE_END:PROJECT_SPEC.md -->

---

<a id="anexo-05"></a>

# ANEXO 05 — lab/README.md

Origem: [lab/README.md](lab/README.md). Linhas originais: **1123**. Bytes: **70422**. SHA-256: `1cd8dd7482e0746ba67320b1ee574ae533023634dcbf501006ec0e907fda36d7`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:lab/README.md -->
# NEXO laboratory: canonical device state and conservative native V0

This implements a **partial preparation** for the first increment in the root
README, section 31.1. It preserves the current VU runtime as a regression
baseline and implements a laboratory AOT V0 backend for finite microcode banks.
An original VIF-call replay now includes VU, GIF and CPU GS state. An independent
reference with a qualified full state relation, broader external-input closure,
second-architecture validation and a qualified native game package remain
unfinished. The identified independent VU engine and its timing differences are
described in `PCSX2_VU_REFERENCE.md`.

## Finite offline EE overlay catalog

`extract_ee_overlay.py` recovers source snapshots and binding identities from
identified diagnostic ELF artifacts without loading them. `generate_ee_bank_catalog.py`
regenerates each bank offline, checks every recovered binding, isolates native
symbols by bank namespace and emits a hashed static catalog. The opt-in
`PS2X_RUNTIME_AOT_EE_OVERLAYS=ON` backend uses sparse physical-PC pages and complete
declared instruction-footprint guards. It retains multiple precompiled versions
and stops on uncovered code, including branches under diagnostic skip policies.
The diagnostic driver implementation is omitted from this backend. The complete
V0 contract and commands are in [`nexo-ee-aot-v0.md`](../schemas/nexo-ee-aot-v0.md).

The original Monster House observation yielded **32 banks, 508,176 bindings**.
Resuming validated recovery after replacing a quadratic symbol search took
1.778 seconds. Initial C++ generation took 4.458 seconds. Regenerated bindings
agreed exactly with the recovered tables; these measurements cover that observed
corpus only. All snapshots/generated commercial sources remain local and ignored.

The first native catalog/runtime/test build took 167.928 seconds with four workers
and no LTO. The EE catalog and its test built successfully; the overall command
then failed on an incorrectly named additional IOP target. The corrected
incremental build took 11.515 seconds. Updating only the catalog index and manifest
took 10.117 seconds and retained **all 32 bank objects' bytes and timestamps**.
Relinking the actual game against EE and IOP catalogs took 12.757 seconds, using
the existing original game objects with zero original EE game recompilations.

The isolated Xvfb run had the EE diagnostic driver explicitly removed from its
environment. The executable contains the AOT dispatcher/catalog and lacks the
identified diagnostic EE resolver/release and generic IOP execution symbols.
It displayed the Sony logo and autosave confirmation, then remained on a dark
loading screen with a memory-card notice. Captures recorded 14 module loads and
4,096 RPC requests. At the last captured checkpoint: 29,724,395 native IOP
instructions, zero interpreted instructions and no captured native fault.
The RPC capture limit was reached; these are subset observations, not total
execution counters or service-fidelity evidence. VU remained diagnostic.

The run automatically stopped with `EE:UNSEEN_CODE` at `0x1193880`, called from
`0x119a458`. That PC already exists in the recovered catalog. Its first four
logged instruction words agree with the old snapshot; the full declared
40-byte dependency and entry/module context were not captured at the miss.
The precise rejection reason is therefore unqualified. Adding the same address
manually would not establish coverage. The next step is automatic miss-state
capture and comparison, then version/family admission from exact observed bytes.
No gameplay, save correctness, 60 FPS, Android or full campaign gate passed.

The restored diagnostic build passed 65 CTest groups and 485 general runtime
cases. The native EE profile passed its three C++ cases and four selected CTest
groups before restoration; Python coverage includes five extractor cases,
six catalog cases and six headless isolation cases. The current diagnostic IOP
profile passed eight groups and the current native catalog profile passed four.
Restoration retained 51 of 52 common runtime objects' bytes/timestamps; the
runtime source rebuilt after its included overlay header was edited. This is
not an unrelated whole-runtime rebuild from the backend selection flag.

Evidence is under the ignored directory in `build/lab/latest-ee-aot-bank-job.txt`:
recovered banks, manifests, command/timing records, source/binary copies, symbol
audit, RPC observations, screenshots, unseen classification and regression logs.
The root README's M3/publication/context/canonical-checkpoint requirements remain
open. Finite observed coverage does not prove closure or independent fidelity.

### Automatic EE guard diagnosis and incremental admission

The laboratory now captures an AOT miss's RAM, optional canonical EE model
context, module ownership and every declared candidate footprint. The copied
RAM supports reproducible guard comparison; writer quiescence and a complete
machine checkpoint remain unqualified. `prepare_ee_miss.py` converts an admitted
byte capture into a fresh offline bank case. Catalog `--extend` validates prior
identities and preserves unchanged sources and their timestamps. The minimal
`ps2_ee_aot_bank.h` isolates bank declarations from dispatcher/diagnostic changes.
The format, exact entry restrictions and commands are in
[`nexo-ee-miss-v1.md`](../schemas/nexo-ee-miss-v1.md).

In the new isolated Monster House run, the previous `0x1193880` miss was captured
as `CodeChanged`, with no loaded-module ownership. The 40-byte candidate begins
eight bytes before the observed entry. Only those preceding words changed:
`0` became `0x857610` and `0x8578d0`; all 32 bytes from the entry agreed.
Preparation used the observed target without a TOML address edit and emitted
16,381 bindings. Its root dependency begins at `0x1193880` and spans 32 bytes.
This refines the generated boundary while retaining complete declared guards;
it does not qualify general interior-entry semantics or hardware fetch state.

The catalog grew from 32 to 33 banks. Extension generation took **4.392 seconds**,
the new-bank/index build **17.217 seconds**, and actual game relink **10.707
seconds**. All **32 previous active bank objects retained their bytes and
timestamps**, with zero original EE game compilations. The earlier header
isolation migration required a one-time 158.240-second build of the 32 banks;
the 5.429-second capture-fixture rebuild retained all cached bank objects.
These are current-host laboratory measurements, not an ISO conversion SLA.

Tests cover 409 model cell/lane mutations, fixed-width context bit preservation,
malformed encodings, model inventory drift, whole RAM/context nonmutation,
capture limits/failure paths, offline entry contracts, prepared-case consumption,
generator identity and extension preservation. The second game run reached
another miss at `0x1142ce0`, called from `0x114302c`. Its 44-byte dependency
begins four bytes before the entry; only that preceding word changed from zero
to `0x857150`. The 40 bytes from this new entry matched. Both runs exited under
the runtime's normal stop policy, without killing another process.

An isolated guard probe linked against the preserved 33-bank artifacts admits
`0x1193880` against both saved RAM copies and rejects `0x1142ce0` against the
second copy, reproducing the diagnosis without executing a guest callback.
The second capture is prepared as a regression case, **not compiled into another
address-specific workaround**. The repeated prefix-only mismatch motivates a
general offline entry/dependency-boundary correction, with tests for normal,
interior and delay-slot entries before any guard is refined. Adding addresses
individually would not solve that generator issue. Full entry-context/fetch
qualification remains separate from this correction.

The restored regression profile passed **69 CTest groups and 485 general cases**.
Restoration took 14.774 seconds and retained all 54 common runtime object bytes
and timestamps. Gameplay, save correctness, 60 FPS, Android, VU runtime migration,
complete checkpoint replay and universal closure remain open.
Evidence is in the ignored directory recorded by `build/lab/latest-ee-miss-job.txt`.

### Batch correction of normal-entry dependencies

The repeated prefix mismatch above now has a generic offline correction. Normal
entries guard their suffix plus any earlier local static target reachable within
the callback. Standalone terminal slot entries guard their own instruction; the
runtime adapter rejects pending architectural delay contexts and rechecks bytes
at invocation. Raw RAM diagnosis remains a query, not context admission. The
diagnostic DSO driver explicitly retains legacy whole-block guards.
See [`nexo-ee-entry-dependencies-v1.md`](../schemas/nexo-ee-entry-dependencies-v1.md)
for the sidecar, producer checks and precise unresolved obligations.

Migrating **33 banks / 524,557 entries** took **9.135 seconds**, producing **58,945
dependency runs** without changing any bank C++ source. The index/runtime/test
build took **9.547 seconds** and retained **all 33 bank object hashes and
timestamps**. Actual game relink took **10.969 seconds** with no LTO and zero
original game compilations. Both saved prefix-only failures passed guard replay;
the second address-specific regression bank was not added to the catalog.

The isolated actual game reached its title, main menu and new-game file menu.
Attempting to start the selected game then stopped at `0x1724d70`, called from
`0x1baae4`, with `MissingEntry` and zero candidates. RAM, model context and the
64 KiB window were captured automatically. This is a newly uncovered region,
not either previous prefix-only rejection. Existing memory-card files were backed
up before confirming the test. No complete gameplay qualification is claimed.
Synthetic adapter execution verifies local loops, stale-code rejection after
resolution, skipped prefixes, standalone slots and preservation of guest RAM and
context on rejection. Migration tests reject callback source rewrites and retain
prior timestamps. Evidence is under `build/lab/latest-ee-entry-job.txt`.
The restored diagnostic profile passed **69 CTest groups and 487 general cases**;
the catalog group includes **14 Python cases**. The native EE profile passed its
four C++ cases. Profile restoration took **13.773 seconds**, preserving all 54
common runtime objects. The five preexisting test memory-card files were unchanged.
Full entry/fetch semantics, native VU in the game, independent fidelity, campaign,
saves, Android, universal coverage and sustained 60 FPS remain open.

### Automatic offline EE miss batches and owned inputs

`admit_ee_misses.py` consumes up to 16 captured records, prepares every case before
changing the catalog, deduplicates bank identities and calls the verified offline
publisher. After one bootstrap, hashed input copies under `ee_cases/` let the next
batch reuse all old cases automatically. A per-bank producer ledger preserves
prepared-case provenance across generator migrations. It cannot authorize an
unseen foreign case or any rewrite of a previously compiled bank source.
See [`nexo-ee-miss-batch-v1.md`](../schemas/nexo-ee-miss-batch-v1.md) for the command
and the distinction between prepared candidates and published banks.

The captured Monster House region at `0x1724d70` contributed **12,811 entries**
from its 64 KiB image. Batch preparation/extension took **11.546 seconds**, growing
the catalog to **34 banks / 537,368 entries**. The new-bank/index/runtime build
took **14.885 seconds**, preserving all **33 prior bank objects' hashes and
timestamps**; game relink took **12.934 seconds**, with no LTO and no original EE
game compilations. Saved-RAM guard replay now admits that region.

A repeat submission with no external case paths took **11.308 seconds**, detected
the duplicate and retained 34 banks. This still regenerates checked source
descriptors offline; it does not compile another bank or claim semantic cache
correctness. Synthetic tests cover atomic rejection of invalid preparations,
producer lineage, input ownership/hash/path/symlink checks, immutable timestamps,
duplicate handling, bootstrap restrictions, output conflicts and terminal reports.

This implements one laboratory preparation step of autonomy. It does not repair
arbitrary semantics, prove closure or produce a zero-touch campaign route. Game
input in the current experiment is agent-controlled and recorded. Strict approval
remains false; complete game/VU/Android/fidelity/performance gates remain open.
Evidence is under `build/lab/latest-ee-batch-job.txt`.

The 34-bank actual run stopped at `0x184f448`, again called from `0x1baae4`,
with no candidate. Both RAM copies still contain the earlier `0x1724d70` routine.
In the new RAM, three 19-word routines have the same normalized structure; only
two 16-bit immediate fields of a LUI/store pair differ, encoding distinct data
addresses. This suggests a shared initializer shape across code copies/modules.
It does not identify their materializer or prove a relocation family. The earlier
bank's saved-RAM guard success does not prove that this run executed it. No second
address-specific bank was added for the new target. Producer tracing and guarded
native family synthesis are the next investigation, alongside the remaining
entry/fetch/fidelity obligations; gameplay remains unqualified.

The restored diagnostic profile passed **70 CTest groups and 487 general cases**.
Catalog tests include **17 Python cases**, and the new batch command has **7**.
The native EE profile passed its four C++ cases. Restoration took **15.939
seconds**, retaining all 54 common runtime objects. The five preexisting test
memory-card files remained unchanged; the owned test runner exited normally.

### Offline discovery of typed EE data-family candidates

`discover_ee_data_families.py` analyzes the owned captured inputs without editing
the catalog, compiling another address-specific bank or running guest code. It
groups exact dependency-byte structures, proposing only varying LUI unsigned
immediates and SW signed offsets. Opcode/register/control changes split groups;
unchanged immediates retain exact guards. Input and search budgets, deterministic
identities, signed boundaries, round trips, duplicates and output conflicts are
covered by tests. See the
[`candidate schema`](../schemas/nexo-ee-data-family-candidates-v1.md).

The 34-bank input set yielded **196 candidates** from **71,757 dependency regions /
872,138 words**, in **1.447 seconds** including owned-ledger validation. Adding
the next miss as an offline prepared observation yielded **226 candidates** from
35 cases in **1.739 seconds**, including preparation. No bank was admitted and
the game executable/catalog remained unchanged. The eight-word suffix at
`0x184f474` (`0x184f448 + 44`) matched the same guarded structure at **eight
observed locations**, with LUI/SW parameters `389` and `-1852`. This automates
the previous manual instruction-shape comparison; it does not synthesize the
native function yet.

All **1,896 source-origin round trips** across the 226 candidates reconstructed
their exact captured bytes and hashes. Final-source replay reproduced the same
candidate structures and counts; reports now pin the analyzer source hash.
The **16 detector tests** and **71 CTest groups** passed. These extraction and
regression checks do not validate execution of a native family.

All approval fields remain false. These dependencies may be speculative or
partial function regions. Producer invariants, reachable parameter domains,
relative-PC/control semantics, entry/fetch/write/alias obligations, independent
hardware fidelity and native execution of the candidate remain open. Candidate
recognition alone cannot resume the stopped game. Evidence is under
`build/lab/latest-ee-data-family-job.txt`.

### Generated native EE data-family functions

The offline `ps2_native_data_family` frontend now reuses the existing semantic
emitters with typed LUI/SW data operands and relative PC expressions. Its generated
wrapper guards the complete physical RAM structure and normal entry, extracts
live data fields, and calls a precompiled native body. All instruction resume
labels and standalone terminal slots are retained. There is no opcode execution
loop or compilation in the runtime function. Initial regions are linear, with an
optional terminal JR/JALR plus slot; unsupported forms fail explicitly. See the
[`native synthesis contract`](../schemas/nexo-ee-native-data-family-v0.md).

The synthetic compiled fixture passed **168 execution comparisons** over three
families, 36 concrete variants/bases and every normal entry. It compares the
identified complete EE context codec and all 32 MiB of RAM. The actual observed
eight-word suffix was also compiled as **one shared function**, passing **64
entry comparisons** across its eight observed variants/bases, including the new
miss at `0x184f474`. Its generated fixture compiled in **2.275 seconds**, linked
in **0.423 seconds** without LTO and ran in **2.548 seconds**. These are constructed
context tests against the shared conservative emitter, not independent hardware
or complete-machine game replay.

The initial 226-proposal sweep generated **167 candidates** and rejected **59**
unsupported shapes in **0.397 seconds**. Generated source is not an executed or
approved family. All 34 prior concrete bank sources remained byte-identical to
the archived converter's output. The game catalog, backend and executable have
not been extended with these functions yet. Family admission, materializer/domain
proofs, fetch/write/alias/device semantics and all complete-game gates remain
open. Evidence is under `build/lab/latest-ee-family-native-job.txt`.

### Experimental precompiled family admission in the game

The finite family matcher/backend now selects immutable native structures by
their complete masked RAM identity and entry offset, across physical bases.
It rejects ambiguous matches and pending architectural slots and repeats
admission at invocation. The generated native function then rechecks its guard
and reads its live typed parameters. There is no guest compiler or opcode
execution loop in this family path. The build option defaults OFF and requires
the AOT EE laboratory profile. See the
[catalog and admission contract](../schemas/nexo-ee-family-catalog-v0.md).

The offline publisher hashes generated sources and input provenance, merges
duplicate structures and records unsupported bodies. CMake checks manifest
types, counts, laboratory flags, paths and hashes before compiling. Neither
publication nor a successful match establishes producer domains or code
closure. Fetch/cache/writes/aliases and independent fidelity remain open.

The actual Monster House test used two families, generated in **0.093 seconds**:
the observed eight-word saved-frame suffix and its eleven-word fixed prefix.
The native incremental build took **13.861 seconds**, followed by a
**16.428-second game link**, with **zero original game compilations** and no
LTO. No new concrete bank or per-address TOML entry was added. The final
provenance publication took 0.115 seconds and preserved both generated bodies
byte-for-byte.

An owned Xvfb `:90` display and isolated silent audio sink reached New Game.
The log passed the previous `0x184f448` missing entry and recorded its
`0x184f474` continuation, then stopped at another uncovered callback
`0x184f498`, source `0x1ba930`. The capture contains EE model context and RAM;
it is not a complete machine checkpoint. All five memory-card files remained
unchanged, and the runner/display/audio helper exited. Agent-supplied menu
inputs are recorded; autonomous gameplay has not been validated.

The matcher and backend recheck groups each passed three C++ cases. Eight
Python cases cover publication/provenance and malformed CMake inputs. The
native concrete-directory group also passed four cases. These are laboratory
regressions, not PS2 or campaign approval. After restoring the diagnostic
development profile, **76/76 CTest groups** and **487/487 general C++ cases**
passed. Coverage of loaded code, control-flow
family synthesis, producer/fetch proofs, native VU, Android and 60 FPS remain
open. Evidence is under `build/lab/latest-ee-family-admission-job.txt`.

### Batched typed families and stable source cache, 2026-10-01

The synthesis frontend and runtime descriptor validation now share a classifier
for 20 low-16 integer data operand classes. Integer conditional branches,
branch-likely and REGIMM link forms use relocated precise PCs and preserve the
existing conservative slot policy. Opcode/register/control bits stay fixed.
Direct J/JAL, coprocessor branches and unsupported operations remain explicit
synthesis failures. This is restricted ahead-of-time synthesis, not universal
guest execution support.

The publisher groups compiled structures into bounded hash buckets and records
the numerical family ledger in manifest schema 2. CMake verifies source hashes
and uses immutable content-addressed copies, retaining object paths across
fresh job directories. A changed header or compiler flag still invalidates its
normal dependencies. Corrupt cached bytes are rejected.

The current capture corpus proposed 1,956 families from 36 cases. Offline
publication admitted **1,622 structures in 69 source files**, using root-only
entries, terminal-transfer filtering and typed operands in one prepared root.
These restrictions and the once-observed root parameter proposal are recorded
in provenance. Counts measure this corpus, not supported games or an ISO
compatibility percentage. Unsupported/filtered proposals remain in the ledger.

Measured under the local development profile, with no LTO:

- The typed catalog's cold cache build took **55.421 seconds**; its game link
  took **11.823 seconds**, compiling zero original game source units.
- Moving an identical catalog into a fresh job directory took **0.227 seconds**
  to rebuild its family target, with zero compilations.
- Republishing after the final emitter regression fix took **4.980 seconds**;
  all 69 source files and the numerical family ledger remained unchanged.
- All 34 concrete bank sources remained byte-identical to the archived
  converter after the final changes. This checks default emission stability,
  not independent semantic correctness.

The generated differential fixture passes **1,464 normal-entry comparisons**
across 43 structures and 360 concrete fixtures, checking the identified EE
context and all 32 MiB RAM. The parameterized ADDIU-to-zero slot test first
failed because the canonical zero elided delay metadata, then passed with its
live-parameter emission guard. Comparisons share semantic emitters; they do
not cover the complete machine, independent hardware or campaign fidelity.

`nexo_ee_family_probe` diagnoses admission against immutable captured RAM and
constructed normal-entry contexts without invoking guest callbacks. Its JSON
records lookup status, candidate checks and false approval/execution flags.
For the relocated callback and its continuation, both queries returned Ready;
the result qualified a guard lookup only.

The subsequent owned Xvfb/silent-audio game test passed that relocated callback
and advanced into another loaded module. It stopped at an uncovered normal
entry `0x1a51b70`, reached through the loader's indirect call. Its eleven-word
prefix ends in BNE plus a complete slot and is supported by the current generic
frontend; the current catalog does not provide coverage there. No new concrete
bank or address-specific TOML was added in this increment. Capture, prefix
diagnosis and module coverage remain separate steps; the latter still needs an
automated batch expansion policy.

This menu regression used finite image templates prepared by the agent, ran
174.015 seconds and ended during loading. It is not a zero-shot campaign
validator. Its owned processes exited; changed memory-card files were archived
and the five original files restored with identical hashes. VU remains
diagnostic in this game experiment. Full gameplay, graphics/audio/save fidelity,
native VU closure, physical Android and sustained 60 FPS are still unqualified.

Evidence and frozen native artifacts are identified by
`build/lab/latest-ee-family-batch-job.txt`; generated game assets stay ignored.
Final native-profile admission/catalog/probe checks passed all five selected
CTest groups. After freezing those artifacts and restoring the diagnostic
build profile, the rebuilt regression suite passed **77/77 CTest groups** and
the general C++ suite **487/487 cases**. Diagnostic-profile success does not
approve a native game package.

### Singleton structure batches and offline orchestration, 2026-10-01

The detector now exposes an explicit singleton/typed policy, retaining the
original two-variant policy by default. This avoids excluding a routine merely
because it was observed once. All opcode/register/control bits remain fixed;
the broader immediate domains retain false producer/fidelity approval.

The bounded offline orchestrator prepares captures, owns hash-checked case
copies, deduplicates them and publishes a catalog with a diagnostic receipt:

```sh
python lab/prepare_ee_family_batch.py \
  --catalog /path/to/previous-concrete-catalog \
  --capture /path/to/new-ee-miss \
  --family-generator build/ps2xRecomp/ps2_native_data_family \
  --overlay-generator build/ps2xRecomp/ps2_native_overlay \
  --output /path/to/fresh-batch --workers 8
```

Additional `--case` inputs bootstrap existing laboratory cases. Subsequent jobs
can use `--previous-batch /path/to/previous-batch` with new captures; this verifies
the receipt, manifest, framed case identities and exact owned bytes. The source
cases are never rewritten. The tool launches neither a game nor a compiler in
a game's execution path, and does not change title TOMLs. It currently proposes
root-only entries and terminal-transfer regions; those are explicit incomplete
coverage policies. A failure leaves a false-approval receipt with its stage.

The publisher supports up to 32,768 finite structures and 16 concurrent offline
converter processes. Source bytes remain deterministic across worker counts.
Body units are bounded by both 32 families and 1 MiB; the index is bounded by
8 MiB and the manifest by 16 MiB. Candidate serialization has a 64 MiB bound
checked before publication. Direct J/JAL synthesis now preserves absolute
targets and relocated source/link PCs, including slot ordering; encoded targets
remain fixed, and unsupported operations are still declined explicitly.

The 37-case experiment found **15,272 candidate structures** in 2.356 seconds.
With direct control support, eight offline workers admitted **8,293 structures
in 293 source units** in 14.809 seconds; 6,979 proposals were declined with
reasons. The full orchestrator then reproduced exactly the same numerical
ledger and all 293 source files in 18.170 seconds. These numbers count captured
byte structures, not games or code closure.

The initial CMake configuration took 123.979 seconds. Profiling its structure
showed that every source query reparsed the large numerical JSON ledger.
Extracting the smaller source/hash indexes reduced the later measured
configuration to **6.891 seconds**. The first native compilation took
322.753 seconds; the identical catalog in the orchestrated job rebuilt in
**0.816 seconds with zero source compilations**. The game link took 17.403
seconds, retaining all original game objects. Measurements use the local
development profile and uncontrolled host load, not a conversion-time guarantee.

The admission-only probe queried 14,483 prepared addresses against captured
RAM in 0.179 seconds: 3,456 Ready, 11,027 MissingEntry, zero Ambiguous, and
5,866,982 candidate checks. These include speculative window bindings and
constructed normal contexts; they are not an execution coverage percentage.

The owned headless New Game test passed `0x1a51b70`, continued through
`0x1a51b9c` and `0x1a51bb4`, and stopped at `0x1a51be8`. The latter's 28-word
linear dependency was already present in the prepared case and is accepted by
the generic synthesis frontend. It was omitted by the terminal-only policy.
The next work must include and qualify these linear regions in batches, rather
than adding a binding for that one address. Runtime progress is documented by
the guest trace, not inferred from probe results alone.

The route took 189.480 seconds and ended during loading. Its menu templates are
an agent-authored finite regression fixture. Its processes exited and changed
card files were archived, then all five original files restored by exact hash.
Gameplay, zero-touch exploration, producer/fetch/alias/timing proofs, native VU,
physical Android and sustained 60 FPS remain unqualified.

Review subsequently added actual-region checks for J/JAL destinations that
land inside a relocated family. The final development-profile regression passed
**78/78 CTest groups and 487/487 general cases**; the generated differential
fixture compared **1,518 normal entries across 387 fixtures / 47 structures**.
These compare shared emitter semantics, not an independent PS2 reference.
The archived game runner and its measured catalog predate this final local
destination correction; their traces and timings identify that earlier build.
A fresh offline batch after the correction reproduced the same 8,293-family
numerical ledger in 20.586 seconds; 134 of its 293 source units remained
identical. All 34 conventional overlay outputs remained byte-identical with
the same default normal-entry generation recipe. This fresh catalog has not
yet supplied a new game execution test.

Evidence is identified by `build/lab/latest-ee-structure-batch-job.txt`.

## Initial IOP AOT path

The IOP now also has an instruction-specialized V0 bridge integrated with IRX
startup and RPC callbacks. `generate_iop_bank.py` consumes an already relocated
RAM bank, creates an entry for every aligned word, and emits C++ operations with
constant instruction parameters. The native dispatcher verifies live code
identity, supports interior entries and RAM aliases, and refuses missing,
changed, or misaligned code without interpreting it.

The synthetic acceptance corpus has 23 strict native cases, 26 diagnostic cases
(including 292 absolute and 159 parameterized one-step comparisons to the
identified CPU model), and 12 converter cases. It includes native RPC, self modification, all RAM writers,
pending loads and branches, unknown imports, incomplete relocations, and
unfinished startup. `PS2X_IOP_ENABLE_INTERPRETER=OFF` removes the generic CPU
instruction-execution symbol from the native test executable.

This is preparation for M4. The default game runtime uses its diagnostic IOP
configuration; an explicit native IOP catalog option is described below.
Banks for its complete commercial corpus,
independent R3000A fidelity, canonical snapshots and qualified service/timing
contracts remain open. A synthetic startup/RPC result does not qualify Monster
House or a complete game. Commands and the internal bank contract are in
[`nexo-iop-aot-v0.md`](../schemas/nexo-iop-aot-v0.md).

### Original IRX startup bridge

`PS2X_IOP_BUILD_LAB=ON` adds `nexo_iop_inspect`: it runs the existing loader
offline, rejects incomplete relocation tables and misaligned/out-of-range entry
points, and writes a complete relocated RAM bank plus `module.json`.
`generate_iop_bank.py --loaded-module` reads that metadata directly; no manual
function-entry or callback address list is supplied.

```sh
cmake -S ps2xIOP -B build/iop-aot-strict -DPS2X_IOP_BUILD_LAB=ON -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto' -DCMAKE_BUILD_TYPE=Release
cmake --build build/iop-aot-strict --target nexo_iop_inspect --parallel 4
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-case
python lab/generate_iop_bank.py --loaded-module /path/to/new-case --output /path/to/generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-result
```

The native probe uses a bounded laboratory host and reports unsupported external
operations as failures. It captures final physical IOP RAM and EE RAM, startup
return, counters and logs. `nexo_iop_baseline_probe` is a separate target available
only with the diagnostic interpreter enabled. Neither probe is a game runner.

The original Monster House `HKSIF.IRX` startup was exercised with 85 native guest
operations and 11 service dispatches, zero interpreted operations and no generic
CPU execution symbol in the native binary. Its 2 MiB IOP RAM, 32 MiB EE RAM,
startup return, counters and logs matched the identified diagnostic model.
All 11 external IRX files passed **offline loader acceptance**, each isolated at
the default base. The later catalog experiment below also executed their startups.

Relocation-family binding in the actual game, embedded `IOPRP271.IMG` modules,
canonical hidden state and independent fidelity remain open. The commercial
images, generated C++ and captures stay in ignored local build directories.

### Relocatable IRX families

The inspector also emits `source-image.bin` and per-word relocation masks.
`generate_iop_bank.py --family-module` embeds the full image identity and generates
fixed operation shapes with bound operands. It admits immediate-16 operand
forms and J/JAL targets. Full-word relocated data and unqualified operand forms
receive no executable callback; jumping to them fails explicitly.

```sh
build/iop-aot-strict/nexo_iop_inspect /path/to/module.irx /path/to/new-family-case
python lab/generate_iop_bank.py --family-module /path/to/new-family-case --output /path/to/family-generated --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-aot-strict -DNEXO_IOP_BANK_CPP=/path/to/family-generated/iop_native_bank.cpp
cmake --build build/iop-aot-strict --target nexo_iop_native_probe --parallel 4
build/iop-aot-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-family-result 2
```

The subsystem binds the compiled family after relocation and before startup.
The dispatcher owns image/entry metadata, checks source identity, dimensions,
relocation masks and fixed instruction bits, then guards the complete bound
word before each native guest operation. Reset clears bindings while retaining
compiled families. Directory replacement invalidates a previous overlapping
binding completely; module unload also retires its directory.

The original HKSIF family passed two consecutive startups, automatically placed
at `0x10000` and `0x10500`: 170 native operations, 22 service dispatches, zero
interpreted operations and no native faults. Full RAM and all reported fields
apart from native/diagnostic counters agreed with the identified model. A
modified source image failed before executing a native operation.

This is one module's startup coverage. The loader remains an identified model;
independent fidelity, hidden state, service/version/timing contracts, code
publication epochs and full kernel lifecycle on replacement are open. Buffer
load identity variants, the complete module corpus and final game integration
are also open. Matching a family at two bases does not qualify M4 or a game.

### Shared multi-module catalog and runtime adapter

`--family-catalog` accepts multiple inspector directories and emits one catalog.
Each full image keeps its own directory and complete bound-word guards. Immediate
and jump target operands share compiled operation shapes across modules, including
non-relocated instructions whose original words must still match exactly. The
operation pool is explicitly instantiated once, in 64 stable hash shards; module
directories contain references only. Unchanged files retain their mtimes.

```sh
python lab/generate_iop_bank.py --family-catalog /path/to/case-a /path/to/case-b --output /path/to/catalog --symbol compiledIopProgram
cmake -S ps2xIOP -B build/iop-catalog-strict -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_BUILD_LAB=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF -DNEXO_IOP_BANK_CPP= -DNEXO_IOP_BANK_MANIFEST=/path/to/catalog/catalog.json -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto'
cmake --build build/iop-catalog-strict --parallel 4
ctest --test-dir build/iop-catalog-strict --output-on-failure
build/iop-catalog-strict/nexo_iop_native_probe /path/to/module.irx /path/to/new-result
```

CMake admits only generated C++ basenames and checks the hashes of every source
and the semantics header. Source/header changes trigger manifest revalidation
before building. The converter has eight catalog tests covering normalization,
deduplication, incremental stability, unsafe manifests and changed sources.

For the 11 external Monster House IRX images, generation took 0.40 s and a new
standalone library/catalog/probe/test build took 27.58 s with four workers and
`-O1 -fno-lto`. Their 156,928 directory words selected 3,933 shared kernels;
435 relocated-data or unqualified words have no executable callback. All 11
isolated startups completed: 1,915 native operations, zero interpreted operations
and zero native faults. Each reported state and full IOP/EE RAM matched the
identified diagnostic model. Created background threads were not exercised.
These measurements cover this IOP corpus, not whole-game conversion or FPS.

The experimental root integration uses the actual runtime file/memory adapter:

```sh
cmake -S . -B build/runtime-native-iop -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF -DPS2X_RUNTIME_NATIVE_IOP=ON -DNEXO_IOP_BANK_MANIFEST=/path/to/catalog/catalog.json
cmake --build build/runtime-native-iop --target nexo_iop_runtime_probe --parallel 4
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/runtime-native-iop/lab/nexo_iop_runtime_probe /path/to/module.irx /path/to/new-runtime-result
```

The runtime probe initializes RAM only, never window/audio or EE/VU execution,
and loads the module twice through `PS2Runtime::loadIopModule`. Original HKSIF
completed both startups with 170 native operations, zero interpreted operations
and no faults; full RAM and reported state matched the actual diagnostic runtime
adapter. The native executable lacks the generic IOP instruction-execution symbol.
The opt-in requires a compiled catalog and the IOP interpreter disabled; it does
not qualify the application's EE/VU paths. In the root fast-iteration profile,
both the IOP library and catalog use `-O1 -fno-lto` and disable IPO.
In the existing root cache, rebuilding the native adapter/library/catalog with
that profile took 27.37 s; the next unchanged build took 0.26 s. Regenerating the
same catalog took 0.39 s and preserved all 78 source/header/manifest mtimes. A
one-byte change to the original HKSIF source was rejected by the actual runtime
before any guest operation, with no interpreted fallback.

Complete dependency ordering/RPC, embedded `IOPRP271.IMG`, buffer-load identity
variants, full replacement lifecycle, independent fidelity, service/timing
contracts and complete game execution remain
open. All commercial inputs, generated sources and RAM captures remain local in
ignored build directories.

### Import guards and bounded combined boot sequence

Native service dispatch now requires admitted identities for the import stub,
its ordinal word, table metadata and all preceding words examined by the
identified decoder. A known library name cannot execute a service from an empty
bank. Guest stores that change the ordinal, library name or version fail before
service execution. Relocated metadata can have a guarded data identity without
acquiring an executable callback. Aliases, reset and malformed dependency ranges
are covered by the native tests. This checks import dependencies; it does not
qualify the underlying service implementations or fetch/publication timing.

The actual runtime probe can load multiple modules and schedule IOP work after
each load without executing EE/VU code or initializing a window:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/runtime-native-iop/lab/nexo_iop_runtime_probe --sequence /path/to/new-sequence-result 80000 /path/to/SIO2MAN.IRX /path/to/PADMAN.IRX
```

The decimal cycle budget is EE cycles per load (0..16,000,000), passed to the
existing IOP clock model. At most 32 module paths from one host directory are
accepted. Reports include each load/scheduling checkpoint and detect exhausted
startup budgets. Four CLI tests cover invalid budgets/counts, mixed directories
and preservation of existing outputs.

The ten external modules in a preserved Monster House boot log were exercised
in its observed order, with 0, 800 and 80,000 EE cycles scheduled after each
load. At 80,000 cycles, the actual native runtime adapter observed eight threads,
six registered RPC servers, 6,482 native operations, zero interpreted operations
and no native faults; 4,758 guest/service instructions occurred during scheduling.
All reported fields except execution counters, full IOP/EE RAM, and runtime logs
agreed with the identified diagnostic adapter at all three budgets.

This experiment covers that module sequence and bounded thread work. It does
not cover RPC requests, the game's EE path, load arguments, complete dependency
ordering, or game progression. The guest logs include an event-wait failure and
a SIF-initialization warning in both adapters; model equality does not resolve
those issues. Embedded modules, buffer identities, full replacement lifecycle,
canonical hidden state, service fidelity and hardware timing remain open.

### Observed module and RPC capture

The root `PS2X_BUILD_NEXO_LAB=ON` build now includes optional IOP observation
hooks. Set `PS2X_IOP_CAPTURE_DIR` to a fresh local directory when launching a
laboratory producer or the actual-runtime probe. The non-laboratory build does
not compile these hooks. The standalone `PS2X_IOP_BUILD_LAB` option continues
to select offline inspector/probe tools; it does not enable live observation.

Each chronological event has a numbered directory. Module events contain the
exact loader input (`image.irx`), raw arguments (`arguments.bin`) and metadata.
RPC events record request metadata and bounded EE send bytes, then result
policies, instruction counters and bounded receive bytes. A native fault records
its diagnostic and physical IOP RAM. Missing payloads are explicitly marked;
a missing metadata/result file is incomplete evidence. Recording errors are
reported without writing guest memory, and failure of the diagnostic sink is
contained. Output event directories are never reused.

Capture has separate process-wide limits: 4,096 RPC events, 256 module events
and 16 fault events. RPC payloads are limited to 1 MiB. Exhaustion warns once
per event kind and leaves the other kinds' budgets available. These are bounded
observations, not complete execution histories or canonical state checkpoints.
Captures add work and are unsuitable for certifying performance.

Four capture CLI tests cover raw arguments and escaping, request/result
payloads and policies, invalid and oversized buffers, empty buffers, an HLE
route and an unhandled route, recording failures, disabled observation,
independent event budgets, preserved existing events, first native-fault
observation, and unchanged actual-runtime reports/full RAM.

A headless Monster House diagnostic producer yielded ten external module
images, four buffer module loads and 4,082 handled RPC observations before
the original shared event limit was exhausted. The four buffer images were
byte-identical (7,096 bytes, SHA-256
`aae4e64bbb49d54caf2e1c9c9071dccf46ed95f06deaf072a4c79eef7b8765c1`),
so they require one additional offline family. The captured unique images plus
CUTSTRM produced a twelve-module catalog with 3,947 shared kernels. Generation
took 0.412 seconds and the cached standalone native build took 21.113 seconds
with four workers and `-O1 -fno-lto`. Relinking the diagnostic game runner reused
all existing EE objects and took 9.988 seconds.

The captured buffer family executed one/four isolated startups with 28/103
native operations, zero interpreted operations and zero native faults. Reports
and full IOP/EE RAM matched the identified diagnostic model. Its startup return
was **1**, and the isolated probe registered no threads or RPC servers; this
does not establish residency or successful operation in the game's dependency
context. Actual RPC replay, native game progression, complete replacement
lifecycle, hidden state, service/hardware fidelity and platform/game gates remain
open. The headless producer still used diagnostic IOP/VU and EE overlay paths.
Commercial images, payloads, generated catalogs and screenshots remain local.

### Native IOP in the observed game path

The twelve-module catalog was linked into the actual Monster House producer
with `PS2X_RUNTIME_NATIVE_IOP=ON` and the IOP interpreter excluded. On its
isolated virtual display it loaded ten external IRXs and all four buffer
instances. Those buffer startups returned **2** in the game context, unlike
their isolated/staged startups, which returned **1**. Captured inputs therefore
do not replace the dependency and RPC state needed to reproduce a load.

The bounded capture contains 4,096 handled RPC calls across four SIDs and
30,425,512 native operations at its last RPC checkpoint, with zero interpreted
operations and no native fault event/log. Among the diagnostic producer's
previous observations, 2,718 transactions had identical request metadata,
send bytes, result policies and receive bytes; 1,378 native observations had no
identical recorded request. No identical request had an unmatched result in
that comparison. This is observed transaction agreement, not aligned histories,
canonical state replay, hardware fidelity or a proof for unobserved requests.
The RPC capture budget was exhausted, so the complete run is not represented.

A separate staged fourteen-module sequence at zero/80,000 EE cycles per load
matched the identified diagnostic adapter's reports (apart from execution
counters) and full IOP/EE RAM, with zero native faults. It correctly failed
startup acceptance because all four buffer startups returned 1. That result
is retained as a context counterexample; RAM agreement does not turn it into
successful residency.

The game remained on its loading screen while the diagnostic EE driver
compiled additional overlays. EE closure/AOT packaging, VU integration, the
GS backend, campaign progression, fidelity, FPS and Android qualification
remain open. This producer proves observed IOP execution through the original
modules and handlers without its generic instruction interpreter; it is not a
final native game package.

### Isolated runtime IOP backend selection

The native/diagnostic constructor choice now lives in a private object target,
`ps2_runtime_iop_backend`. This keeps its flag out of the common runtime's
`flags.make`: Make dependencies previously recompiled unrelated runtime objects
even though the flag was attached to only one source. Public runtime headers
and generated EE objects do not change.

A Make-based regression switches diagnostic → native → diagnostic and checks
that the common object's bytes and timestamp stay unchanged, the backend
object changes, and the executable selects the requested implementation.
In the actual cached root build, native → diagnostic configuration took 2.850
seconds and the build took 9.056 seconds with four workers. All 52 common
runtime objects retained their bytes and timestamps. The IOP library and the
small backend still rebuild when their profile changes; the larger runtime does
not. The one-time migration of its shared flags took 34.722 seconds.

Actual-runtime HKSIF probes through both factory variants still agreed on
reports/full RAM: two native startups executed 170 native operations with zero
interpreted operations and zero faults. These measurements concern profile
iteration and bounded module startups, not complete ISO conversion latency.

## Build and run

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON
cmake --build build --target nexo_vu_snapshot_tests nexo_device_snapshot_tests nexo_vu_native_tests nexo_vu_replay nexo_vu_inspect --parallel 4
ctest --test-dir build -R '^nexo_(vu_|device_)' --output-on-failure
build/lab/nexo_vu_replay /path/to/canonical-capture 65536 100
```

The replay's explicit cycle budget must match the recorded call budget. Restore
uses continuation semantics even for MSCAL: the capture already contains the
fresh-call scheduler normalization. Re-executing a fresh call would normalize
the captured pipeline a second time. MSCNT captures must retain their pending
work. Fresh-call normalization in this runtime preserves the absolute clock.

The lab targets compile with `-O1 -fno-lto`, and IPO is disabled. The existing
runtime fast-iteration profile uses `-O1 -fno-lto` with its private VU sources
at `-O2`. There is no rebuild of generated EE game functions for a replay case.
Laboratory capture hooks and the codec enter the runtime archive only when the
lab option is enabled. The default option is off.

Root development settings now apply to the recompilation, analysis and test
targets too. Their prior release helper ignored fast iteration and enabled LTO
for 48 source units. The Linux compile-command audit now finds zero LTO units
among those project targets. Release IPO requires both fast iteration off and
`PS2X_ENABLE_RELEASE_IPO=ON`; MSVC's explicit /GL and /LTCG follow that gate too.
Only the Linux configuration has been exercised for this change.

## Capture boundary

Set `PS2X_CAPTURE_SCENE` to a private local directory and create `.vu-request`
inside it. A lab-enabled runner consumes the marker at the next VU1 MSCAL or
MSCNT call. It records canonical input/output state, code/data byte memories,
and timed PATH1 submissions. Capture and code/data writers must be synchronous.
Commercial game data belongs in ignored local build workspaces.

The state codec covers pending vector/integer/ACC writes, delayed flags, Q/P
results, readiness and resource clocks, branch history and partially transferred
XGKICK packets. Host pointers and derived decode caches are rebuilt on restore.
See the versioned documents in `schemas/` for field order and bounds. States
with a second XGKICK already issued and waiting for PATH1 use the version-2
extension; empty request slots retain the exact version-1 encoding. Tests
exercise the second pair's Upper operation, its latched source, live future
payload reads, and continuation without generic VU execution in the AOT path.

Legacy `.bin` register images remain useful only with the original host ABI.
They cannot substitute for `input-state.nexo` in this tool.

The replay observes submissions using an otherwise empty GIF receiver. It does
not restore GIF arbitration, VIF execution, GS state or VRAM; it does not render
the game. Comparisons are explicitly `tested_only`, against the same runtime.
An enclosing case manifest must bind all blobs and producer/reference binaries
with SHA-256 before it can be used as evidence.

## Compile a finite native VU bank

After the inspector has been built, translate a captured bank before execution:

```sh
python lab/generate_vu_bank.py --inspect build/lab/nexo_vu_inspect \
  --code /path/to/canonical-capture/code.bin --unit vu1 \
  --output build/lab/compiled-vu-bank.cpp --manifest build/lab/compiled-vu-bank.json
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DNEXO_VU_BANK_CPP="$PWD/build/lab/compiled-vu-bank.cpp"
cmake --build build --target nexo_vu_native_replay nexo_vu_runtime_replay --parallel 4
build/lab/nexo_vu_native_replay /path/to/canonical-capture 65536 100
build/lab/nexo_vu_runtime_replay /path/to/canonical-capture 65536 100
```

The converter derives static dependency descriptors and emits upper/lower
operations specialized by compile-time instruction constants, including the
opcode-dependent numeric and flag helpers. The native scheduler takes the
compiled entries and data memory; it has no guest instruction buffer to fetch
or decode. Unsupported entries, invalid PCs and code identity changes produce
`UNSEEN_CODE`, without an interpreter or runtime guest compiler fallback.

Eight link wrappers reject generic interpreter execution entry points in this
replay executable. A deliberate forbidden call verifies that the trap is active.
The inspector is a separate conversion target. The prototype still shares
pipeline and device helpers with the existing runtime, and the linked runtime
contains legacy interpreter code. These checks do not establish final-package
interpreter absence or native execution of the whole game. Linux with the
GNU/LLVM C++ ABI is the currently supported laboratory configuration.

## Native runtime integration

`bindNativeVu1` installs strict native callbacks into an initialized PS2Runtime
memory session, using the existing VIF MSCAL/MSCALF and MSCNT interfaces. It
copies bank descriptors and identities into owned storage. Compiled function
pointers must remain loaded. Selection compares complete microcode bytes on
every call, including after raw mutable code writes that bypass generation
counters. Ambiguous identities and malformed collections are rejected before
replacing an installation. Unknown code does not fall back to interpretation.

Native fresh execution resets pending work according to the identified runtime
model while retaining the clock. Native MSCNT retains pending pipelines. Both
propagate D/T enables and CPU-visible stop flags through the normal runtime.
The current callback horizon is 65,536 cycles. See
`schemas/nexo-vu-runtime-binding-v0.md` for ownership, reset and scope limits.

The runtime replay executable restores a normalized VU capture and drives it
through a **synthesized MSCNT command**. It reconstructs TOP/ITOP and D/T inputs
from canonical VU fields and observes PATH1 with the same empty receiver as the
standalone replay. This exercises real VIF parsing and native runtime callbacks;
it does not reconstruct the original VIF command stream, full VIF state, GIF
arbitration or GS/VRAM. Its JSON identifies the synthesized input explicitly.
Headless fixtures contain no executable EE entries and open no user window.

Both native replay executables link the same compiled bank archive, avoiding
duplicate bank compilation. Semantic generation updates both output timestamps
so unchanged outputs do not make Make rerun generation for each dependent target.

## Transport and CPU graphics checkpoints

`Vif1SnapshotCodec`, `GifSnapshotCodec` and `GsSnapshotCodec` preserve the current
runtime's incremental VIF parser/transport, queued GIF submissions, and CPU GS
frontend/backend plus VRAM. Restore is bounded and transactional. GS includes
partially assembled primitives, loaded palettes and remembered CBPs, stale
texture page bytes, partly consumed readbacks, private register values, and
latched presentation data. Host pointers and callbacks remain owned by the
receiving instance. Execution and all writers must be paused before capture.

The headless device suite splits example VIF inputs at every byte and compares
restored continuations. It tests GS/GIF continuation and corrupt states as well.
See `schemas/nexo-device-state-v1.md` for exact scope, ordering and bounds.
The older VU replay CLIs retain their documented empty GIF receiver and
synthesized input. The separate original-call replay below restores these
device codecs with the runtime's real GIF-to-GS routing. Neither comparison
is an independent graphics reference.

## Original VIF call and all observed native banks

Create `.vif-request` in the lab runner's `PS2X_CAPTURE_SCENE` directory. The
request remains pending until a completed call containing a VU callback. It
captures the literal VIF argument before parser normalization, full VIF/VU/GIF/
CPU-GS states, 16 KiB code/data memories, each actually executed code identity,
and both GIF submissions and GS deliveries. Host presentation is serialized
across acquisition; other writers must remain quiescent. See
`schemas/nexo-observed-vif-case-v1.md` for the boundary and exclusions.

```sh
cmake --build build --target nexo_vif_replay nexo_vu_inspect --parallel 4
build/lab/nexo_vif_replay /path/to/completed-vif-case 10
python lab/generate_vif_banks.py --case /path/to/completed-vif-case \
  --inspect build/lab/nexo_vu_inspect --output build/lab/vif-banks \
  --cache build/lab/vif-bank-cache
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DNEXO_VIF_BANK_SOURCES="$(cat build/lab/vif-banks/cmake-sources.txt)"
cmake --build build --target nexo_vif_native_replay --parallel 4
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10
ctest --test-dir build -R '^nexo_' --output-on-failure
```

Conversion processes the entire finite bank collection, rather than waiting for
an unknown callback and manually adding one address. Metadata reuse binds the
inspector/emitter/converter identities and full bank bytes. Corrupt cache is
reinspected; unchanged generated C++ retains its timestamp. Changing unseen
game code still requires conversion-time closure, rather than silent runtime
interpretation. The collection manifest explicitly denies closure beyond the
observed case.

The native CLI installs strict owned native callbacks and eight fatal VU
interpreter link traps. Both original-call CLIs open no user window, compare
complete canonical component states and events, and produce JSON on stdout
with diagnostics on stderr. The acquisition runner remains a laboratory
producer with legacy IOP/VU and overlay paths; it is not a final game package.

The headless launch tool supports `--runner` and explicit `--capture-scene`.
It chooses a free display after the base display option and checks that the
Xvfb lock PID belongs to its own process group before executing the game.
This avoids an installed wrapper's behavior of launching a client even when
its new server failed. An existing session's virtual server is rejected.

### Original Monster House sample, 2026-09-30

The captured original call contains nine VU callbacks and two distinct full
code banks. All ten baseline repetitions and ten conservative AOT repetitions
match the recorded VIF, VU, GIF, CPU GS/VRAM, code/data, relevant CPU status and
ordered events exactly. No interpreter wrapper fired in the native path.
These results are same-model `tested_only` evidence for one observed call.

Compiling both banks and the native CLI took 23.489 seconds under concurrent
host load, with six source compilations and zero generated EE compilations.
An earlier diagnostic game relink took 11.621 seconds using existing EE objects.
These are scoped development build measurements, not whole-ISO conversion,
whole-game gameplay or a promise of 60 FPS.

Repeated conversion of the unchanged collection used zero inspector runs,
preserved every C++ source timestamp and took 0.348 seconds. The following
unchanged native replay build took 0.307 seconds and compiled zero source
files. These measurements cover local cache reuse, under uncontrolled host
load, and exclude discovery of unobserved code.

At commit `ea497eb`, the eight laboratory suites passed 82 cases: 14 VU checkpoints, 14 device
checkpoints, 21 native VU cases, 11 original VIF cases, three Python generator
suites of six cases each, and four headless isolation cases. The rebuilt general
runtime suite passes 484/484 with DISPLAY, WAYLAND_DISPLAY and capture unset.

## Profiling the original call and CPU GS compilation

```sh
build/lab/nexo_vif_native_replay /path/to/completed-vif-case 10 --profile
cmake -S . -B build -DPS2X_FAST_ITERATION=ON \
  -DPS2X_FAST_ITERATION_OPTIMIZE_GS_CPU=ON -DPS2X_ENABLE_RELEASE_IPO=OFF
cmake --build build --target nexo_vif_native_replay --parallel 4
```

Profiling brackets actual VU callbacks, GIF submission and GS delivery using
thread-local host timers. Nested exclusive totals avoid counting GS work again
as VU work. Profiling remains outside canonical states/events, is disabled by
default, and includes observation overhead according to the documented scope.
There is no guest interpreter fallback in the native replay.

In ten instrumented repetitions of the original Monster House case, the GS
receiver used 247.990 ms out of 250.813 ms of VIF-call wall time (about 98.9%).
VU callback exclusive time was 1.296 ms. These are one-case measurements under
uncontrolled host load, not whole-game throughput.

A paired `ABBA ABBA` experiment compared CPU GS at `-O1` with `-O2` and FP
contraction disabled. Each mode replayed the original case 20 times. Median
call execution was 251.435 ms versus 212.890 ms (measured ratio 1.181), with
identical full states and events. No new graphics algorithm, hardware fidelity
proof, GPU backend, game FPS or whole-game qualification follows from this.

The kernel lives in `ps2_gs_cpu_backend`, a separate object target included in
the runtime archive. It inherits the runtime's includes, definitions and common
compile options. Its private optimization flags have their own Makefile: the
previous per-source-option placement changed `ps2_runtime/flags.make` and
recompiled 52 runtime units. This target split isolates later option changes.
The first structural migration still invalidates the old monolithic flags.
Linux GNU/Make is the configuration exercised for this change.

After that migration, switching the GS option off and on compiled exactly one
source each time, taking 2.192 and 2.742 seconds respectively. The global
runtime flags file remained byte-identical; generated EE compilation count
remained zero. Each switch replayed the original case five times with exact
state/event equality. The optimization remains an opt-in development variant.

The final CTest run passes all 55 registered groups, including the expanded 85
laboratory cases and 484 general runtime cases. Original-call reference,
native and instrumented native paths each match ten repetitions of the
recording with the split object target. The three new timing cases initially
failed, then passed after nested timing and the actual callback brackets were
implemented. They cover memory filtering, nested exclusive subtraction,
exception unwinding, disabled timing and preservation of canonical state/trace.

The external gprofng sampling attempt was rejected because the collector
reported a changed interval timer and unreliable data. Its 49 samples cannot
support whole-run CPU percentages. Direct scope measurements replaced that
attempt; matching game state alone does not approve a profiler.

For independent-reference investigation, upstream PCSX2 revision
`94d86c891b1621c0b252e4fc2e155bf90274dcc0` was acquired with source hashes and
its GPL license in ignored local reference storage. It has not executed this
canonical case. An adapter must establish its state correspondence and device
boundary before it can provide independent evidence. No Android device was
attached at the latest ADB inventory.

## Recorded checks, 2026-09-30

- Device checkpoint cycle: all six initial cases first failed against explicit
  stubs; VIF/GIF implementation passed five, then GS completed all six. The
  expanded suite passes 14/14 cases, including checksummed semantic corruption,
  transactional rejection, partial transfers/vertices, cached palettes and
  texture visibility, atomics and presentation state. Together with the other
  four lab suites this is 61 passing cases. General runtime regressions remain
  484/484 after rebuilding against the new sources, with no user display.
- A development rebuild after touching only `lab/src/gs_snapshot.cpp` took
  7.537 seconds under concurrent CPU load, compiled exactly one source unit
  and rebuilt no generated EE functions. This is a codec rebuild measurement,
  not ISO conversion time or whole-game compilation.
- Initial codec test cycle: 6 failures / 1 pass, then 7/7 passes; expanded to 10/10.
- Capture integration: the new MSCAL/MSCNT tests first failed, then 12/12 passed.
- Timed PATH1 capture: the added XGKICK test first failed, then 13/13 passed.
- Standalone replay: the three integration cases first failed against its stub,
  then all 13 tests passed with the implementation.
- Six malformed CLI invocations were rejected.
- A checksum-valid scalar deadline outside the scheduler model was first
  accepted by the decoder (the new test failed), then rejected transactionally
  after adding deadline bounds. The expanded suite passes 14/14.
- A real Monster House MSCAL capture ran for 277 VU cycles and issued 254 pairs.
  All 100 standalone repetitions matched the complete canonical output state,
  VU data memory and timed PATH1 submissions exactly.
- That sample averaged 104.589 microseconds for VU execution, including cold
  derived-code decoding and PATH1 recording. File input, machine construction,
  state restoration and final comparison were outside the measured interval.
  This is one CPU sample, not game FPS, an AOT speedup, or sustained performance.
- The diagnostic game runner was relinked in 13.408 seconds using existing EE
  objects: zero generated-source compilations. Its legacy overlay driver still
  compiles guest code at runtime; it is not eligible for final AOT acceptance.
- Initial conservative native V0: 11/11 C++ cases passed, including FMAC, suspended Q/P
  pipelines, VI branch history, stores, future XGKICK reads, interpreter traps,
  unknown entries and an overflowing PC. The latter first caused a segmentation
  fault, then was rejected before table lookup after fixing the bounds check.
- The native suite now passes 21/21 cases. Added checks cover fresh normalization,
  real VIF callbacks, ownership after source views are destroyed, changed banks
  with pending pipelines, CPU-visible D/T status, unknown identities, ambiguous
  installations, canonical runtime replay and its exact budget and D/T inputs.
  The new behavior first failed against explicit implementation stubs. The D/T
  replay check subsequently exposed overwritten enables and passed after their
  canonical inputs were reconstructed. Captured TOP/ITOP outside the VIF callback
  domain are explicitly rejected, rather than silently masked.
- A continuation test exposed a stale reference decode cache after a raw write.
  The fixture now publishes that write to the reference's generation counter;
  native selection still detects the new bytes without a generation increment.
- Semantic generation and bank generation each pass 6/6 Python cases. Together
  with the 14 codec/capture cases, the four laboratory suites cover 47 cases;
  the latest CTest run completed in 1.51 seconds.
- The existing general C++ suite was rebuilt in the fast profile and passes
  484/484 cases with DISPLAY, WAYLAND_DISPLAY and scene capture unset.
- An unchanged rebuild of all five laboratory executable/test targets completed
  in 1.534 seconds, compiled zero source units and reran the semantic generator
  zero times. This measures an unchanged incremental build, not ISO conversion.
- The real Monster House bank contains 2,048 compiled entries. Frontend analysis
  and C++ emission took 0.169 seconds; this excludes C++ compilation and linking.
- All 100 native-only repetitions of the real capture matched complete state,
  data and timed PATH1 submissions exactly, without triggering the interpreter
  traps. A changed instruction byte was rejected by the identity guard.
- Undefined-symbol audits of the generated bank and native scheduler objects
  found no generic interpreter execution/decoder imports. This is an audit of
  those two objects, not a complete executable reachability proof.
- All 100 repetitions through the real runtime's native VIF callbacks also
  matched the Monster House recording exactly. Changed-code and wrong-budget
  CLI cases were rejected. The existing reference and direct native paths each
  retained 100/100 exact matches after the shared replay refactor.
- The final integrated sample averaged 112.056 microseconds under uncontrolled CPU
  load, including synthesized VIF processing and native bank selection. It is
  not a controlled speed comparison, whole-game FPS or evidence of a faster port.

Local evidence is under `build/nexo-*.log`. The canonical game capture and its
SHA-256 manifest are in the directory named by
`build/lab/latest-monsterhouse-vu-capture.txt`. Generated captures/binaries are
ignored and must not be committed with the laboratory sources.
The native validation receipt and frozen artifacts are identified by
`build/lab/latest-monsterhouse-native-v0-evidence.txt`.
Integrated runtime replay evidence is identified by
`build/lab/latest-monsterhouse-native-vif-evidence.txt`.

## Remaining first-increment work

1. Capture/replay VIF input and hidden state at the same boundary.
2. Integrate the transport/CPU GS checkpoints into the synchronized original
   VIF case and compare GIF/GS effects with identified references. Synthetic
   checkpoint tests do not complete this integration gate.
3. Add an identified independent reference and extend V0 validation to more
   programs, both VUs, additional operations and code-upload variants.
4. Map divergences to semantic fields and causal events; current CLI reports
   only the first differing byte per output blob.
5. Validate the canonical format and execution on a second architecture.
6. Measure baseline/AOT/optimized variants on a held-out corpus with proof gates.
7. Extend runtime integration to all required banks, prove upload/code closure
   and exclude legacy guest execution from a separate final package build.

None of these outstanding gates is satisfied by matching one case against the
same runtime. Full game progression, Android and universal conversion remain
separate unfinished milestones in the root plan.
<!-- PS2NATIVE_SOURCE_END:lab/README.md -->

---

<a id="anexo-06"></a>

# ANEXO 06 — lab/PCSX2_VU_REFERENCE.md

Origem: [lab/PCSX2_VU_REFERENCE.md](lab/PCSX2_VU_REFERENCE.md). Linhas originais: **214**. Bytes: **11967**. SHA-256: `28787716badabb6f98df3fce3cb908c6eb52c7baf4d651ade8daf7b258823114`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:lab/PCSX2_VU_REFERENCE.md -->
# Independent VU reference for the original VIF case

This laboratory implements the separate VU implementation required by README
§31.1. It does not qualify the whole PS2 runtime, the complete Monster House
port, the hidden-state correspondence, or a final native package.

## Source identity and isolation

The reference is the official PCSX2 repository at commit
`94d86c891b1621c0b252e4fc2e155bf90274dcc0`. The instruction engine compiles
`VUops.cpp`, `VUflags.cpp`, and `VU1microInterp.cpp` byte-for-byte unchanged.
`VU.h`, `VUops.h`, `VUflags.h`, and `VUmicro.h` are unchanged too. GIF tag parsing
and packet sizing are unchanged slices of that revision's `Gif_Unit.h`, with
the original copyright/license header retained. The complete original header
and GPLv3 license are also staged. Source SHA-256 values are pinned in
`prepare_pcsx2_reference.py`; a mismatch fails configuration.

The adapter supplies a small surrounding environment instead of building the
whole emulator. Unsupported IRQs, asynchronous VIF wakeups, unknown opcodes,
unfinished microcalls, blocked GIF delivery, foreign-thread execution, and
unmapped starting states fail. These paths do not return invented success.
The optional reference archive only links to its laboratory executables.
Neither the native VIF executable nor final packaging depends on this engine.

Build flags are `-O1 -fno-lto -fno-strict-aliasing -ffp-contract=off`.
The explicitly selected FP policy is rounding toward zero, FTZ and DAZ enabled,
overflow clamping enabled, and the configurable ADD/SUB hack disabled. This is
an identified comparison configuration; it is not a hardware accuracy proof.
The upstream interpreter's own instruction arithmetic remains unchanged.

## Input domain and state mapping

The initial adapter accepts exactly the canonical reset VU1 snapshot. It checks
the complete byte encoding, including the hidden state, before accepting it.
Its schema-v1 reset encoding is fixed independently of the current model's
constructor; changes to that model cannot silently broaden this import domain.
It rejects a warm pipeline instead of dropping its pending writes. Microcode
and data each require 16 KiB of aligned, disjoint storage. VU0 activity and
interrupt enables are excluded from this replay's initial domain.

The Monster House original VIF capture meets this initial VU domain: cycle 0,
empty pipelines, no branch or transfer pending, VF0.w/Q/R reset bits, and all
remaining architectural registers reset. MPG and UNPACK still execute through
the current VIF parser. Each callback then executes the separate upstream VU
engine. Its registers, hidden pipelines, and monotonic VU clock persist across
callbacks. No register or cycle is overwritten to force comparison agreement.

VIF parsing, GIF arbitration, and the CPU GS remain shared components. The
reference's raw XGKICK chunk boundaries and cycles are retained. Chunks are
aggregated at the upstream EOP boundary before delivery to the shared GS;
this delivery translation is explicitly reported. Consequently a matching GS
output is evidence about VU output under this environment, not independent GS
accuracy or whole-machine scheduling accuracy. A physical PS2 trace and a
qualified relation between both hidden pipeline representations remain open.

## Build and run

Acquisition is explicit; ordinary CMake configuration does not download the
reference. Existing cached files are verified before staging. Unchanged files
keep their timestamps, avoiding another reference compilation on every run.

```sh
python3 lab/prepare_pcsx2_reference.py \
  --source build/lab/references/pcsx2/94d86c891b1621c0b252e4fc2e155bf90274dcc0 \
  --output build/lab/pcsx2-reference --fetch

cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON \
  -DPS2X_FAST_ITERATION=ON -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DNEXO_PCSX2_VU_SOURCE_DIR="$PWD/build/lab/references/pcsx2/94d86c891b1621c0b252e4fc2e155bf90274dcc0"

cmake --build build --target nexo_vif_pcsx2_reference nexo_pcsx2_vu_reference_tests -j 4
env -u DISPLAY -u WAYLAND_DISPLAY -u PS2X_CAPTURE_SCENE \
  build/lab/nexo_vif_pcsx2_reference /absolute/path/to/observed-case /new/output/directory
```

The executable runs a private, windowless runtime. It requires a complete input
case and a new output directory. Exit 0 means all explicitly compared fields
agree; exit 2 means execution completed with comparison differences; exit 1
means an input, domain, execution, or publication failure. A `.complete` byte
is written last. It means a completed laboratory observation, not certification.

`report.json` records source revision, FP/device policies, comparisons, all
architectural differences, and callback clocks. Raw artifacts include GS/VIF/GIF
snapshots, actual data and microcode, engine chunk events, VIF events using the
reference's own clock, and register projections at every callback exit.
There is deliberately no fabricated canonical NEXO hidden-state output.

Binary artifacts use the existing version-1, little-endian, CRC32 envelope:

| Magic/variant | Payload |
|---|---|
| `NEXOVPR\0` / 1 | VF[32][4] u32 bits, VI[16] u16, ACC[4] u32 bits, Q/P/I/R/MAC/STATUS/CLIP/byte-PC u32, cycle u64 |
| `NEXOPXR\0` / 1 | u32 chunk count; each chunk: cycle u64, EOP-boundary bool8, byte blob |
| `NEXOPXC\0` / 1 | u32 callback count; each callback: complete `NEXOVPR` envelope as a byte blob |

## First original-case result

The first executed original case contained 1,825,472 VIF input bytes, nine VU
callbacks, and two uploaded 16 KiB banks. Under the identified environment:

- All 168 GIF submission payloads and paths agree with the recorded runtime.
- Final VIF, GIF, CPU-GS, microcode, and VU data bytes agree.
- Final projected registers, flags, and PC agree.
- VU time differs: the reference ends at 3,777 cycles, the recorded model at
  4,024 cycles. Canonical VIF events therefore differ too.

This exposes a timing obligation instead of hiding it behind matching images.
The current replay is `tested_only`; `full_reference_qualified`,
`vu_hidden_state_relation_qualified`, `gs_reference_independent`,
`final_package_qualified`, and `whole_gameplay_qualified` remain false.
The measured host time for this one call is not a gameplay FPS measurement.

## Complete instruction issue traces

Tracing is optional and disabled by default. The following commands replay the
same original VIF input with the original callback sequence. They do not open
a window or drive a running game process. All output directories must be new.

```sh
cmake --build build --target nexo_vif_issue_trace nexo_vif_pcsx2_reference -j 4
env -u DISPLAY -u WAYLAND_DISPLAY build/lab/nexo_vif_issue_trace \
  /absolute/path/to/observed-case /new/model-trace
env -u DISPLAY -u WAYLAND_DISPLAY build/lab/nexo_vif_pcsx2_reference \
  /absolute/path/to/observed-case /new/reference-trace --issue-trace
# The identified case returns 2: its timing disagreement is retained.
python3 lab/compare_vu_issue_traces.py \
  --case /absolute/path/to/observed-case --model /new/model-trace \
  --reference /new/reference-trace --output /new/timing-map.json
```

The model records every pair, including histories longer than the legacy
512-entry diagnostic ring. Each callback has its own complete receipt and
canonical input/output state. The reference uses the pinned interpreter's
upper-dispatch logging point after its leading cycle tick and issue stalls.
The unchanged upstream core is not patched to add observations. Its shim
identifies the existing diagnostic call; diagnostic arguments remain unevaluated.

`NEXOVPI\0` uses the version-1 CRC32 envelope. Variant 1 means the model issue
point and variant 2 means the upstream dispatch point. The payload is a u32
count followed by cycle u64, byte-PC u32, lower-word u32, and upper-word u32.
Each history is bounded at 262,144 pairs and 6 MiB. The adapter's identified
coordinate relation is `model issue = reference dispatch clock - 1`. Both raw
clocks are retained; this relation describes logging phases and does not remove
stalls or end-of-program drain time.

The mapper verifies receipts, checksums, callback partitions, original callback
arguments, full code-bank bytes, and clock horizons. A changed PC, instruction,
or pair count stops timing alignment. It reports the first disagreement and
every change in accumulated issue delta, then separately reports the tail from
the last issue to callback completion. Input and output digests bind this map
to its observed artifacts.

## Observed timing decomposition

The complete original-case trace contains **3,761 identical instruction pairs**
across nine callbacks. Enabling tracing preserves the model's entire recorded
output state and event stream. The independent reference also preserves its
untraced architectural outputs, packets, and cycle count.

| Callback | Pairs | Last issue delta | Model tail | Reference tail | Elapsed delta |
|---|---:|---:|---:|---:|---:|
| 0 | 254 | 8 | 14 | 1 | 21 |
| 1 | 254 | 8 | 14 | 1 | 21 |
| 2 | 255 | 10 | 1 | 1 | 10 |
| 3 | 245 | 10 | 1 | 1 | 10 |
| 4 | 412 | 11 | 14 | 1 | 24 |
| 5 | 412 | 11 | 14 | 1 | 24 |
| 6 | 1,105 | 16 | 74 | 1 | 89 |
| 7 | 412 | 11 | 14 | 1 | 24 |
| 8 | 412 | 11 | 14 | 1 | 24 |

All values are guest cycles. For each callback, elapsed delta equals last
issue delta plus model tail minus reference tail. Totals are **96 issue cycles
and 151 tail cycles**, accounting for the entire 247-cycle disagreement.

Bank 0 first differs at byte-PC `0x148`, lower word `0x800f18f0`: an integer add
consuming VI15 after ILW. Bank 1 first differs at byte-PC `0x28`, lower word
`0x80016b70`: an integer add consuming the preceding loaded VI registers.
The model waits for its four-cycle pending VI writes. In the pinned upstream
interpreter, `_vuRegsILW` declares four cycles but `_vuTestLowerStalls` only
dispatches integer dependency stall checks for the branch pipeline. ILW writes
the upstream VI value immediately; IADD can therefore issue before that load's
declared completion. This is a difference between implementations, not proof
that either implementation's timing agrees with physical hardware.

At E termination the upstream core clears its VU-running bit before flushing
pending XGKICK data. The final packet transfer then does not advance its VU
clock. The model's `flushPipelines` advances cycles while pending transfers
drain. Two reduced cross-engine regression cases preserve these observations:
ILW followed by dependent IADD, and an E-terminated XGKICK IMAGE packet compared
with the same two-pair NOP program. They deliberately preserve timing
disagreements while checking the loaded value and completed transfer.

## Remaining timing obligations

The manufacturer's VU User's Manual v6.0, pp.44-45/168, describes integer-load
data hazards and ILW's four-cycle latency. This supports retaining the model's
load-consumer waits rather than adopting the pinned interpreter's omitted IALU
stall. It remains documentary evidence, without a new physical measurement.
The same manual's pp.52/196 describe the following-pair stall for a second
XGKICK. The runtime now latches that request while allowing its Upper to issue;
the queued checkpoint extension is documented in `schemas/nexo-vu-state-v2.md`.
[Manufacturer manual mirror](https://studylib.net/doc/25815876/vuusersmanual.158394566).

Retain exact original input, source, executable, policy, and output identities.
Use the complete trace and reduced microtests to establish the required
hardware timing and observable device boundaries before changing the model.
Do not subtract a fitted constant, reset a
clock between callbacks, or adopt every PCSX2 approximation as hardware truth.

Then expand the input-state relation beyond reset, execute on a second
architecture, acquire independent GS/device and physical timing evidence,
and proceed through the remaining IOP AOT, GPU GS, game progression, audio,
controls, saves, Android, and catalogue gates in the root README.
<!-- PS2NATIVE_SOURCE_END:lab/PCSX2_VU_REFERENCE.md -->

---

<a id="anexo-07"></a>

# ANEXO 07 — tools/ps2native/README.md

Origem: [tools/ps2native/README.md](tools/ps2native/README.md). Linhas originais: **124**. Bytes: **7562**. SHA-256: `625434f1bd747291c79364e9a316d637db0ef2ff1152a0992670687f5c323727`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:tools/ps2native/README.md -->
# `ps2native` host builder

`ps2native` is a host-side orchestrator for the current PS2Recomp analyzer and
recompiler. The analyzer/recompiler and desktop CMake configure on the build
machine; the eventual package contains generated game C++, the current runtime,
and extracted disc files. It does not promise universal PS2 game compatibility.

## Commands

```sh
python3 -m tools.ps2native inspect --iso /path/to/game.iso
python3 -m tools.ps2native inspect --iso /path/to/game.iso --json-output
python3 -m tools.ps2native build --iso /path/to/game.iso --target desktop
python3 -m tools.ps2native build --iso /path/to/game.iso --out /path/to/new-package
python3 -m tools.ps2native verify --package /path/to/new-package
```

The equivalent executable is `tools/ps2native/ps2native`. `inspect` requires
the separate `ps2iso-inspect` executable and consumes its schema-version-1 JSON
contract. `build` calls the inspector to identify the boot ELF and extract the
disc, verifies the ISO and boot-ELF hashes, then runs `ps2_analyzer` followed by
`ps2_recomp`.

The inspector's ELF inventory is preserved in `manifest.json` and summarized
as `executable_inventory`. It deduplicates identical ELF contents while
retaining their disc paths and identifies secondary MIPS `ET_EXEC` and IRX
candidates. Secondary ELF32 little-endian MIPS `ET_EXEC` files whose MIPS
`e_flags` select MIPS III are treated as EE candidates, compiled into unique
module directories, and linked with sparse runtime registration tables. This
is a conservative heuristic rather than definitive EE/IOP identification;
other MIPS executables remain inventory-only. At runtime the SIF loader
activates a matching table by normalized module path, with the newest loaded
executable owning overlapping code ranges. The boot ELF keeps its legacy
dense table while registering sparse aliases for the ISO path and staged
`boot.elf`. Candidate-specific analysis/recompile failures remain in the
manifest and do not prevent other candidates from being processed. CD path
resolution is ASCII case-insensitive and strips ISO version suffixes for each
path component. Named partial `SifLoadElfPart` loads currently fail before
guest memory is changed.

Tool discovery checks the matching `PS2NATIVE_INSPECTOR`,
`PS2NATIVE_ANALYZER`, `PS2NATIVE_RECOMPILER`, and `PS2NATIVE_CMAKE` environment
variables, command search path, then `<checkout>/build` and
`<checkout>/out/build`. Explicit `--inspector`, `--analyzer`, `--recompiler`,
and `--cmake` options override discovery. `--work-root` selects the isolated
workspace parent; each invocation receives a unique `<title>-<iso-hash>/run-*`
directory. Generated work is kept out of runtime/recompiler source trees.
`--out` chooses a package destination outside that workspace. It must not
already exist; the completed package is copied to a sibling staging directory
and published with an atomic no-replace rename. Intermediate builds and logs
stay in the isolated workspace.

The complete workspace includes extraction, per-tool logs, analyzer TOML,
recompiler output, `source_manifest.json`, and `manifest.json`. The manifest
records hashes, resolved tool paths, pipeline steps, source lists, and errors.
Completed package builds also store a SHA-256 and size for every packaged file
except `manifest.json` itself. `verify` detects changed, missing, or added
files against that inventory, while allowing the declared runtime log paths
(`ps2_log.txt` and `game/ps2_log.txt`) to be created or updated after launch.
If either path was already present in the ISO, its original packaged bytes
remain in the immutable inventory. The manifest includes a compact source-tree
fingerprint and resolved analyzer/recompiler/inspector binary hashes. These
hashes detect drift against the stored inventory; they are not a signature and
do not attest gameplay compatibility.
`native_translation_assessment` summarizes reported function counts,
skipped/stubbed functions, decode failures, unhandled instructions, and
recompiler errors for the boot ELF and each compiled secondary candidate.
`known_gaps` means the counters identify a static gap, `unknown` means the
report is incomplete, and `runtime_stubs_present` means generated runtime
stubs exist. `no_reported_instruction_gaps` means only that the reported
counters are clean; every status retains `gameplay_compatibility: unverified`.
This summary does not measure undiscovered code, dynamic overlays, IOP/VU or
device behavior, or whether a game can be completed. When emitted by the
recompiler, `function_coverage.csv` lists each discovered function's estimated
address span, processing status, and decoded instruction count. Those function
records are not a byte-complete map of executable ELF segments. The pipeline
also writes `address_coverage.csv`, partitioning each executable `PT_LOAD`
file-backed span into reported function ranges, overlaps, and unattributed
bytes; executable zero-fill is listed separately. Unattributed bytes may be
data or padding, and range attribution does not prove instruction semantics.
The boot ELF is copied to the extracted disc root as `boot.elf` so the
runtime's disc-root inference points at the extracted disc tree.

## Desktop target

Desktop packaging is a native build for the host ABI. A standalone template
under `templates/desktop/CMakeLists.txt` adds the checkout with
`add_subdirectory` and passes `PS2X_GENERATED_CODE_DIR` before the runtime is
configured. Runtime CMake removes the checked-in `register_functions.cpp`
placeholder from `ps2EntryRunner` and adds the generated sources and includes.
The CMake project and build tree live in the isolated title workspace. The
package contains `bin/ps2EntryRunner`, `game/` (the extracted disc, with a root
`boot.elf` alias), and `run-ps2native.sh` or `run-ps2native.bat`.

The wrapper currently disables the debug UI to keep the first native build path
focused. FFmpeg uses the runtime CMake default, retaining FMV support when host
FFmpeg development packages are available. It uses any already populated source dependencies in the
checkout's `build/_deps`; otherwise CMake fetches the dependencies declared by
the runtime. Host builds do not yet cross-compile to a different desktop ABI.

## Android target

`--target android` stages a per-title Gradle project, puts the extracted disc
tree under `app/src/main/assets/game/`, passes `PS2X_GENERATED_CODE_DIR` to the
Android CMake wrapper, and asks Gradle for an `arm64-v8a` release APK.
`Ps2PackageActivity` copies those assets into app-private storage before
`NativeActivity` starts; the runtime resolves
`ANativeActivity::internalDataPath/game/boot.elf`. If Gradle, JDK 17, Android
SDK platform 34, NDK 28.2, Android CMake 3.22.1, or the Activity bridge is
missing, the manifest records `status: blocked`, the exact prerequisites, and
the staged project path; no APK artifact is claimed. Embedded disc files can
make an APK larger than common distribution limits; split/OBB packaging is not
implemented.

## Current functional boundary

The pipeline covers the boot ELF and MIPS III secondary ELF candidates found
in the ISO. It does not discover/recompile arbitrary overlays or prove that
all EE/IOP, VU, GS, DMA, SPU2, firmware, save-data, and game-specific system
behavior has been implemented. A successful desktop link proves the package
compiled; gameplay compatibility still needs per-title validation.
Every generated manifest and CLI summary marks the support tier `experimental`
and gameplay compatibility `unverified` until a real-title acceptance run
exists.
<!-- PS2NATIVE_SOURCE_END:tools/ps2native/README.md -->

---

<a id="anexo-08"></a>

# ANEXO 08 — tools/iso_inspect/README.md

Origem: [tools/iso_inspect/README.md](tools/iso_inspect/README.md). Linhas originais: **42**. Bytes: **4794**. SHA-256: `b1fe117b795523561161cda37fc90635e51205a35bd275737d38589cca804473`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:tools/iso_inspect/README.md -->
# PS2 ISO Inspector

`ps2iso-inspect` is a standalone C++20 command-line tool for inspecting a local PS2 ISO9660/Joliet image. It reads disc metadata, `SYSTEM.CNF`, and ELF records for identification and inventory. Its explicit `extract` subcommand copies the disc tree to a new host directory. The input image is read-only.

## Build and run

Requires CMake 3.16+ and a C++20 compiler. There are no third-party runtime dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/ps2iso-inspect /path/to/game.iso
```

The CLI prints the volume ID, block size, every file and directory path, the SHA-256 of the input image, and the PS2 boot metadata. It locates the executable named by `BOOT2`, reports its ELF32 little-endian MIPS entrypoint and `PT_LOAD` segments, and prints the boot ELF SHA-256.

For machine-readable output, use `ps2iso-inspect --json /path/to/game.iso`. The stable top-level contract is `schema_version: 1` with `image`, `entries`, `elf_inventory`, and `boot` objects. `boot.system_cnf` contains `path`, `boot2`, `version`, `video_mode`, and `region_inferred`. `boot.elf` contains `path`, `format`, `byte_order`, `machine`, `machine_id`, `entrypoint`, `sha256`, and `load_segments`. For PS2 boot ELFs, `byte_order` is `little-endian`, `machine` is `MIPS R5900`, and `machine_id` is the ELF `EM_MIPS` numeric value `8`. Segment offsets and addresses are fixed-width hexadecimal strings; sizes, flags, and alignment are JSON numbers. Missing optional `VER`/`VMODE` values are empty strings. Parse failures write a human-readable message to stderr and return exit code 1; usage errors return 2.

`elf_inventory` has its own schema version. It scans ELF magic in all ISO files, records class, byte order, type, machine, flags, entrypoint, loadable ranges, SHA-256, and all ISO paths for identical contents. ELF32 little-endian MIPS `ET_EXEC` files outside `BOOT2` are secondary executable candidates, not assumed EE programs; their subsystem must be identified before recompilation. MIPS `ET_REL` `.IRX` records remain IOP module candidates. Inventory is discovery metadata and does not claim that secondary code is compiled or executable in the native runner.

To copy the ISO9660 tree to a new host directory, run `ps2iso-inspect extract /path/to/game.iso /path/to/new-directory`. The destination must not already exist, and its parent directory must exist. This subcommand removes terminal `;` plus numeric ISO version suffixes from physical host names (for example, `SYSTEM.CNF;1` becomes `SYSTEM.CNF`) while the inspector JSON keeps the original ISO paths. It rejects unsafe components and ASCII case-insensitive collisions after normalization before creating the destination. Extraction writes regular files and directories only; it never creates symlinks. A failed extraction removes the newly created destination tree.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests construct synthetic ISO images in the system temporary directory; no commercial disc image or game asset is stored in this project.

## Interpretation notes

- `VER` and `VMODE` are reported as written in `SYSTEM.CNF`.
- Region is a heuristic based on known PS2 serial prefixes in the `BOOT2` executable name (`SLPM`/`SLPS`/`SCPS`/`SCAJ`, `SLUS`/`SCUS`, `SLES`/`SCES`). The label explicitly says it is inferred; unknown prefixes remain unknown.
- Joliet supplementary volume descriptors are supported and preferred for path names when present. Primary ISO9660 identifiers are percent-escaped when they contain non-printable bytes.
- The ISO SHA-256 covers the entire input file, including any bytes after the volume's declared end. This fingerprints the actual image file supplied to the CLI.
- Multi-extent directory records are rejected with a clear error. Raw 2352-byte CD tracks, CUE/BIN sets, and UDF-only images are outside this component's current scope. The boot ELF must be ELF32 little-endian MIPS `ET_EXEC`; other ELF variants can appear in the inventory as unsupported candidates.
- This tool identifies and reports the boot ELF. It does not decompile/recompile the game or translate PS2 graphics, audio, input, I/O, or hardware behavior to another platform.

## Input bounds

The parser validates descriptor signatures, both-endian ISO fields, volume and extent ranges, directory record lengths, path depth, entry count, and ELF program-header/segment bounds before reading. It limits a single directory read to 64 MiB, cumulative directory data to 256 MiB, the path list to 64 MiB, the entry count to 250,000, the `SYSTEM.CNF` read to 64 KiB, and the ELF program-header table to 16 MiB. Hashing and extraction file copies are streamed in fixed-size chunks.
<!-- PS2NATIVE_SOURCE_END:tools/iso_inspect/README.md -->

---

<a id="anexo-09"></a>

# ANEXO 09 — docs/FAST_NATIVE_ITERATION.md

Origem: [docs/FAST_NATIVE_ITERATION.md](docs/FAST_NATIVE_ITERATION.md). Linhas originais: **654**. Bytes: **37581**. SHA-256: `86f38a0272f3aef1074ed5560787663947d9738d6fd507875a9adc1a6be588ca`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:docs/FAST_NATIVE_ITERATION.md -->
# Native iteration and complete static dispatch

## Development build

Use these options in the generated desktop project:

```sh
cmake -S <desktop-project> -B <desktop-project>/build \
  -DPS2X_FAST_ITERATION=ON \
  -DPS2X_ENABLE_RELEASE_IPO=OFF \
  -DPS2X_ENABLE_RUNNER_PCH=ON
cmake --build <desktop-project>/build --target ps2EntryRunner --parallel 12
```

`PS2X_FAST_ITERATION` uses `-O1 -fno-lto` on GNU/Clang Release builds.
Registration code uses `-O0 -fno-lto` and skips PCH because its compiler
options differ. Generated sources larger than 4 MiB also use `-O0` without PCH
in fast iteration, bounding optimizer cost for outliers. Other game sources
use `-O1` with PCH. PCH resolves declaration headers from the actual generated
game directory. The host pipeline supplies these options explicitly, including
when configuring an existing CMake cache.

For a final optimized package, disable fast iteration and enable release IPO.
Compilation alone does not validate game compatibility.

## Offline EE overlay banks

The experimental `PS2X_RUNTIME_AOT_EE_OVERLAYS=ON` backend consumes a hashed
`NEXO_EE_BANK_MANIFEST`. Generation and compilation happen offline; the running
game chooses existing callbacks by PC and source-byte identity and stops on
uncovered code. Its private selection flag does not affect every common runtime
object's compiler flags. The diagnostic overlay driver implementation is omitted
from the AOT backend. See [the V0 contract](../schemas/nexo-ee-aot-v0.md).

Monster House's 32 observed banks retained all 32 compiled bank objects during a
catalog-index update; that build took 10.117 seconds. The actual game relink took
12.757 seconds and reused its original EE objects. The first catalog build still
required offline compilation. These measurements do not promise whole-ISO
conversion times, complete dynamic-code coverage or gameplay compatibility.

For an isolated AOT observation, pass `--disable-overlay-driver` to
`tools/ps2native/headless_native_test.py launch` with an explicit laboratory
runner. The helper removes the inherited diagnostic compiler driver in its owned
child and records this choice. A miss must be diagnosed from full source/state
identity; registering an already registered PC cannot repair a rejected version.

## Indirect calls

The recompiler registers every decoded instruction boundary in every generated
non-stub EE function. This includes interior callback thunks in functions that
contain no indirect calls of their own. Overlaps use the most specific decoded
owner. Function starts retain their own entries, and unresolved/undecoded
addresses remain unresolved.

The generated wrapper selects the exact label from `ctx->pc`. Direct entry into
a branch delay-slot instruction uses a separate handler that executes that
instruction alone, publishes its next PC, and returns to the native dispatcher.
Normal branch execution still executes its architectural delay slot once.
A branch inside an independently entered delay slot is explicitly unsupported.

This covers decoded static code; dynamic overlays, self-modifying code, missing
instruction implementations and PS2 device compatibility require separate work.
The code remains native EE translation backed by the PS2 compatibility runtime.

## Build cost

Consecutive addresses sharing a wrapper are emitted as ranges. The generated
module registrar expands those ranges at startup; the dense dispatcher fills
exact ranges with `std::fill_n`. Holes and different owners are preserved.
The number of runtime bindings does not decrease.

Multi-file generation compares existing contents before writing each C++ source
and declaration header. Identical files retain timestamps, allowing CMake to
reuse their objects and PCH. Changed files are written and checked for I/O errors.
Combined single-file output and diagnostic CSV output are not covered by this
timestamp optimization.

## Reproduced checks (2026-09-29)

- Monster House: 698,143 decoded interior instruction boundaries registered;
  699,283 resumable entries after the existing CFG and configured targets.
- Previously missing `0x176b10`, `0x1639e0` and `0x177350` are emitted without
  their three manually added TOML entries.
- Registration C++: 136,045,196 bytes before compression, 2,253,899 after.
- Repeating generation of this Monster House workspace took 5.66 seconds under
  concurrent compilation and changed zero of 7,244 C++/header timestamps.
- `tools/ps2native/check_dispatch_fixture.py` compiles generated code from a
  synthetic ELF and executes four paths. It also verifies all 16 compressed
  dense bindings. No commercial assets or graphics window are required.
- The code-generator suite covers range holes, overlapping owners and independent
  delay-slot labels. The incremental-output regression checks both unchanged
  timestamps and replacement of modified output.

Run the relevant checks from the repository root:

```sh
cmake --build build --target ps2x_tests ps2_recomp --parallel 2
./build/ps2xTest/ps2x_tests
python3 tools/ps2native/check_dispatch_fixture.py
python3 -m unittest discover -s tools/ps2native/tests -v
```

These checks establish the dispatch/build changes. They do not certify a
playable Monster House campaign; runtime boot, rendering, audio, input and saves
must be checked on the actual native package.

## Live native EE overlays (Linux development)

Set `PS2X_NATIVE_OVERLAY_DRIVER` to the absolute path of
`tools/ps2native/native_overlay_driver.py`. A missing EE target can then be
translated from its current RAM contents into C++ and compiled as a native
shared library. The runtime registers the resulting instruction entries; there
is no per-address TOML edit or rebuild of the full game runner for this path.
Build `ps2_native_overlay` in the repository's `build` directory first.

```sh
cmake --build build --target ps2_native_overlay --parallel 4
cd <package>
PS2X_FUNCTION_TRACE=0 PS2X_TRACE_SIF_DMA=0 \
PS2X_NATIVE_OVERLAY_DRIVER=/absolute/repository/tools/ps2native/native_overlay_driver.py \
./bin/ps2EntryRunner ./game/boot.elf /absolute/path/to/game.iso
```

The driver uses a host C++ compiler with `-O0 -fno-lto`, without a shell.
`clang++` is the default. `PS2X_NATIVE_OVERLAY_CXX`,
`PS2X_NATIVE_OVERLAY_GENERATOR`, and `PS2X_NATIVE_OVERLAY_CACHE` can override
its compiler, generator and cache directory. The runner must export runtime
symbols for these libraries; the generated Linux desktop project already uses
`-Wl,--export-dynamic`.

The cache includes code bytes, entry address, tool binaries, driver source and
runtime headers. Four Monster House libraries loaded from cache in 66–67 ms
each in the SPU timing smoke. A new cross-boundary batch took 9.63 seconds.
These are batch measurements on this machine, not a full-game conversion time.
Windows, macOS and Android live compilation are not implemented by this driver.

Snapshots are normally 64 KiB. A root near the end uses an overlapping window
so that a branch and its delay slot can cross the previous boundary. Blocks are
limited to 128 instructions, with global block/instruction budgets; speculative
prefetch does not guarantee coverage of every address in a window. Each lookup
checks the owning block's bytes before reusing native code. Failed translations
are retried when the snapshot changes, including changes beyond the first
16 bytes. Unsupported instructions and branch-in-delay-slot cases remain errors.

`PS2X_FUNCTION_TRACE=0` suppresses the shared function-entry/exit file stream in
already compiled tracing builds. `PS2X_TRACE_SIF_DMA=0` suppresses verbose DMA
calls/descriptors. Other diagnostics remain available. These options avoid
rebuilding thousands of generated objects solely to turn off this tracing.
For IOP debugging, `PS2X_IOP_DEBUG_SYMBOLS=ON` adds symbols only to its emulator
and thread-kernel implementation files. It does not enable LTO.

## IOP compatibility fixes checked on 2026-09-29

- LOADFILE function 6 and the EE module-buffer helper read physical IOP RAM.
- Resident IRX entry points receive `argc/argv`, including the module name and
  NUL-separated arguments, rather than a raw byte-count/payload pair.
- Internal SDK `_sceSifSendCmd` wrappers remove their extra `mode` argument
  before dispatching to the public six-argument helper.
- IOP thread alarms retain callback GP/userdata, use a 64-bit deadline, repeat
  according to the callback return value, and support cancellation.
- Sysclib `_wmemcopy` and `_wmemset` sizes are bytes. Only complete words are
  touched; tests protect adjacent allocations and partial-word tails.
- SPU AutoDMA output uses 768 IOP clocks per 32-bit stereo PCM sample frame,
  while ordinary SPU transfers retain their existing RAM-copy timing. The game
  was observed using core 1 AutoDMA with a 2048-byte buffer. Its old 1024-cycle
  completion generated refill callbacks much faster than audio playback.
- The IOP heap reuses freed buffers and gaps between live allocations, checks
  arithmetic overflow/alignment and reports the largest free gap.
- IOMAN supports read-only host/CD files through the host VFS: open, close,
  bounded physical IOP reads, signed seek, EOF and reset cleanup. Writes return
  an explicit error. This does not implement every custom IOP device operation.

The IOP test group has four executables. The root native overlay integration
also checks cross-window delay slots and retry after a failed block changes:

```sh
cmake --build build --target ps2x_tests --parallel 4
PS2X_TEST_NATIVE_OVERLAY_DRIVER="$PWD/tools/ps2native/native_overlay_driver.py" \
./build/ps2xTest/ps2x_tests
ctest --test-dir /tmp/ps2native-iop-tests --output-on-failure
```

## SIF command tables and VIF image transfers

The EE and IOP SIF command registration helpers now mirror function/userdata
pairs into the guest command buffers. Dispatch reads those buffers, including
guest writes performed without the registration helper. This keeps the SDK's
reserved control handler visible when the RAD code allocates its own handlers.
The IOP path retains callback GP and clears handlers associated with an unloaded
module. Tests cover registration, direct writes, removal and module cleanup.

VIF DIRECT payloads are assembled before being submitted to GIF. Continuation
across DMA segments retains the declared byte count; VIF command words are not
uploaded as pixels. GIF IMAGE data can also continue across separate DIRECT
commands. Tests include TTE command words before image data, reset, independent
memory instances, and readable VIF register state. This does not establish
complete PACKED/REGLIST fragmentation or every MPG/UNPACK case.

## Packed MMI arithmetic used by movie decoding

The generated implementations of PMULTH, PMADDH and PMSUBH preserve all eight
signed halfword products in the R5900 HI/LO banks. PMFHL's five modes, PMTHL.LW,
the 128-bit HI/LO moves, PINTH/PINTEH, PADDUH/PSUBUH and PEXEH/PREVH now preserve
their lane ordering, signedness and saturation. Source values are captured
before writing aliased destinations; writing register zero still updates HI/LO
where required. These are translator fixes; they do not certify every MMI
instruction or every legacy runtime macro.

```sh
python3 tools/ps2native/check_mmi_fixture.py
```

This fixture generates a synthetic ELF, compiles the emitted C++, and executes
21 variants against independent scalar oracles, with 512 boundary/random cases
each: **10,752 native executions passed**. It checks all HI/LO banks, register
zero, aliased operands and return behavior. The previous PMULTH implementation
failed its first case. Regenerating Monster House changed 12 generated C++
files; the remaining game objects were reused.

## COP0 Count and observed native video

The scheduler advances a shared COP0 Count with its virtual EE cycle clock.
Threads and callbacks observe that shared value instead of restoring stale
copies from saved contexts. Guest MTC0 writes are retained, the 32-bit counter
wraps, and idle/VSync time also advances it. Four regressions cover wraparound,
thread switches, callback writes and idle time. Compare interrupts and
cycle-exact CPU timing are not established by this change.

Monster House's Bink clock reads Count and converts 294,912 cycles into one
millisecond. The counter previously never advanced, so the game repeatedly
displayed its first movie frame. After the clock and MMI fixes, actual native
image transfers advance through colorful Columbia intro frames, and the game
window proceeds into later publisher introductions. The capture comes from
the game's own generated code and GS transfers; no external video decoder is
substituted into the runtime. Start input is visible in `padread` diagnostics.

## Controller byte-array ABI

`scePadRead` publishes the active-low button **low byte at offset 2 and high
byte at offset 3**. This is the little-endian `unsigned short btns` field in
[PS2SDK's padButtonStatus](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/pad/include/libpad.h).
An earlier change reversed these bytes after inspecting only a boot/menu
consumer that reconstructs them with `LBU`, `SLL 8`, `OR`. That diagnosis was
wrong: the actual gameplay consumer tests Start in byte 2 and Cross in byte 3.
The reversed implementation made R1 pause the game and Enter fail to pause.
The corrected native run confirms Enter pauses and R1 no longer pauses.
Tests check literal bytes for all relevant combinations and independently check
Start/R1 and Cross/Down separation across all 16 button bits.

The raylib host backend also previously mapped WASD to D-pad buttons while
leaving both analog axes at 128. It now maps WASD to the left stick and arrows
to the D-pad, matching the existing SDK fallback. Opposing keys neutralize that
axis; diagonals can drive both axes. Two regressions cover all 16 WASD
combinations and 18 arrow/button keys through the same private mapping used by
the backend. A live `PSPadBackend::readState` capture confirmed W changes LY
from 128 to 0 without pressing Up. That verifies packet delivery, not visible
player movement in the incomplete scene.

Latest reproduced root check: **483/483 tests**. Earlier independent checks:
**4/4 IOP test groups**,
**18/18 Python pipeline tests**, the native dispatch fixture's **4 execution
cases / 16 bindings**, and the **10,752 MMI executions**. These measurements
cover the named checks, not a complete campaign.

## Memory-card exact directory queries

`sceMcGetDir` searches the last path component, even when that component names
an existing directory. An exact save-directory query now returns that directory's
single metadata entry; `directory/*` still enumerates its children. The previous
implementation silently converted existing directories into `*` searches.
Monster House received seven entries for its save directory, reported changed
card statistics and failed a subsequent save attempt. The new regression failed
before this fix and passes for relative, absolute and wildcard directory names.
This fix follows the filename-search interface documented in
[PS2SDK libmc](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/memorycard/include/libmc.h).
It does not establish directory-search continuation/pagination support.

## Scene diagnostics without rebuilding

Launch the native runner with `PS2X_CAPTURE_SCENE=/absolute/capture-directory`.
Create that directory and write an empty `.request` file inside it to request
one capture. At a guest executor checkpoint, the runner consumes the marker
and writes a unique `capture-*` subdirectory with EE RAM/context, the published
main EE context, GS VRAM,
VU1 code/data and textual register/graphics metadata. The opt-in session also
retains the bounded GS event ring. With the variable unset, capture is disabled.

```sh
mkdir -p /absolute/capture-directory
touch /absolute/capture-directory/.request
```

Captures contain game data and remain local. They are diagnostic files tied to
the host ABI, not portable or restorable savestates. Two regressions check binary
sizes, guest bytes, metadata, unchanged guest state and missing initialization.

## R5900 square-root operands

R5900 `SQRT.S` reads the radicand from FT. `RSQRT.S` computes FS / sqrt(FT).
The translator previously used FS for SQRT and computed 1 / sqrt(FS) for RSQRT.
The fix is shared by static game translation and the native overlay generator.
The operand definitions also match
[PCSX2's EE FPU implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/FPU.cpp).

Monster House instruction `0x46010044` at `0x2e4eb0` must compute F1 = sqrt(F1).
The previous generated code computed F1 = sqrt(F0), where F0 had just been set
to zero. The following reciprocal then divided by zero. A first-chapter RAM
capture showed NaNs in the collision direction and transformed support vector,
and roughly 61 million iterations in the collision loop. This establishes a
translation error. The corrected native run advances through the 3D prologue
and into Chapter One's HUD/tutorial instead of remaining in that collision
loop. Rendering remains incomplete; campaign compatibility is not established.

```sh
python3 tools/ps2native/check_fpu_fixture.py
```

The synthetic ELF fixture compiles the emitted C++ and executes seven SQRT/RSQRT
operand/alias variants over 512 cases each: **3,584 native executions passed**.
The pre-fix translator failed the first case. The root suite passed **469/469**
at that stage, and the packed MMI fixture still passed **10,752** executions. Regenerating
Monster House changed 138 generated sources; unchanged game objects were reused.
These checks cover finite operands and nonnegative square-root inputs, including
zero for SQRT. They do not certify exceptional-value behavior, FCR31 flags or
exact PS2 rounding.

## VIF1 payloads split across transfers

VIF1 now retains incomplete command headers and STMASK/STROW/STCOL/MPG/UNPACK
payloads across FIFO or DMA chunks. Later payload bytes are never interpreted
as fresh commands simply because they resemble an opcode. UNPACK's retained
length accounts for STCYCL fill-write cycles and NUM=0. Complete streams keep
their existing direct read path; only incomplete commands require concatenation.
The internal per-memory state is released by FBRST, initialization and destruction,
without changing the public memory ABI or rebuilding generated game objects.

A regression compares complete and fragmented streams for seven chunk sizes
from one to 31 bytes, including control registers, microcode, unpacked vectors
and following commands. It failed before the fix. Reset and instance-isolation
checks also pass. The root suite passed **475/475** at that stage.
VIF0 streaming and partial-transfer timing still require separate coverage.

## VIF1 V2 replication and packed color values

V2 UNPACK must write X,Y,X,Y rather than leave Z/W at their previous values.
The decoder now replicates the two decoded lanes before masking and STMOD,
for 32-, 16- and 8-bit sources, with signed/unsigned extension retained.
V4-5 expands each five-bit RGB field by three bits and the alpha bit by seven,
yielding RGB 0..248 and alpha 0/128. V4-5 continues to bypass STMOD.
These rules match [PCSX2's VIF unpack implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Vif_Unpack.cpp).
The two new regressions failed before the fixes and passed afterward:
**478/478 root tests** at that stage. They cover six V2 formats and V4-5
under all four STMOD values with nonzero ROW registers. V3's source-dependent
fourth lane, STMOD=3 ROW updates and zero-valued STCYCL lengths remain separate
questions; this is not complete VIF conformance.

## VU write chains and raw MIN/MAX operands

A branch following consecutive writes to one VI must retain the value from
before the entire write chain. The interpreter previously replaced that saved
value at each write. Monster House issues consecutive SQI instructions, then
compares the output counter with its endpoint. Losing the original counter
caused the equality to be missed repeatedly. Tests cover SQI, LQI and IADDIU
chains, alongside the existing single-write, intervening-instruction and stall
cases. Actual VI writes and their pipeline commits remain separate from the
value retained for the branch.

LOI stores raw bits. VU MIN/MAX select raw operands by encoded numeric order,
preserving denormals, signed zeros and values whose IEEE encodings resemble
infinities or NaNs. Arithmetic continues to normalize operands when consumed.
Monster House constructs GIF fields 1, 6 and 14 using LOI/MAXi; flushing those
values to zero produced malformed graphics packets. Regressions cover exact
GIF tag construction and 48 MIN/MAX vector, broadcast and immediate cases,
including aliased destinations and unchanged arithmetic flags. The semantics
match [PCSX2's VU implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/VUops.cpp).

One actual Monster House MSCAL input was reproduced locally:

| Runtime | Result |
| --- | --- |
| Before the write-chain fix | Reached 65,536-cycle limit, no program end |
| Write-chain fix alone | Advanced in 412 cycles, then stopped on malformed GIF packet |
| Write-chain and LOI/MIN/MAX fixes | Program ended normally in 438 cycles, no stop or limit |

This checks one captured graphics program, not complete game compatibility.
The installed runner has since displayed a progressing, corrupted 3D prologue
and the first-chapter movement tutorial. This does not establish playable 3D
gameplay.

## Isolated graphics replay

With `PS2X_CAPTURE_SCENE` enabled, create `.vu-request` in its directory to
capture the next fresh VU1 MSCAL execution. Its `vu-input-*` directory contains
microcode, input/output data, input/output `VU1State` and a bounded ring of the
last 512 issued instruction pairs with VI registers and flags. `trace-state.bin`
also stores the full `VU1State` for each retained issued pair, including VF,
ACC, Q/P/I and arithmetic flags. On this host each record is 664 bytes; this
is a raw host ABI format, not a portable interchange format. The metadata
distinguishes normal termination, a stop and exhaustion of the cycle budget.
No further captures occur without another marker. MSCNT's hidden pending
pipelines are not captured by this mechanism.

`tools/ps2native/replay_vu_capture.cpp`, linked against the same host runtime and
the test function table, reproduces a captured MSCAL without a game boot or
graphics window. It accepts the capture directory, an optional cycle budget
and an optional output directory. Exit 0 requires recorded normal termination;
exit 2 reports exhaustion of the budget, and exit 1 reports an error or other
stop. A low cycle count alone is not treated as success. Diagnostic inputs
remain local and depend on the host state ABI.

## Current Monster House progression

The native runner has now displayed the Portuguese autosave notice,
the Start prompt, and the main menu. Enter sends Start; X/Space sends Cross.
Selecting New Game advances into the memory-card dialogs. Left changed the
save-creation choice to Yes, and Cross confirmed it. The game created
`mc0/BASLUS-21400/BASLUS-21400` (1,204 bytes) plus its icon files, then read back
the save payload and icon data through libmc. This establishes initial file
creation and reads, not recovery of campaign progress after a fresh launch.

The game subsequently displayed a progressing but visually corrupted 3D
prologue, the loading titles “A Casa Monstro” and “Capítulo Um / Dentro da Casa”,
and Chapter One's HUD and movement tutorial. Enter pauses the chapter; R1
does not pause it. Loading an existing save displays “Carregado com sucesso”
and reaches the first chapter without repeating the long exterior prologue.
This verifies an initial save load, not arbitrary campaign save/resume.

Before the double-libm fix below, the chapter's world was mostly black. Small floor/fire fragments appeared
during its introduction; the complete environment and player movement remain
unverified. Native code loaded into RAM is
translated automatically in batches, with no manual TOML address additions.
The main-menu smoke measured warm batches around 67–81 ms and new batches
around 5–9 seconds on this host; these are batch costs, not complete ISO
conversion times.

Playable 3D gameplay, audible sound, full save/resume and Android operation
remain unverified. The current package is experimental. Logs and captures are in the
Monster House workspace's `logs/` directory; `dispatch-validation.json` records
the latest confirmed stage. In particular, `pad-corrected-menu-smoke.log`,
`native-main-menu-window.png`, and `native-create-save-dialog.png` document the
real menu/initial-save run.

The current root suite is **484/484**, with durable red/green test logs for the
SDK pad layout, VIF formats, keyboard analog mapping and double libm ABI. The latest package
contains normal native builds of these fixes; changes to private runtime files
did not require regeneration or recompilation of game translation units.
The long-running GDB session separately used temporary VIF and pad function
redirects to preserve its chapter state. Those diagnostic redirects are not
a production hot-reload facility or a fresh-launch validation of the package.

## Black-scene investigation

Both framebuffers in a first-chapter capture contain almost only RGB 0/1.
This establishes an actual rendered-output problem rather than only a window
presentation problem. Twelve valid raw GIF packets and the GS event history
show a full-screen untextured gray quad, after HUD primitives, with blending
enabled. Its alpha changed from 128 in an earlier capture to 1 later; this is
a diagnostic lead, not an established cause. Untextured packets' unused ST
fields may contain arbitrary values and are not evidence of corruption.

Thirty-two sampled MSCNT executions completed in 1,665..5,594 cycles with a
65,536-cycle budget; none exhausted it. This rules out budget exhaustion for
those samples, not for every VU program. The software GS rasterizer and VU
interpreter remain performance work. No game-specific primitive suppression
has been added to the runtime. Diagnostic samples, packet captures and the
SDK-corrected menu/pause screenshots are retained under `logs/`.

### Zero world coordinates traced back to camera projection

A bounded sample of 1,600 `GSCpuBackend::Submit` batches contains 1,340
textured triangle-fan batches for the two main framebuffers. All their world
vertices arrive with X, Y and Z equal to zero; HUD coordinates in the same
sample remain valid. A captured fresh VU1 invocation reproduces zero XYZ in
its generated GIF packets and ends normally after 7,200 cycles. These are
measured samples, not a claim about every model or VU invocation.

The invalid projection already exists in the VU input and EE RAM. Tracing its
EE matrix producer shows a valid screen transform and an incoming camera
projection containing infinities. The perspective builder receives a horizontal
field of view of about 1.303225 and a vertical field of view of zero; its vertical
reciprocal consequently becomes infinity. This localizes a bad camera input
before VIF, VU execution and GS presentation. A fresh packaged-runner launch initially sets both camera
values correctly, including approximately 1.303225 / 0.977419. A hardware
watchpoint later catches their first transition to zero through the original
camera angle conversion: `fptodp -> atan -> dpmul -> dptofp -> tanf`. The
double `atan` stub returned float bits in V0. The following double operations
consumed those bits as a tiny double, which narrowed to zero. This is a libm
calling-convention error, reproduced in a fresh runner before the fix.

A temporary debugger-only experiment omitted the observed full-screen gray
quad. It exposed the HUD more clearly but did not restore the world. No such
omission, camera constant or game-specific projection override is included in
the runtime.

Evidence: `native-gs-batches.bin`, `native-gs-batch-layout.json`,
`native-gs-batches.json`, `native-world-any-gif-packets.jsonl`,
`native-perspective-input.json`, `native-camera-projection-fields.json`,
`native-fov-start-inputs.jsonl` and the captured `vu-input-*` files under the
workspace's `logs/` directory. Raw batches, contexts and VU state are tied to
this host ABI. The debugger samples and isolated VU replay do not prove a
complete fresh-launch playthrough.


## Double libm arguments and return values

The standard double libm stubs now read 64-bit A0 (and A1 for `atan2`/`pow`)
and return the IEEE double bits in V0. They no longer read or overwrite the
single-precision FPRs. The change covers `sqrt`, `sin`, `cos`, `tan`, `atan`,
`atan2`, `pow`, `exp`, `log`, `log10`, `ceil`, `floor` and `fabs`. Float kernel
entry points retain their float argument conventions. No public runtime header,
generated source or per-game configuration change is involved.

For Monster House, the original `atan`, `sin`, `cos`, `fabs` and `floor` machine
code consumes A0's full 64-bit value, consistent with the guest's soft double
conversion helpers. A fresh pre-fix watchpoint caught camera FOV becoming
0 / 0 inside `sub_001C60D0`, after the libm conversion chain. This reproduces
the earlier bad camera independently of the long-running session's VIF/pad
redirects.

Three regressions exercise 66 unary calls, ten binary calls and four camera
angle round trips, including signed zero, precision beyond float, NaN/domain
cases and preservation of all float registers. They failed before the fix:
**480 passed / 3 failed**. Afterward the root suite passes **483/483**. These
are host-libm ABI tests, not certification of PS2 libm's exact rounding, errno
or all compiler-specific short-double variants. Symbol recognition must also
identify the calling convention for those variants before substituting a stub.

A debugger-only continuation redirected these 13 functions to the corrected
bodies and restored the immediately preceding camera values recorded by its
getter. The native camera conversion then produced roughly 0.622356 / 0.466767;
128 further perspective samples had valid vertical FOV. In 276 sampled
textured fan batches, coordinates became finite and nonzero. Rendering still
showed serious defects, including a dark environment and corrupted HUD. Omitting
the gray quad in a separate debugger experiment exposed a small character
fragment; that primitive omission is not included in the runtime. These tests
do not establish playable gameplay or a clean complete playthrough.

The normally compiled runner/runtime have been repackaged. A separate fresh
launch now displays a progressing 3D interior with floor, walls and stairs,
without function redirects, restored camera values or primitive omissions.
Graphics remain defective; corrected chapter controls and a complete campaign
are still unverified. `native-libm-fixed-fresh-interior.png` records this clean
run. Durable evidence also includes `libm-double-{red,green}-tests.log`,
`libm-double-native-build.log`, `native-fresh-fov-zero-write-{context,ee}.bin`,
`native-fresh-fov-zero-write-stack.txt`, `native-post-math-fan-batches.bin`,
`libm-double-diagnostic-redirects.json` and `native-libm-fixed-fresh-*` logs.

## Measured VU execution improvements

A 24-interruption CPU sample found 20 stacks in VU execution, two in software
GS drawing and two in the IOP ready-thread scan. This small, nonuniform sample
identified an optimization target; it is not an exact CPU-time percentage.
`native-libm-fixed-cpu-samples.json` retains the stacks.

The VU scheduler now visits only the set bits of VI dependency masks instead
of scanning VI1..VI15 for every instruction pair. It excludes constant VI0 and
preserves lowest-register selection and writeback latency. Double-width FMAC
flag calculation normalizes Q/I only when the instruction uses them. Neither
change skips guest instructions or changes a game-specific address table.

`PS2X_FAST_ITERATION_OPTIMIZE_VU` defaults to ON. For GNU/Clang fast-iteration
Release/RelWithDebInfo builds it appends `-O2` only to the three private VU
sources. Generated game code remains at `-O1`, and LTO remains disabled.
The Makefiles dry run scheduled 48 runtime/utility compilations because their
shared flags file changed, including those three VU sources; it scheduled
zero generated game or PCH compilations. Subsequent source-only edits retain
the existing short relink path.

Alternating before/after trials used three actual MSCAL captures, three trials
and 3,000 replays per variant per trial: **54,000 replays**. Each replay checked
VU state and data against its first replay; that replay also had to terminate
at the recorded PC and reproduce the recorded data. Before and after variants
had identical state/data hashes in all three captures.

| Capture | Earlier runtime, median µs | Optimized runtime, median µs | Time reduction |
| --- | ---: | ---: | ---: |
| Interior, 1,650 cycles | 433.466 | 290.029 | 33.1% |
| Earlier scene, 4,873 cycles | 1,286.710 | 872.148 | 32.2% |
| Diagnostic continuation, 2,062 cycles | 485.227 | 334.043 | 31.2% |

These are warm VU replay timings with an isolated GS instance, not full-frame
timings, cold compilation costs, VRAM comparisons or proof of 60 FPS.
The initial mask-only change reduced median time by 7.5..9.1%; the final table
also includes deferred scalar normalization and source-local `-O2`.
Evidence: `vu-{mask,lazy,final}-alternating-trials.json`,
`vu-{mask,final}-benchmark-summary.json`, and `vu-o2-desktop-build.log`.

`tools/ps2native/benchmark_vu_capture.cpp` resets the interpreter between
independent replays. `execute()` intentionally retains its absolute scheduler
clock; omitting `reset()` initially made the benchmark report nondeterminism
because the cycle counter accumulated. Restoring state uses raw byte copies
to retain the local capture ABI padding. The benchmark does not serialize
hidden MSCNT pipelines or provide a portable save-state format.

The all-VI-bits regression passed before and after the scheduler optimization.
The root suite passes **484/484**. A separate test executable linked against
the actual desktop runtime with VU at `-O2` also passes **484/484**, launched
from the project root as required by its source-reading tests. Its initial
launch from the game workspace failed the source-file lookup test; the VU
tests passed. `vu-o2-desktop-root-tests.log` records the corrected invocation.

## Isolated headless native testing

`tools/ps2native/headless_native_test.py` starts Xvfb on a free display numbered
90 or higher. It records the runner PID and private Xauthority file path in a
local session JSON, and routes only this runner's audio to a temporary PulseAudio
null sink. It does not change the default audio output. Its screenshot and
input commands verify that the live runner owns that virtual display and refuse
desktop displays. Key releases run even when a held-input test is interrupted.
No desktop window activation, desktop screenshots or physical input is needed.

Requirements on this Linux host: Xvfb, xvfb-run, xauth, xdotool, ImageMagick
`import` and a running PulseAudio-compatible `pactl` service. Run from the game
workspace, using the user's local ISO:

```sh
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py launch \
  --package package \
  --iso '/home/pedrohs/Downloads/Monster House (BR-USA) (T2.0) (www.romsportugues.com).iso' \
  --state logs/headless-native-session.json \
  --log logs/native-headless-optimized.log
```

The launch command stays alive with the runner and prints `status: ready` when
its virtual window exists. In another terminal/process:

```sh
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py screenshot \
  --state logs/headless-native-session.json --output logs/headless-checkpoint.png
python3 /home/pedrohs/Downloads/ps2-native-recompiler/tools/ps2native/headless_native_test.py press \
  --state logs/headless-native-session.json w --hold 2
```

SIGINT/SIGTERM to the launcher closes its runner/display and removes only the
silent sink it created. The session file remains as a record; a terminated
session cannot send input. The local smoke confirmed screenshots, Cross and
Start on `:90`, and rejected a `:1` session before executing any screenshot or
input command. The observed Xvfb OpenGL renderer is **llvmpipe**. Full-game FPS
in this environment must be reported separately from hardware-GPU desktop FPS.
Muted headless playback does not validate audible sound.

The fresh optimized package has now reached that same progressing interior on
`:90`: floor, walls, stairs and three animated characters are visible, with
character rendering defects. `headless-vu-o2-current-scene.png` and
`headless-vu-o2-interior-start.png` record it. No debugger redirects or camera
restoration were used. `headless-harness-validation.json` records five CLI
checks, and `headless-isolation-verified.json` confirms the runner's audio
stream is on its silent sink. Player control and stable 60 FPS remain unverified.
Four additional malformed/stale/desktop session records were rejected before
input or capture, recorded in `headless-harness-schema-validation.json`.
A 15-second sample of the progressing interior cutscene counted 23 presentation
uploads, approximately **1.53 uploads/second**, on Xvfb/llvmpipe. This poor rate
is retained in `headless-vu-o2-interior-presentation-rate.json`; the VU replay
speedup must not be presented as a 60-FPS or hardware-GPU result.
<!-- PS2NATIVE_SOURCE_END:docs/FAST_NATIVE_ITERATION.md -->

---

<a id="anexo-10"></a>

# ANEXO 10 — docs/NATIVE_HANDOFF.md

Origem: [docs/NATIVE_HANDOFF.md](docs/NATIVE_HANDOFF.md). Linhas originais: **116**. Bytes: **9526**. SHA-256: `16e1a4f5362fceb004757ec4dc2611f1c3ec375877ae1b07baba44ed9d97698a`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:docs/NATIVE_HANDOFF.md -->
# ELF to native runner handoff

Audit was run in an isolated tree at `/tmp/ps2-native-handoff-75d729c-20260928`; no production source or CMake file was edited for this test. The host build and a generated-function desktop link both succeeded. Android APK compilation was not run because this environment has no `gradle`, `ANDROID_HOME`, or `ANDROID_SDK_ROOT`.

## Existing host pipeline

`ps2_analyzer <input.elf> <output.toml>` discovers functions and writes a TOML next to the requested file. Its generator sets `general.output` to a sibling `output/` directory and `single_file_output = false` by default. `ps2_recomp <config.toml>` then emits one `.cpp` per function plus `register_functions.cpp`, `ps2_recompiled_functions.h`, and `ps2_recompiled_stubs.h` into that directory. Relevant sources are `ps2xAnalyzer/src/analyzer_main.cpp`, `ps2xAnalyzer/src/toml_generator.cpp`, `ps2xRecomp/src/runner/main.cpp`, and `ps2xRecomp/src/lib/ps2_recompiler.cpp`.

The default runtime target does **not** link the configured recompiler output directory. `ps2xRuntime/CMakeLists.txt` builds `ps2EntryRunner` from the runtime runner sources and links it to `ps2_runtime`; its source glob also picks up `ps2xRuntime/src/runner/register_functions.cpp`, which is an empty-table placeholder. A game build must add the generated function sources and headers and replace that placeholder’s table source. Otherwise the executable does not contain the generated game entrypoint.

## Reproduced smoke path

Host tools were built outside the checkout. The root `PS2X_BUILD_RUNTIME=OFF` option keeps raylib and runner dependencies out of the tool build; the analyzer still builds its required `ps2_recomp_lib` dependency.

```sh
REPO=/home/pedrohs/Downloads/ps2-native-recompiler
WORK=/tmp/ps2-native-handoff-75d729c-20260928

mkdir -p "$WORK/host" "$WORK/fixture"
cmake -S "$REPO" -B "$WORK/host" \
  -DCMAKE_BUILD_TYPE=Release \
  -DPS2X_BUILD_RUNTIME=OFF \
  -DPS2X_BUILD_TEST=OFF \
  -DPS2X_BUILD_STUDIO=OFF
cmake --build "$WORK/host" --target ps2_analyzer ps2_recomp -j2
```

For a toolchain smoke test, this Python snippet writes a 264-byte ELF32 little-endian MIPS executable with one executable PT_LOAD segment at `0x00100000`. Its two instructions are `jr $ra; nop`; this checks the handoff and is not a game-compatibility test.

```sh
python3 - "$WORK/fixture/mini.elf" <<'PY'
import os, struct, sys
out = sys.argv[1]
os.makedirs(os.path.dirname(out), exist_ok=True)
ident = b'\x7fELF' + bytes([1, 1, 1, 0, 0]) + bytes(7)
code = struct.pack('<II', 0x03e00008, 0)
header = struct.pack('<16sHHIIIIIHHHHHH', ident, 2, 8, 1,
                     0x00100000, 52, 0, 0, 52, 32, 1, 40, 0, 0)
ph = struct.pack('<IIIIIIII', 1, 0x100, 0x00100000, 0x00100000,
                 len(code), len(code), 5, 0x1000)
with open(out, 'wb') as f:
    f.write(header)
    f.write(ph)
    f.write(bytes(0x100 - f.tell()))
    f.write(code)
PY

"$WORK/host/ps2xAnalyzer/ps2_analyzer" \
  "$WORK/fixture/mini.elf" "$WORK/fixture/config.toml"
"$WORK/host/ps2xRecomp/ps2_recomp" "$WORK/fixture/config.toml"
```

Observed analyzer/recompiler result: one discovered and recompiled function, zero decode failures, zero unhandled instructions. Files produced in `$WORK/fixture/output/` were `sub_00100000_0x100000.cpp`, `register_functions.cpp`, `ps2_recompiled_functions.h`, and `ps2_recompiled_stubs.h`.

## Desktop link and run

This temporary CMake wrapper proves the missing handoff without changing the repository’s target definition. It adds generated `.cpp` files to `ps2EntryRunner`, adds the generated-header include directory, and filters out the checked-in empty registration table. It also disables optional debug UI and FFmpeg for this small test:

```sh
mkdir -p "$WORK/link"
cat > "$WORK/link/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.21)
project(ps2_native_handoff LANGUAGES C CXX)

set(PS2X_BUILD_RECOMP OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_DEBUG_UI OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_FFMPEG OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_RUNNER_PCH OFF CACHE BOOL "" FORCE)
set(PS2X_ENABLE_SCCACHE OFF CACHE BOOL "" FORCE)

add_subdirectory("/home/pedrohs/Downloads/ps2-native-recompiler" ps2x)

get_target_property(runner_sources ps2EntryRunner SOURCES)
list(FILTER runner_sources EXCLUDE REGEX "register_functions\\.cpp$")
set_property(TARGET ps2EntryRunner PROPERTY SOURCES "${runner_sources}")
file(GLOB generated_title_sources CONFIGURE_DEPENDS
     "/tmp/ps2-native-handoff-75d729c-20260928/fixture/output/*.cpp")
target_sources(ps2EntryRunner PRIVATE ${generated_title_sources})
target_include_directories(ps2EntryRunner PRIVATE
    "/tmp/ps2-native-handoff-75d729c-20260928/fixture/output")
EOF

cmake -S "$WORK/link" -B "$WORK/desktop" -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/desktop" --target ps2EntryRunner -j2
xvfb-run -a gdb --batch \
  -ex 'set debuginfod enabled off' \
  -ex 'set pagination off' \
  -ex 'break sub_00100000_0x100000' \
  -ex run \
  --args "$WORK/desktop/ps2x/ps2xRuntime/ps2EntryRunner" "$WORK/fixture/mini.elf"
```

This configures, compiles, and breaks in the translated guest function under a virtual X server.

Observed: the host build linked `ps2_runtime`, `ps2_iop`, raylib, generated function code, and the generated table. GDB hit `sub_00100000_0x100000` on the `GameThread`, confirming the guest ELF entry dispatched to the translated host function. The isolated desktop configure used `PS2X_ENABLE_FFMPEG=OFF`; on non-Windows desktop builds FFmpeg is on by default and its development libraries are found through pkg-config. Debug UI is on by default and can be disabled with `PS2X_ENABLE_DEBUG_UI=OFF`.

For an interactive run without GDB, run the same `ps2EntryRunner <mini.elf>` command under a desktop display. The smoke function returns to guest PC zero; the raylib window remains open, so the app waits for its window loop to close. In the headless smoke run, a five-second timeout returned 124 after initialization and entry dispatch.

## Required Android wiring

The Android APK should compile the **same generated title sources** with the NDK. Keep `ps2_analyzer` and `ps2_recomp` as desktop host tools; root CMake intentionally disables them under `ANDROID`. In `ps2xRuntime/CMakeLists.txt`, add a cache path such as `PS2X_GENERATED_CODE_DIR`, glob the generated directory’s `.cpp` files, remove the checked-in `src/runner/register_functions.cpp` from `RUNNER_SRC_FILES` when generated code is supplied, append the generated sources, and add that directory to `ps2EntryRunner`’s private include paths. The current Android runner target is a shared library named `ps2EntryRunner`; those source/include changes let Gradle’s existing external CMake target compile and link the game functions into `libps2EntryRunner.so`.

In `android/app/build.gradle`, read a `ps2xGeneratedCodeDir` Gradle property and pass it as `-DPS2X_GENERATED_CODE_DIR=...` in the existing CMake arguments. Build after host recompile, for example `gradle -p android assembleRelease -Pps2xGeneratedCodeDir=/path/to/generated`. Use a unique staging directory per title/build; do not copy generated files over the checked-in placeholder or include host executables in the APK. The Gradle target already names `ps2EntryRunner` and filters Android ABIs; `arm64-v8a` is the phone target, while Android `x86_64` is for Android devices/emulators and is distinct from a desktop x86-64 executable.

Generated code alone is insufficient to boot a game. `PS2Runtime::loadELF()` still loads the original guest ELF to initialize guest memory and data. The current Android README uses a device-side ELF path plus `adb push`; a self-contained APK additionally needs the boot ELF and game files packaged as assets, copied into app-private storage, and resolved to a device path. Do not pass the desktop build machine’s ELF path as `PS2X_DEFAULT_BOOT_ELF`.

The Android dependency path uses the root `sse2neon` FetchContent shim for ARM. The checked-in `ps2xRuntime/CMakeLists.txt` currently also has an in-progress x86 compile-option change that propagates SSE4.1 to the runtime/runner; the desktop link above used that working-tree change. Keep host SIMD requirements explicit when integrating generated sources. Android’s `PS2X_ENABLE_FFMPEG` default is off, so MPEG video uses the runtime’s stub-frame path unless an Android FFmpeg build is added.

## What “native” means in this build

The recompiler emits C++ for EE/R5900 functions; the desktop compiler or Android NDK turns those functions into x86-64 or ARM64 machine code. That is native host code for the translated EE functions. The generated code still operates on `R5900Context` and guest RAM, and calls `PS2Runtime` for branches, syscalls, MMIO, scheduling, graphics, audio, and other PS2 services. Other processors remain runtime subsystems: `ps2xIOP` contains an R3000A interpreter, and VU0 microprograms execute through the runtime VU core. This handoff is an AOT EE code recompiler linked to a PS2 compatibility runtime, not recovered game-engine source or a runtime-free native port.

The smoke test establishes only that one supported `jr $ra` ELF can be analyzed, translated, compiled into a native desktop runner, loaded, and dispatched. It says nothing about an arbitrary commercial title’s instruction coverage, dynamic overlays, libraries, disc reads, or full-game behavior.
<!-- PS2NATIVE_SOURCE_END:docs/NATIVE_HANDOFF.md -->

---

<a id="anexo-11"></a>

# ANEXO 11 — docs/ANDROID_PIPELINE.md

Origem: [docs/ANDROID_PIPELINE.md](docs/ANDROID_PIPELINE.md). Linhas originais: **188**. Bytes: **20388**. SHA-256: `d2939d7c41d614fd89124e571b3d72441f6ea85339f7e8405879b4e6e5513a28`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:docs/ANDROID_PIPELINE.md -->
# ISO-to-Android APK Build Pipeline

## Purpose and boundary

This document specifies a desktop-hosted build that accepts a locally supplied PS2 ISO, runs the existing analysis and static recompiler tools on the desktop, then cross-compiles the generated C++ and PS2 runtime into an ARM64 Android APK.

The recompiler and analyzer remain host programs. The APK contains the Android runtime, the generated title-specific code, and the game files selected for that package. It does not contain the analyzer, the recompiler, or a general-purpose ISO-to-APK builder.

This produces a **game-specific native build backed by the PS2 compatibility runtime**. It does not turn arbitrary PS2 engine code into source code or remove the runtime's responsibility for PS2 memory, graphics, audio, IOP, and device behavior. A title is complete only after its generated code, runtime paths, assets, and supported device behavior have been validated together.

## Implementation status (2026-09-28)

The checkout now contains `tools/iso_inspect/`, the host-side `tools/ps2native`
orchestrator, an isolated desktop CMake package template, and Android Gradle /
CMake / `Ps2PackageActivity` wiring. A synthetic adapter run has configured,
linked, and packaged the desktop runner. It does not validate a commercial ISO,
real analyzer output, or gameplay. Android packaging is staged, but this host
has no JDK 17 or Android SDK/NDK, so an APK has not been built here. The
builder records that state as `blocked` and never reports an APK when the
toolchain is absent.

## What the repository does today

| Observed in this tree | Consequence for the pipeline |
|---|---|
| The root [`CMakeLists.txt`](../CMakeLists.txt#L16-L32) defines separate `PS2X_BUILD_*` switches. When `ANDROID` is set, it disables host recompiler/analyzer/tests/Studio and leaves the runtime as the Android product. | Host analysis and NDK runtime builds remain separate toolchains. |
| [`ps2iso-inspect`](../tools/iso_inspect/README.md) reads supported ISO9660/Joliet images, reports `SYSTEM.CNF` and the boot ELF, inventories ELF contents and `PT_LOAD` ranges, and can extract the tree. | MIPS III flags are used as a secondary EE-candidate heuristic; other MIPS `ET_EXEC` and IRX files remain outside automatic EE recompilation. Raw/CUE-BIN images, multi-extent files, and some unusual layouts remain outside its verified scope. |
| [`ps2_analyzer`](../ps2xAnalyzer/src/analyzer_main.cpp#L5-L64) accepts an ELF and emits TOML. [`ps2_recomp`](../ps2xRecomp/src/runner/main.cpp#L7-L45) consumes TOML and emits C++ output; [`ps2native`](../tools/ps2native/README.md) invokes them for the boot ELF and MIPS III secondary candidates. | The boot keeps its dense table and adds sparse aliases; module paths and function symbols are namespaced. Failed secondary candidates stay in the manifest while other work continues. Partial named `SifLoadElfPart` loads currently fail before guest memory is written. |
| The analyzer's [`TomlGenerator`](../ps2xAnalyzer/src/toml_generator.cpp#L24-L61) writes an output directory beside its TOML file. The recompiler emits `register_functions.cpp`, generated function sources, and headers ([`ps2_recompiler.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1375-L1385), [`ps2_recompiler.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1771-L1881)). | `ps2native` runs those tools in a per-ISO workspace, preserves logs, and records the generated source list and hashes in its manifest. |
| [`ps2xRuntime/CMakeLists.txt`](../ps2xRuntime/CMakeLists.txt#L440-L463) builds `ps2EntryRunner` as a desktop executable or Android shared library. `PS2X_GENERATED_CODE_DIR` is the generated-source handoff. | Runtime CMake replaces the checked-in empty `register_functions.cpp` only for a per-game build; normal source files stay untouched. |
| The checked-in placeholder [`register_functions.cpp`](../ps2xRuntime/src/runner/register_functions.cpp) is replaced when `PS2X_GENERATED_CODE_DIR` is set. Runtime CMake adds generated sources/headers to `ps2EntryRunner` without modifying the checkout. | Per-title builds can run in isolated workspaces without overwriting the shared placeholder. |
| [`android/app/build.gradle`](../android/app/build.gradle) uses NDK 28.2, min SDK 28, ARM64 ABI by default, and `android/CMakeLists.txt` as an external wrapper. It passes `PS2X_GENERATED_CODE_DIR` to runtime CMake. | The host builder copies the Android project per title, stages `assets/game/`, and invokes Gradle only after checking prerequisites. |
| [`ps2xRuntime/src/main.cpp`](../ps2xRuntime/src/main.cpp) resolves Android boot ELF from `ANativeActivity::internalDataPath/game/boot.elf`. | Device boot selection uses app-private storage instead of a host path or public-storage location. |
| [`Ps2PackageActivity.java`](../android/app/src/main/java/com/ps2x/runner/Ps2PackageActivity.java) copies `assets/game/**` into app-private `files/game/**` before `NativeActivity` loads the native library. The manifest enables Java code and launches that activity. | ISO selection stays on the host; APK contains the complete staged disc tree for the runtime. Asset delivery may make APKs large; split/OBB delivery is not implemented. |
| [`android/README.md`](../android/README.md) describes `ps2native build --target android`; this checkout has wrapper properties but no Gradle wrapper executable/JAR. | Provide Gradle through `PATH`, `--gradle`, or the local wrapper-distribution cache. The builder does not install SDK/NDK components or accept SDK licenses. |
| [`ps2xStudio/CMakeLists.txt`](../ps2xStudio/CMakeLists.txt#L1-L3) builds a desktop SDL/ImGui tool linked to the analyzer, recompiler, and test library. | Studio may later front the builder, but the first end-to-end implementation should expose a deterministic CLI contract that can also be called by a GUI. |

The components above form an experimental host-to-package path, not a universal ISO converter. The C++ recompiler translates the boot ELF and selected secondary candidates into native host code that runs within a substantial PS2 runtime; successful compilation or APK generation does not establish full-game compatibility.

## Target architecture

```text
Desktop CLI or Studio front-end
  └─ host builder/orchestrator
      ├─ inspect ISO and resolve SYSTEM.CNF boot path
      ├─ extract boot ELF and stage the disc file tree
      ├─ invoke host ps2_analyzer → TOML/report
      ├─ invoke host ps2_recomp → generated C++/headers/table
      ├─ validate support gates and write package manifest
      └─ invoke Gradle + Android NDK in an isolated package workspace
          └─ game-specific APK
              ├─ lib/arm64-v8a/libps2EntryRunner.so
              ├─ generated title code linked into the native runner
              ├─ Android manifest and game files
              └─ runtime libraries and Android resources
```

The same generated title sources feed the desktop package path as well. The desktop artifact and Android APK are distinct products with their own host APIs and ABIs.

### Implemented pieces and remaining gates

1. **ISO intake and ELF inventory (implemented).** `ps2iso-inspect` identifies `SYSTEM.CNF` and the boot ELF, hashes the image, extracts supported ISO9660/Joliet trees, and inventories ELF files by content hash and disc paths. Secondary MIPS `ET_EXEC` files remain EE/IOP-ambiguous until runtime profile or stronger binary analysis classifies them. Raw 2352/CUE-BIN images and multi-extent files remain unsupported.
2. **Disc staging (implemented).** `ps2native` stages the complete extracted tree, verifies the extracted boot ELF hash, and adds a root `boot.elf` alias for runtime boot. Secondary EE ELFs and raw overlays are not yet analyzed or recompiled.
3. **Host analysis and recompilation (boot ELF only).** The builder invokes `ps2_analyzer` and `ps2_recomp`, retains their logs/config/generated sources, and records source hashes and counts. The tools do not yet provide a complete, machine-readable title-compatibility gate.
4. **Desktop and Android package handoff (implemented, experimental).** Isolated CMake/Gradle projects pass `PS2X_GENERATED_CODE_DIR` into runtime CMake, where the per-game registration table replaces the empty placeholder. Android stages assets under `assets/game/`, derives an application ID from the ISO hash, and records missing toolchain components as `blocked` instead of reporting a nonexistent APK.

### Generated-source handoff to CMake

The runner still globs checked-in sources, and now also accepts the generated directory through `PS2X_GENERATED_CODE_DIR`. Runtime CMake gathers generated `.cpp`/headers and includes them in `ps2EntryRunner`; when the option is set it filters out the checked-in empty registration table.

Keep the existing runtime source list and host-side tool targets separate. Do not configure the Android toolchain for `ps2_recomp` or `ps2_analyzer`; the Android Gradle build should only compile the runtime and generated title C++ with the NDK.

The generated [`register_functions.cpp`](../ps2xRecomp/src/lib/ps2_recompiler.cpp#L1771-L1781) is title-specific even though the repository has a checked-in placeholder with the same filename. Per-title runtime CMake links the staged version and filters out the placeholder, without copying over it. This avoids dirtying the checkout, cross-game contamination, and races between simultaneous builds.

### Android assets and boot selection

The boot ELF and disc data are staged for the runtime at launch:

1. Package the staged disc tree under Android assets and add a small hash marker for cache reuse.
2. On first launch, copy the tree into app-private files storage, preserving the relative paths the PS2 VFS/CDVD layer expects.
3. Resolve the boot ELF using `internalDataPath/game/boot.elf`; the build machine ISO path is not embedded in the APK.
4. `Ps2PackageActivity` stages the files before `NativeActivity` loads its native library. Raylib's Android app context exposes the activity through `GetAndroidApp()`, and `main()` resolves `internalDataPath/game/boot.elf` from there.

The Java activity verifies the packaged ELF and copies the tree transactionally to app-private `files/game/`. A package marker based on ISO/boot hashes avoids copying the same data on every launch. The native runner resolves that private path through raylib's `GetAndroidApp()` accessor; no machine-specific absolute path is compiled into the app.

Package the complete extracted disc tree for the MVP because disc reads, IOP modules, and overlays can be discovered at runtime. This increases APK size and first-launch storage use. If a title is too large for the chosen distribution channel, define an external-data or split-asset delivery mode; do not silently omit files. Keep BIOS/firmware outside the builder and package unless the runtime has a clean, redistributable implementation for the required service. A missing firmware requirement should be explicit in the manifest and support report.

## Builder interface and UI boundary

### CLI-first contract

The host builder currently exposes this command interface:

```text
python3 -m tools.ps2native build --iso <local.iso> --target android --out <new-directory>
```

It currently:

- validates and hashes the ISO, then identifies/extracts its boot ELF;
- records named stages in `manifest.json` and preserves tool logs/reports when a stage fails;
- supports process interruption through the host CLI;
- invokes processes with argument vectors rather than shell-concatenated user paths;
- builds under a temporary per-input workspace and publishes only a completed artifact to a previously absent destination;
- returns nonzero exit status for parse, analysis, recompile, Android build, or packaging failures;
- prints the final package path and marks gameplay as `unverified` / `experimental` until a real-title run exists.

Expected workspace structure:

```text
<workspace>/
  manifest.json
  source_manifest.json
  disc/                 # extracted game files
  analysis/             # ELF, generated TOML, and C++
  android-project/       # isolated Gradle/CMake project for Android
  desktop-project/       # isolated CMake project for desktop
  package/               # completed package before optional --out publication
  logs/
```

The `package/` directory contains the desktop runner package or Android APK. The final `--out` directory receives the completed package contents and `manifest.json`; the full workspace remains under the configured workspace root. Never put generated game files into tracked runtime source directories.

### GUI role

`ps2xStudio` can later call the same builder API/CLI to provide ISO selection, output selection, progress, cancellation, reports, and per-title profile selection. It should not duplicate analysis, codegen, or Gradle orchestration logic. The builder remains callable without Studio for automation and reproducibility.

The generated Android APK is a **player for one packaged title**. It should not contain an ISO analyzer/recompiler screen or ask the user for the desktop ISO again. The desktop builder owns ISO access and package generation. Android settings should be limited to runtime concerns such as display, input, and saves.

## Gradle, CMake, and artifact evolution

1. **Keep separate build trees:** `out/host` uses the desktop compiler; `out/android/<disc-hash>` uses Gradle's Android toolchain and NDK. Never reuse CMake cache directories across host and Android toolchains.
2. **Build host tools first:** the current root options allow a host tools-only configure by keeping recomp/analyzer enabled and setting runtime/tests/Studio off. The analyzer depends on the recompiler library, so the host build includes both `ps2_recomp` and `ps2_analyzer`.
3. **Generated-source input (implemented):** the runtime's `ps2EntryRunner` target remains the Android shared library. `PS2X_GENERATED_CODE_DIR` supplies staged title C++ and headers, and CMake excludes the placeholder registration source for per-title builds.
4. **Per-build Gradle properties (implemented for the current target):** the builder supplies a unique application ID derived from the ISO hash, plus the generated-source path and ARM64 ABI as separate build properties. The display label remains generic and signing uses the debug key for local builds.
5. **Target packaging:** MVP output is `arm64-v8a`. Android `x86_64` may be added for Android emulators but does not create a Windows/Linux desktop executable. Build desktop x86-64 separately using the host runtime target.
6. **Pin the toolchain:** Android pins AGP, SDK, NDK, and CMake; the repository has wrapper properties but no wrapper executable/JAR. The builder can use Gradle on `PATH`, `--gradle`, or a cached distribution, and checks for JDK 17 before invoking it.
7. **Signing:** current release config uses Gradle's debug signing config. That is suitable only for local prototypes. Any distributable release needs a signing key owned by the publisher/user and must never place that secret in the repository or generated build logs.

For desktop artifacts, build the generated source and runtime as a separate target using the desktop toolchain. Reuse the package manifest and disc tree, but do not try to put desktop x86-64 binaries in the Android APK's ABI directories.

## Open-world compatibility plan

Use versioned support records keyed by disc hash plus boot ELF hash/region. A serial number alone is not enough to distinguish revisions. A profile may describe verified boot-file selection, known overlays, compatibility patches, firmware requirements, and tested runtime features. Every patch needs an address, expected original bytes, reason, and exact identity match; never apply a title patch to a different hash by default.

Publish support as explicit tiers:

- **Automatic:** the generic pipeline extracted, analyzed, recompiled, packaged, and launched this tested revision without a game-specific patch.
- **Profiled:** a checked-in or user-supplied version-specific profile is required; it is still one-command after selection.
- **Experimental:** APK generation succeeds, but one or more runtime paths have not passed full-game acceptance.
- **Unsupported:** extraction, static analysis, generated-source coverage, firmware/device requirements, or runtime behavior blocks packaging or execution.

The project can expand its supported-title set over time, but it must not market the first working APK as proof that arbitrary ISOs are plug-and-play. This tree currently recompiles an ELF into C++ and runs it inside a substantial PS2 runtime. Dynamic overlays, unrecognized code paths, runtime-loaded IOP modules, unusual disc access, and device timing can require additional runtime support or per-title work.

## Acceptance checkpoints

These remain title-level release gates. The desktop CMake plumbing smoke test does not satisfy the real-ISO or gameplay gates.

### A. Host pipeline

- A clean desktop build produces `ps2_analyzer` and `ps2_recomp` without configuring Android.
- Given a supported test ISO, ISO intake resolves the boot ELF and emits an extraction manifest with ISO/ELF hashes and relative file paths.
- Analyzer and recompiler run in the per-ISO workspace and emit all expected generated code, headers, registration table, and reports.
- A malformed ISO, missing boot ELF, analyzer failure, or missing Android SDK produces a stage-specific error and no final APK. Unsupported-instruction analysis is retained in the recompiler log, but a complete compatibility gate is not implemented yet.
- Two builds for different ISO hashes can run concurrently without writing into or overwriting the source checkout or each other's outputs.

### B. Android package build

- Gradle builds the Android runtime and generated title code with the NDK for `arm64-v8a`; host `ps2_analyzer`/`ps2_recomp` are absent from the APK.
- APK inspection confirms a game-specific runner library, package manifest, and complete staged game-data tree; no path from the build machine is embedded as the runtime boot path.
- The generated package installs side-by-side with another generated title package using a distinct deterministic application ID.
- A physical ARM64 device launches the APK offline, stages assets into app-private storage, resolves the boot ELF, and reaches the first defined gameplay checkpoint.

### C. Title support claim

- The tested ISO revision has a published support record and build report with profile and firmware requirements.
- The title passes its defined end-to-end acceptance route: launch, menus, controls, level/session transition, audio, save, process restart, and save reload. For a “complete” claim, all known game modes and content paths must be accounted for, not only boot or one playable scene.
- Every incomplete subsystem or known crash remains visible as experimental/unsupported; APK generation alone is not a compatibility pass.

## Unsupported assumptions to resolve before implementation

- **ISO coverage:** current intake covers supported ISO9660/Joliet directory records and boot ELF paths; raw/CUE-BIN, multi-extent files, and unusual disc layouts need explicit fixtures/support.
- **Disc/VFS integration:** whether the existing runtime can map an extracted disc tree to every CDVD/IOP access pattern required by the selected title. Full ISO-tree extraction does not itself implement CDVD timing or device behavior.
- **Generated-source integration:** `PS2X_GENERATED_CODE_DIR` is implemented for desktop and Android; it still needs validation against real commercial-title output.
- **Android asset bootstrap:** `Ps2PackageActivity` copies packaged assets into app-private storage; device boot and complete disc access still need a real ARM64 acceptance run.
- **Runtime completeness:** the generated C++ and existing PS2 runtime must support the selected game's EE/VU/GS/audio/IOP paths. A successful C++ compile does not prove gameplay completeness.
- **Distribution mode:** whether packages are local sideloads or store releases; asset size handling and signing differ.
- **Build prerequisites:** the desktop machine needs a compatible JDK, Gradle, Android SDK/NDK, CMake, and access to dependencies fetched by CMake/Gradle. Offline and reproducible dependency caching are future requirements unless explicitly added to MVP.
<!-- PS2NATIVE_SOURCE_END:docs/ANDROID_PIPELINE.md -->

---

<a id="anexo-12"></a>

# ANEXO 12 — docs/RECOMPILER_GAPS.md

Origem: [docs/RECOMPILER_GAPS.md](docs/RECOMPILER_GAPS.md). Linhas originais: **74**. Bytes: **16480**. SHA-256: `167fe59ac6decdf2bcddeb5b00f4ed95ca64fac1a2d997c6bf841277c79b4067`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:docs/RECOMPILER_GAPS.md -->
# PS2 recompiler: current pipeline and coverage gaps

Audit baseline: repository commit `75d729c` (`Feature/iop emulator (#244)`). The ISO intake, `ps2native` host builder, and per-title package handoff described below were added after that baseline in the current working tree. This note records that integration as well as the remaining compatibility gaps; it does not claim complete gameplay for any title.

## Current path through the code

```text
PS2 ISO
  -> ps2iso-inspect (SYSTEM.CNF, boot ELF metadata, disc extraction)
  -> tools/ps2native (hash checks, workspace and package orchestration)
  -> ps2_analyzer (boot ELF -> TOML)
  -> ps2_recomp / PS2Recompiler::initialize()
  -> ElfParser (sections, symbols, relocations, DWARF/Ghidra map, heuristics)
  -> R5900Decoder + InstructionTranslator / CodeGenerator
  -> generated C++ functions + register_functions.cpp + headers
  -> isolated desktop CMake build or Android Gradle/NDK build
  -> ps2EntryRunner / PS2Runtime::loadELF() + static guest-address function table
```

The recompiler accepts one ELF and TOML configuration per invocation; the host `ps2native` layer resolves `SYSTEM.CNF`/`BOOT2`, extracts the ISO, recompiles the boot ELF, and now runs additional invocations for secondary ELF32 little-endian MIPS `ET_EXEC` files whose MIPS `e_flags` declare MIPS III. This is a conservative EE-candidate heuristic and does not prove title playability.

| Stage | Current implementation and boundaries |
|---|---|
| CLI/config | `ps2xRecomp/src/runner/main.cpp` runs `PS2Recompiler` after `initialize()` using one TOML config. `ps2xRecomp/src/lib/config_manager.cpp` reads `[general].input` and output options. The CLI usage is `ps2recomp <config.toml>`. |
| ELF input | `ps2xRecomp/src/lib/elf_parser.cpp`: `ElfParser::parse()` uses ELFIO and rejects non-`EM_MIPS`; then `loadSections()`, `loadSymbols()`, `loadRelocations()`, and `loadDebugFunctions()`. It can fall back to PT_LOAD-derived sections when section headers are absent. It parses ELF metadata and executable bytes, not an ISO container or the complete PS2 filesystem. |
| Function discovery | `ElfParser::extractFunctions()` combines sized `STT_FUNC` symbols, DWARF/Ghidra map entries, and fallback discovery. `ScanFunctionStartsFallback()` in the same file seeds the ELF entry and direct J/JAL targets, then uses `ScanMaterializedCodeAddresses()`, `ScanDataFunctionPointerTables()`, and `ScanAdjacentLeafThunkRuns()`. It fills missing bounds from known starts/section ends and clamps ranges to sections. These are useful heuristics, not a proof that every callable address is found. |
| Control flow | `ps2xRecomp/src/lib/ps2_recompiler.cpp`: `recompile()` decodes discovered functions, then calls `discoverAdditionalEntryPoints()`. This collects internal J/JAL targets and configured hints, uses `ps2xRecomp/src/lib/control_flow_analyzer.cpp`/`CodeGenerator::collectInternalBranchTargets()` for branch, indirect fallback, and jump-table candidates, and `ResliceEntryFunctions()`/`CollectInternalEntryTargets()` for extra entry slices. The result still depends on static target recovery. |
| R5900 decode | `ps2xRecomp/src/lib/r5900_decoder.cpp` initializes Rabbitizer’s R5900 instruction model, calls `RabbitizerInstructionR5900_processUniqueId()`, and fills the project `Instruction` record in `ps2xRecomp/include/ps2recomp/instructions.h`. Rabbitizer supplies decoding/disassembly metadata; the project translators implement semantics. |
| Translation/code emission | `ps2xRecomp/src/lib/instruction_translator.cpp` routes to `special_translator.cpp`, `regimm_translator.cpp`, `cop0_translator.cpp`, `fpu_translator.cpp`, `mmi_translator.cpp`/`mmi_translation_helpers.cpp`, and `vu_translator.cpp`/`vu_translation_helpers.cpp` (all under `ps2xRecomp/src/lib/`); ordinary integer and memory operations are also handled in the instruction translator. `code_generator.cpp` is the translator façade; `function_emitter.cpp` and `control_flow_emitter.cpp` emit C++ for instructions, branches, and delay slots; `function_table_emitter.cpp` emits the static function-address table. `ps2_recompiler.cpp::generateOutput()` writes `ps2_recompiled_functions.cpp` (or per-function sources), `register_functions.cpp`, `ps2_recompiled_functions.h`, and `ps2_recompiled_stubs.h` into the configured output directory. |
| Unsupported translation | `CodeGenerator::emitUnhandledInstruction()` in `ps2xRecomp/src/lib/code_generator.cpp` records the unsupported instruction and emits a `throw std::runtime_error(...)` into generated C++. Several translator switches route unknown encodings there. A successful parse or source-generation pass therefore does not establish complete ISA coverage or playable behavior. |
| Runtime boot/dispatch | `ps2xRuntime/src/lib/ps2_runtime.cpp::loadELF()` loads a 32-bit little-endian `EM_MIPS`, `ET_EXEC` ELF from its PT_LOAD segments, zeros BSS, and registers executable regions. `lookupFunction()` indexes the generated static function table. `dispatchGuestBranch()` reports an absent target; its missing-function policies do not decode or execute arbitrary EE code. |
| ISO/package handoff | `tools/ps2native` invokes the ISO inspector, analyzer, and recompiler, then stages generated sources and the extracted disc into isolated desktop or Android projects. Runtime CMake accepts `PS2X_GENERATED_CODE_DIR`; the Android activity copies `assets/game/` to app-private storage before native startup. Android APK production still depends on the host JDK/Gradle/SDK/NDK toolchain, and the current environment did not produce an APK. |

## Specific coverage gaps visible in this tree

### Disc, modules, and assets

- Basic ISO intake and full disc-tree extraction are implemented for supported ISO9660/Joliet images. The inspector inventories ELF contents by SHA-256, retains every ISO path, records `PT_LOAD` ranges, and classifies MIPS III `ET_EXEC` files as secondary EE candidates by a documented heuristic. The builder compiles those candidates into isolated directories and creates module-scoped registration sources; the runtime activates the module matching the normalized SIF path and gives the newest load ownership of overlapping executable ranges. CD lookup resolves ASCII case differences and strips ISO `;version` suffixes per path component. The boot ELF keeps its legacy dense table and also registers sparse bindings for its ISO aliases and staged `boot.elf` path. Other MIPS candidates remain unresolved, and a MIPS III flag is evidence rather than proof of EE ownership. Raw 2352-byte/CUE-BIN inputs, multi-extent files, unusual layouts, and arbitrary overlays remain outside the verified scope.
- Initial runtime boot starts from the boot ELF. The ISO manifest feeds MIPS III secondary candidate paths through the analyzer and recompiler; a failed candidate is retained with its error in the manifest and does not stop other modules. Generated descriptors are registered per `PS2Runtime` instance before boot. The EE SIF loader activates a matching module only after a full ELF load. Named section loads through `SifLoadElfPart` currently return an error before writing guest memory; they are not silently treated as full loads. Arbitrary archive-embedded executables, relocated `ET_REL` modules, and all overlay formats remain unsupported.
- Android now has a per-title asset packaging and startup staging path. It is still an experimental local APK build path: Android packaging was not built in this environment, large-game split/OBB delivery is absent, and packaging does not establish gameplay compatibility.

### Entry points, overlays, and dynamic code

- Retail symbol/DWARF data may be incomplete. The fallback scans recover many common function patterns, and the analyzer also detects some jump tables, but computed function pointers, unusual thunks, split functions, decompression-at-boot, and game-specific overlay formats can evade bounded pattern heuristics. `ps2xAnalyzer/src/analysis_passes.cpp::detectJumpTables()` uses recognizable instruction sequences; `ElfAnalyzer::isSelfModifyingCode()` delegates to a pattern heuristic. These are signals, not complete proofs of all targets or writes.
- `ElfParser::isExecutableSection()` deliberately excludes `.vutext*` and `.DVP.overlay*` from EE/R5900 function discovery because those section payloads use the VU ISA. The runtime executes VU0 through `PS2Runtime::executeVU0Microprogram()`/`m_vu0.execute()`, but the EE static function scanner has no generic overlay model that enumerates, relocates, and recompiles every later-loaded code block.
- Runtime code regions are registered while loading the main ELF, and the SIF loader also records secondary `PF_X` ranges as module ownership. Generated module function maps are sparse and instance-local; a loaded range with no exact function does not fall back to an older boot/module mapping. `PS2Memory` has VU0/VU1 code-generation counters for VU code writes, but there is no general EE recompile/invalidate-on-write path. An EE call to an unregistered target reaches `reportMissingFunction()`/the missing-function handler, not a guest EE interpreter or JIT.
- `register_functions.cpp` checked into `ps2xRuntime/src/runner/` remains an empty default table. For per-title builds, runtime CMake now requires the generated directory's table, excludes the placeholder, and compiles the generated sources without modifying the checkout. The dispatch table still covers only the boot ELF's statically discovered functions.

### ISA and host code

- The output is native host C++ source that a host compiler can turn into machine code; it is not a separate x86-64/ARM64 machine-code emitter or host-neutral IR. Vector helpers in `instruction_translator.cpp` use `__m128`/SSE-style operations, with root CMake fetching `sse2neon` and defining `USE_SSE2NEON` for ARM targets. This provides a compatibility route, but does not by itself prove exact R5900 lane, flag, exception, or timing semantics on both hosts.
- R5900 integer, COP0, FPU, MMI, and VU0 macro-op translators exist, which is meaningful coverage. Their explicit unhandled paths and runtime dependencies remain per-instruction and per-title compatibility gates. `RecompilerReporter` reports discovered/processed/recompiled/skipped functions, decode failures, unhandled instructions, stubs, and correctness-critical failures. The host manifest now aggregates these counters per compiled ELF under `native_translation_assessment`; its scope is the boot ELF and heuristic MIPS III `ET_EXEC` candidates. `no_reported_instruction_gaps` only means the reported static counters are clean. The report does not account for undiscovered or dynamically generated code and does not certify a complete runnable game.
- `ps2Runtime` models useful systems (EE scheduling, memory/MMIO, GS/GIF, VIF1, VU, audio, VFS, and IOP integration), but many syscall/library compatibility handlers live under `ps2xRuntime/src/lib/Kernel/Stubs/` and `Syscalls/`; some cases still route through `TODO_NAMED` or unimplemented handlers. `ps2xIOP` implements an R3000A core and selected imports/modules, not every game’s IRX, RPC service, CDVD behavior, or device profile.

### Targets and build dependencies

| Target/host path | Build graph and relevant dependencies |
|---|---|
| `ps2_recomp_lib`, `ps2_recomp` | `ps2xRecomp/CMakeLists.txt`; C++20. FetchContent dependencies: ELFIO `Release_3.12`, toml11 `v4.4.0`, fmt `12.1.0`, libdwarf `v2.2.0`, Rabbitizer `1.14.3`. |
| `ps2_analyzer_lib`, `ps2_analyzer` | `ps2xAnalyzer/CMakeLists.txt`; links `ps2_recomp_lib` and nlohmann/json `v3.11.3`. |
| `ps2_iop`, `ps2_runtime`, `ps2EntryRunner` | `ps2xIOP/CMakeLists.txt` and `ps2xRuntime/CMakeLists.txt`; runtime links `ps2_iop`, `ps2_host_backend`/raylib, and optional FFmpeg. `PS2X_ENABLE_FFMPEG` defaults on except Android, where it defaults off; enabled Windows builds fetch prebuilt FFmpeg, while other enabled builds require pkg-config libraries. `ps2EntryRunner` is an executable on desktop and a shared library on Android. |
| `ps2x_tests`, `ps2xStudio` | Optional top-level targets. Studio fetches SDL2 and ImGui components and requires OpenGL; test sources cover CPU/recompiler/runtime areas, but a passing build is not game-compatibility evidence. |
| ARM/Android | Root CMake fetches sse2neon for ARM targets. Android Gradle uses SDK 34, NDK `28.2.13676358`, CMake `3.22.1`, min SDK 28, and ARM64 for per-title packages; the README expects Gradle 8.7+ and JDK 17. Android disables host recompiler/analyzer/tests/Studio and builds only `ps2EntryRunner`. No runnable Gradle wrapper is checked in. |

## Prioritized work toward broad title coverage

1. **Complete reproducible package acceptance.** The host builder and desktop package path now exist, and a synthetic ELF has been translated, linked, and dispatched in the desktop runtime. Add real-title acceptance on desktop and Android from a clean environment; retain toolchain, ABI, source-hash, and gameplay status in each manifest. Android APK generation is not verified yet.
2. **Broaden module discovery and validate dispatch.** The MIPS III candidate path, per-module symbols, sparse registries, and SIF activation are implemented. Establish explicit EE/IOP classification across retail images, handle path aliases and modules loaded from buffers, and exercise overlays that reuse addresses. Acceptance: a fixture loads and calls a second EE ELF through normal SIF dispatch, followed by an instrumented retail title trace.
3. **Extend the initial coverage summary into a semantic executable-range ledger.** The host manifest aggregates recompiler function/instruction counters and links each ELF to `function_coverage.csv` and `address_coverage.csv`. The latter partitions executable `PT_LOAD` file bytes by heuristic function spans, showing overlaps and unattributed bytes; zero-fill is separate. Unattributed bytes may be data or padding, and a function span does not prove the instructions were translated correctly. Still needed: distinguish translated instructions from embedded data, configured stubs, VU payloads, unresolved code/data, indirect targets, and excluded ranges; preserve relocation/symbol provenance; report indirect branches, jump-table candidates, and heuristic confidence. Use a corpus of legally sourced test ELFs and instruction-level differential checks against a trusted R5900 reference before expanding title claims. Acceptance: an artifact build fails or carries an explicit unresolved-address report rather than silently treating gaps as covered.
4. **Model module/overlay and EE code lifecycle.** Add explicit parsers for supported overlay/module formats and their relocation/load events. Track writes to EE executable pages and correlate runtime-observed entry targets with the static map. For any target that cannot be known before execution, either add a documented runtime translation fallback or mark that game outside the strict AOT support envelope; do not count VU code-generation counters as EE coverage. Acceptance: instrumented title traces show every executed EE target maps to generated code or a recorded, handled unsupported case.
5. **Close instruction and hardware semantics with evidence.** Use reporter output plus instruction tests to prioritize unhandled R5900/MIPS III, MMI, COP0/FPU, and VU0 macro forms. Differentially validate register lanes, overflow/exception paths, delay slots, branch targets, memory/MMIO, and host SIMD behavior on x86-64 and ARM64. In parallel, implement title-observed missing syscalls, IRX/RPC imports, GS/VIF/GIF/DMA, CDVD/IPU, SPU2, and input behavior. Acceptance: per-title boot-to-gameplay traces and frame/audio comparisons, not just successful compilation.
6. **Finish platform delivery after compatibility work.** Isolate the SSE compatibility layer behind a validated host SIMD interface; add Android touch controls (the current `ps2_android_runtime.cpp` marks them TODO), large-game asset delivery, and a PC launcher/configuration path. These are deployment steps after code coverage and runtime behavior have measurable support.

This repository now has an experimental ISO-to-package path over its static recompilation foundation: ISO inspection/extraction, boot-ELF analysis, generated C++, and isolated desktop/Android handoff. It still does not make arbitrary PS2 titles plug-and-play. Secondary code, dynamic overlays, missing instruction/system behavior, and actual title/device validation remain open work.
<!-- PS2NATIVE_SOURCE_END:docs/RECOMPILER_GAPS.md -->

---

<a id="anexo-13"></a>

# ANEXO 13 — android/README.md

Origem: [android/README.md](android/README.md). Linhas originais: **41**. Bytes: **2012**. SHA-256: `7ed9c4f1ef5c1eef258b3d00265f73b839dba6f22fcf851f24c8825299e20968`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:android/README.md -->
# Android runner

## Building a game package

Run the desktop builder from the repository root:

```sh
python3 -m tools.ps2native build --iso /path/to/game.iso --target android --out /path/to/new-package
```

The host needs JDK 17, Gradle 8.7 or newer, Android SDK platform 34, NDK
`28.2.13676358`, and Android CMake `3.22.1`. Set `ANDROID_SDK_ROOT` or
`ANDROID_HOME`; pass `--gradle /path/to/gradle` when Gradle is not on `PATH`.
The builder also checks a cached Gradle wrapper distribution under
`~/.gradle/wrapper/dists`. This checkout has `gradle-wrapper.properties` but no
wrapper scripts or wrapper JAR, so the builder does not rely on `./gradlew`.

The builder creates a separate project under the per-ISO workspace, copies the
extracted disc to `app/src/main/assets/game/`, and passes generated C++ through
`PS2X_GENERATED_CODE_DIR`. It does not write title files into the checked-in
Android project. Each APK gets an application ID derived from the ISO SHA-256
prefix and a stable output name such as `game-<hash>-arm64.apk`.

If Gradle, JDK, SDK, NDK, CMake, or `Ps2PackageActivity` is missing, the command
records the exact blocker and staged project path in `manifest.json`, exits as
`blocked`, and does not report an APK artifact. The builder does not install
SDK components or accept Android SDK licenses on the host's behalf.

## Runtime asset path

`Ps2PackageActivity` copies `assets/game/` into
`getFilesDir()/game/` before starting the native library. The runtime resolves
the boot ELF at `ANativeActivity::internalDataPath/game/boot.elf`; no ISO path
or external-storage permission is needed. The APK embeds the complete extracted
disc tree, so large games produce large APKs. Split/OBB delivery is not
implemented.

The checked-in Gradle project can build a generic runtime with the placeholder
registration source when `PS2X_GENERATED_CODE_DIR` is empty. The host builder is
the supported route for a game-specific generated build. Runtime output is
available through `adb logcat -s ps2x`.
<!-- PS2NATIVE_SOURCE_END:android/README.md -->

---

<a id="anexo-14"></a>

# ANEXO 14 — ps2xIOP/README.md

Origem: [ps2xIOP/README.md](ps2xIOP/README.md). Linhas originais: **134**. Bytes: **7274**. SHA-256: `3675db68978b2d72b3ecdaeebeea07a8eee2700446a48c8b50ae9f389aa5e521`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:ps2xIOP/README.md -->
# ps2xIOP

`ps2xIOP` runs original IRX modules on an R3000A interpreter, with a virtual
IOP kernel providing imports without a PS2 BIOS. The C++20 static library
`ps2_iop` / `ps2x::iop` is linked into `ps2xRuntime`.

There is now an initial native AOT path with an internal laboratory bank ABI.
Constructing the subsystem with an `IopNativeProgram` requires compiled entries;
missing or changed instructions fail with `UNSEEN_CODE` and never use the
interpreter. `PS2X_IOP_ENABLE_INTERPRETER=OFF` excludes the diagnostic interpreter
from the library and makes the default constructor require a native bank too.
The runtime defaults to the diagnostic configuration. An experimental native
IOP catalog option has passed original HKSIF startup through its actual adapter;
complete commercial game execution remains unqualified.

## Execution policy

Game-specific IOP code executes from IRX modules. There is no game-profile
selection or native profile-plugin loader. A physical IRX RPC server is
authoritative for its SID.

Generic HLE services remain available when no loaded IRX provides an endpoint:

| Service | SID | Activation | Implemented operations |
| --- | --- | --- | --- |
| LOADFILE | `0x80000006` | IOP boot/reset | Version query and path based module load |
| MCSERV | `0x80000400`, `0x80000480` | Recognized module load | Memory-card RPC subset |
| LIBSD | `0x80000701` | Recognized module load | Runtime audio dispatch |
| DBCMAN | `0x80001300` | Recognized module load | Version query |

LOADFILE is part of the no-BIOS boot profile and is registered whenever the
IOP resets. Its module-load RPC delegates to the same IOP module manager used by
the direct runtime calls. The other HLE services are dormant before module
load and after reset or the final module stop. Unsupported LOADFILE operations
remain unhandled and appear in its counters; this is not a full LOADFILE
protocol implementation yet. Unknown modules fail to load; unknown RPC SIDs
remain unhandled.
Games previously using TSNDDRV, CRI DTX, CLFILE, SOUND or SDRDRV profiles now
require their IRX modules and support for the imports and hardware they use.

## Lifecycle and transport

- `reset()` clears loaded modules, HLE service state and emulator state.
- `loadModule(...)` / `loadModuleBuffer(...)` load and start an IRX.
- `stopModule(...)` releases a module and its owned state.
- `runEeCycles(...)` advances the IOP from EE cycle accounting.
- `selectRpcAbi(...)`, `handleRpc(...)` and `onSifTransfer(...)` connect SIF transport.

IOP RAM is separate from EE RAM. The transport copies data through the IOP
memory accessors; SIF notifications do not mirror bytes into equal-numbered EE
addresses. `RpcResult` describes completion and dispatch actions for the runtime.

Link with `target_link_libraries(my_runtime PRIVATE ps2x::iop)`. The public API
is [iop_subsystem.h](include/ps2x/iop/iop_subsystem.h); `PS2Runtime` owns its
subsystem and host adapter.

## Diagnostics and tests

`debugSnapshot()` exposes emulator cycle/instruction counts, loaded module,
thread and RPC-server counts, generic service metrics and load diagnostics.
It also reports native instructions, interpreted instructions, and whether a
persistent native fault is present (zero or one). Reset clears those counters
and failures while retaining the configured compiled bank.
The runtime debugger renders these in the **IOP/SIF** tab.

Build standalone tests with:

```sh
cmake -S ps2xIOP -B out/build/iop-tests -DPS2X_IOP_BUILD_TESTS=ON
cmake --build out/build/iop-tests
ctest --test-dir out/build/iop-tests --output-on-failure
```

The suites cover IRX execution, RPC, imports, version resolution and generic
HLE compatibility. `ps2x_tests` also covers runtime SIF RPC/DMA integration.

## Native AOT laboratory

```text
already relocated RAM words → offline C++ generation → host compiler
                            → compiled bank → native dispatcher
```

The converter is `lab/generate_iop_bank.py`. It covers every aligned word in
the supplied RAM range, including interior entries, with fixed instruction
parameters. The runtime reads the live word only to verify its identity.
Parameterized kernels also extract admitted immediate/jump operands after the
full bound-word guard; opcode and register fields remain compiled constants.
The dispatcher owns its entry directory; callbacks must remain loaded while
the subsystem uses the bank. Its ABI is internal and not installed for third
party use yet.

`PS2X_IOP_BUILD_LAB=ON` builds an offline IRX loader frontend. Its output can be
passed to the converter with `--loaded-module`, so the base and complete bank
range are taken from loader metadata. A native startup probe accepts generated
bank C++ through `NEXO_IOP_BANK_CPP`; the diagnostic startup probe is a separate
target. Both record their scope and reject unsupported external host operations.
See [the laboratory guide](../lab/README.md) for the complete commands.

`--family-catalog` generates a shared operation pool and per-image directories for
multiple IRX files. Set `NEXO_IOP_BANK_MANIFEST` instead of `NEXO_IOP_BANK_CPP` to
link it. CMake verifies source/header hashes before building. Stable hash shards
and unchanged-file preservation support incremental compilation. The root
`PS2X_FAST_ITERATION` profile compiles the IOP library and catalog with
`-O1 -fno-lto`, with IPO disabled.

At the root, `PS2X_RUNTIME_NATIVE_IOP=ON` requires a compiled catalog and
`PS2X_IOP_ENABLE_INTERPRETER=OFF`. The laboratory runtime probe loads modules
through the real adapter without initializing a window. This option covers IOP
only; EE/VU, complete services and final game qualification are separate gates.

The original bank binds absolute physical addresses. The optional IRX family
frontend adds full source-image identity and binding across loader-selected
bases, with static operation/register fields and bound immediate/jump operands.
Module unload/reset retire bindings; the full kernel lifecycle on module
replacement remains unqualified. The initial semantics are specialized from
the existing CPU model: differential agreement
does not certify hardware semantics or timing. Native service contracts also
retain the current qualification limits. See
[the V0 contract](../schemas/nexo-iop-aot-v0.md) for commands and remaining gates.

The standalone native tests exercise synthetic IRX startup, load and branch
checkpoints, interior entries and aliases, code writes, empty/invalid banks,
unresolved imports, incomplete relocations, and startup budget exhaustion.
Diagnostic builds additionally compare the emitted operations to the identified
CPU model. The strict build excludes its generic instruction-execution symbol.

Native import dispatch also guards the stub, ordinal and table dependency words
against admitted identities before invoking a service. Empty banks cannot admit
known imports. Bound data metadata remains non-executable. The actual runtime
laboratory probe can load a module sequence and advance the IOP scheduler between
loads; a bounded original boot sequence observed threads and registered RPC
servers with native operations and complete RAM/model agreement. Service fidelity,
guest warning causes, canonical state and complete game execution remain open.
<!-- PS2NATIVE_SOURCE_END:ps2xIOP/README.md -->

---

<a id="anexo-15"></a>

# ANEXO 15 — ps2xRuntime/Readme.md

Origem: [ps2xRuntime/Readme.md](ps2xRuntime/Readme.md). Linhas originais: **77**. Bytes: **2869**. SHA-256: `82120aefb2db56b10af603502a834b82a5d9ebd8fa2237946744855b358ed2d4`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:ps2xRuntime/Readme.md -->
# Runtime Library
The runtime library provides the execution environment for recompiled code, including:

* Memory management (32MB main RAM, scratchpad, etc.)
* Register context (128-bit GPRs, VU0 registers, etc.)
* Function table for dynamic linking
* Basic PS2 system call stubs

# How to use:

Take your decompiled code and place the cpp files on ps2xRuntime/src/runner and header files on ps2xRuntime/include and compile/be happy.

Desktop runners accept the boot ELF as the first argument and an optional raw
disc image as the second argument. The image supplies sector reads such as
`sceCdRead`; extracted files alone do not provide data for arbitrary LBNs.

```sh
ps2EntryRunner /path/to/boot.elf /path/to/game.iso
```

The generated package launcher also accepts the image through
`PS2NATIVE_CD_IMAGE`:

```sh
PS2NATIVE_CD_IMAGE="/path/to/game.iso" ./run-ps2native.sh
```

Release configurations default to `PS2X_FAST_ITERATION=ON`: generated game
code and the runner use `-O1` without LTO so changed code can be rebuilt and
linked quickly. For final packaging, configure with
`-DPS2X_FAST_ITERATION=OFF -DPS2X_ENABLE_RELEASE_IPO=ON` to enable release LTO.

## Vita Build Notes

The Vita runtime uses `Quenom/raylib-5.5-vita` for vita build. I recommend build runtime only.

Expected environment:

* `VITASDK` points to your VitaSDK root.
* `Quenom/raylib-5.5-vita` has already been built and installed into `$VITASDK/arm-vita-eabi`.
* SDL2 with the PVR backend required by that raylib fork is also installed into the same VitaSDK prefix.
* `PS2X_DEFAULT_BOOT_ELF` is mandatory, you need to define where your game is like "ux0:data/RANJ00001/game/SLUS_201.84".

The CMake for `ps2xRuntime` consumes those preinstalled headers and libraries from VitaSDK. It does not fetch or install the Vita raylib fork for you.

## Adding Custom Function Implementations
You can add custom implementations for PS2 system calls or game functions by:

1. Creating function implementations that match the signature:
```cpp
void function_name(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime);
```

2. Registering them with the runtime:
```cpp
runtime.registerFunction(address, function_name);
```

## Advanced Features
Memory Translation
The runtime handles PS2's memory addressing, including:

* KSEG0/KSEG1 direct mapping
* TLB lookups for user memory
* Special memory areas (scratchpad, I/O registers)

## Vector Unit Support
PS2-specific 128-bit MMI instructions and VU0 macro mode instructions are supported via SSE/AVX intrinsics.

## Instruction Patching
You can patch specific instructions in the recompiled code to fix game issues or implement custom behavior.

## Limitations

* Graphics and sound output require external implementations
* Some PS2-specific hardware features may not be fully supported
* Performance may vary based on the complexity of the game
<!-- PS2NATIVE_SOURCE_END:ps2xRuntime/Readme.md -->

---

<a id="anexo-16"></a>

# ANEXO 16 — ps2xAnalyzer/Readme.md

Origem: [ps2xAnalyzer/Readme.md](ps2xAnalyzer/Readme.md). Linhas originais: **85**. Bytes: **4169**. SHA-256: `ced2830b4973818cf959210d4ac03971c000f8634177b39b88bec93b69ce877a`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:ps2xAnalyzer/Readme.md -->
# PS2 ELF Analyzer Tool

The PS2 ELF Analyzer Tool automates the creation of TOML configuration files for the PS2Recomp static recompiler. It identifies function boundaries, library stubs, and problematic instructions.

## Analysis Paths

The analyzer supports three distinct paths for discovering code within a PS2 binary:

### 1. DWARF Debug Information
If the ELF was compiled with debug symbols (`-g`), the analyzer uses `libdwarf` to extract perfect function names and exact start/end addresses. This is common in homebrew or early development builds.

### 2. Native Heuristic Scanner (Test only)
For commercial games where symbols are stripped, the analyzer uses a "JAL Scanner":
* It scans executable sections for `JAL` (Jump and Link) instructions.
* It infers function start points based on jump targets.
* It generates names like `sub_XXXXXXXX`.

Use this path only as a quick fallback when you do not yet have a Ghidra project. It is not the preferred workflow for retail games.

### 3. SCE SDK Symbol Database (For SDK Function Names)
For stripped retail games, the analyzer can identify SCE/PS2SDK library functions from a
`sce-symbol-scanner` compatible database. A snapshot of the database is embedded in the
analyzer. Pass the directory that contains `symbols.json` and `tree.json`, or point
`PS2RECOMP_SCE_SYMBOL_DB` at that directory, only when you want to override the embedded
snapshot.

This path is meant to recover names such as CD/DVD, pad, DMA, GS, kernel, and libc SDK
functions so they can be classified before the expensive analysis passes run.

The current database was built from PS2 games with debug information, primarily the
Japanese set, and depends on samples that retained relocations. Treat the result as a
high-confidence hint rather than a complete SDK catalog: it can miss SDK variants that
were not present in the sampled games, and ambiguous matches are intentionally ignored.

### 4. Ghidra Integration
1. Use the provided script: `ps2xRecomp/tools/ghidra/ExportPS2Functions.java`.
2. Run it in Ghidra to export a CSV map of all functions.
3. Let the script generate the TOML, and keep the CSV path in `ghidra_output = "path/to/map.csv"`.
4. Run the recompiler with that exported TOML.
5. The recompiler will prioritize Ghidra's boundaries over its own heuristics.

## Key Features

* Analyzes PS2 ELF binaries to extract symbols, functions, and structure
* Identifies common library functions that should be stubbed
* Reports risky instruction patterns for manual review without auto-skipping functions
* Detects potential instruction patterns that may need patching
* Generates a ready-to-use TOML configuration file for PS2Recomp

## Using the Analyzer
```bash
ps2_analyzer <input_elf> <output_toml> [sce_symbol_db_dir]
```

### Parameters:

* `input_elf`: Path to the PS2 ELF file.
* `output_toml`: Path where the generated TOML configuration will be saved.
* `sce_symbol_db_dir`: Optional override path to a directory containing `symbols.json` and `tree.json`.

## Example Workflow
1. Open `game.elf` in Ghidra.
2. Run `ps2xRecomp/tools/ghidra/ExportPS2Functions.java`.
3. Use the exported TOML and CSV.
4. Run the recompiler: `ps2recomp config.toml`

Fallback:
1. Run `ps2_analyzer game.elf config.toml`.
2. Use that TOML only for quick bring-up or symbol-rich builds.

## Generated Configuration
The tool creates a TOML file with the following sections:
* `[general]`: Paths to ELF and Ghidra maps.
* `stubs`: Runtime-known functions to be replaced by C++ stubs or syscall handlers.
* `untracked_stubs`: Detected library-like functions without runtime handlers. This is informational only and is ignored by the recompiler.
* `entry_points`: Guest functions without runtime handlers that may be referenced by address.
* `skip`: Legacy compatibility field. The analyzer no longer auto-populates it.
* `[patches]`: Individual instructions that need to be replaced (SYSCALLs, COP0, etc.).

## Limitations

* Heuristics may not catch all special cases in highly optimized code.
* Self-modifying code is flagged but requires manual review.

For more details on the recompilation process, see the [Main README](../README.md).
<!-- PS2NATIVE_SOURCE_END:ps2xAnalyzer/Readme.md -->

---

<a id="anexo-17"></a>

# ANEXO 17 — schemas/nexo-device-state-v1.md

Origem: [schemas/nexo-device-state-v1.md](schemas/nexo-device-state-v1.md). Linhas originais: **152**. Bytes: **8377**. SHA-256: `9c72b971ddc33a0092266eda044ca38bbeab38ccdd4adfac327c234b01db3ad5`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-device-state-v1.md -->
# NEXO VIF1, GIF queue and CPU GS checkpoints, version 1

These laboratory formats describe the **identified current runtime model**.
Matching a restored model against the same model is `tested_only` evidence.
It does not establish independent PS2 correctness, full-game native execution,
Android support, universal code closure, or whole-game frame rate.

The codecs operate while execution and all writers are paused. Their internal
locks protect storage ownership; they do not create a cross-device atomic
capture boundary. The enclosing case must pause CPU, DMA, VIF, VU, GIF, GS and
presentation writers and bind all component states, input bytes and producer
implementations with hashes. No host pointers or callbacks are serialized.

## Common envelope

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | Format magic below |
| 8 | 4 | Envelope version, `1` |
| 12 | 4 | Component variant below |
| 16 | 4 | Payload length, excluding the 24-byte envelope |
| 20 | 4 | CRC32 of envelope and payload, skipping bytes 20–23 |
| 24 | variable | Explicit component fields |

All integers are little-endian. Booleans occupy one byte and accept only 0/1.
Enums use their declared fixed-width underlying integer type. Floating point
values preserve their IEEE binary32/binary64 bits, including signed zero and
NaN payloads; no host struct padding is included. Byte blobs use a `u32` length
followed by exactly that many bytes. Arrays contain their elements in order
without another length. CRC32 uses the reflected polynomial `0xEDB88320`,
initial accumulator `0xFFFFFFFF`, and final bitwise complement. CRC detects
accidental corruption; an enclosing evidence manifest provides SHA-256 binding.

The reader verifies magic, version, variant, exact length, size budget and CRC
before decoding. It rejects truncated fields and trailing bytes. Decoding uses
owned temporary storage. A rejected restore preserves the target state; all
allocations and validation precede publication. Target callbacks, pointers and
backend ownership remain installed.

## VIF1 variant 1

Magic: `4e 45 58 4f 56 49 46 00` (`NEXOVIF` plus zero).
Maximum encoded size: 64 MiB including the envelope.

Payload order:

1. Twenty-three `u32` VIF1 register words: `stat, fbrst, err, mark, cycle, mode,
   num, mask, code, itops, base, ofst, tops, itop, top`, then `row[4], col[4]`.
2. Parser residue: `remainingBytes:u32, directHl:bool, payload:blob,
   pendingCommand:blob`.
3. Transport: `path3Masked:bool, pendingPath2ImageQwc:u32,
   pendingPath2DirectHl:bool`.
4. Masked PATH3 FIFO: packet count `u32`, then one blob per packet in queue order.

An incomplete DIRECT/DIRECTHL transfer is bounded by 65,536 qwords (1 MiB).
Consumed payload plus remaining bytes must fit that budget and total a whole
qword when a DIRECT transfer is active. Pending command residue is at most
4,099 bytes. A parser cannot have an active DIRECT transfer and pending command
residue simultaneously; completed DIRECT payload storage is empty. Pending
PATH2 IMAGE qwords are at most 32,767. Masked FIFO packets are at least 16 bytes.
Packet counts are checked against remaining encoded bytes before allocation.

This checkpoint excludes VIF0, RAM/scratchpad, VU memories/state, CPU status,
DMA registers/queued DMA work, completed DMA causes, MMIO storage, the GIF
arbiter and GS. Those are separate parts of an enclosing causal case. The
parser bridge accesses the existing sidecar keyed by the memory instance,
without changing the memory class layout. A memory reset still discards its
parser sidecar. Resetting an instance after restore discards the restored state.

## GIF arbitration queue variant 0

Magic: `4e 45 58 4f 47 41 52 00` (`NEXOGAR` plus zero).
Maximum encoded size: 64 MiB including the envelope.

Payload: queue count `u32`, then for every packet:
`pathId:u8, path2DirectHl:bool, path3Image:bool, data:blob`.

`pathId` is 1, 2 or 3. Packet data is at least 16 bytes. DIRECTHL metadata is
valid only for PATH2. The PATH3 IMAGE bit must agree with the first tag's format.
Queue order and all metadata survive restore; the receiving target's callback
is preserved. The codec does not alter or certify the runtime's arbitration
algorithm or its relationship to hardware timing. It does not serialize
callbacks, VIF's separately masked PATH3 FIFO, or delivered packet history.

## CPU GS variant 0

Magic: `4e 45 58 4f 47 53 00 00` (`NEXOGS` plus two zeros).
Maximum encoded size: 128 MiB including the envelope.

Source and target must have the identified `GSCpuBackend`, coherent frontend
and backend VRAM pointers, and exactly 4 MiB of initialized local memory.
The target must match whether privileged registers are bound. Other raster
backends and uninitialized instances are rejected.

Payload order:

1. Privileged-register presence `bool`, then twenty `u64` values in the declared
   `GSRegisters` order: `pmode, smode1, smode2, srfsh, synch1, synch2, syncv,
   dispfb1, display1, dispfb2, display2, extbuf, extdata, extwrite, bgcolor, csr,
   vsyncTick, imr, busdir, siglblid`. An unbound register set contains all zeros.
   Atomics are sampled/restored as values, rather than serialized as objects.
2. VRAM byte blob, exactly 4 MiB.
3. Frontend fields, in the `GS_FRONT_FIELDS` order in `lab/src/gs_snapshot.cpp`:
   both contexts; active, PRIM and PRMODE primitive registers; current color,
   Q/ST/UV/fog inputs; global draw registers; transfer registers; all six vertex
   slots and both signed 32-bit counters; display snapshot; preferred display
   source; latched presentation frame/metadata; native upload/packet counters.
4. Backend CLUT: 512 `u16` values and two remembered CBP values as `u32`.
5. Texture page state: page base `u32` and 8,192 cached bytes. Base is either
   `UINT32_MAX` or an aligned page entirely within VRAM.
6. Transfer command fields: BITBLTBUF, TRXPOS, TRXREG, direction `u32`.
7. Transfer progress: `x, y, totalPixels, copiedPixels, direction` as `u32`,
   then stored pending-byte count as explicit `u64` with host-size fit checking.
8. Local-to-host staging blob and read cursor as explicit `u64`.

Public GS aggregates use explicit fields in declaration order, recursively,
as listed by `GS_FIELDS` in the codec. Vertex Z is binary64; X/Y/Q/S/T are
binary32. Primitive registers contain a `u8` type plus eight booleans. Reserved
primitive type 7 is preserved because the current parser can produce it.
The runtime's vertex index accumulates across draws; it is not restricted to
the six-slot queue length. Both counters must be nonnegative.

### Hidden state that must survive

Loaded CLUT colors and remembered CBPs are not reconstructed from VRAM. A game
may reuse the palette source after loading it. Likewise the current backend's
texture page may intentionally hide subsequent VRAM writes until TEXFLUSH.
The page bytes are preserved even if they differ from VRAM; silently clearing
this cache would change the continuation. Partly assembled primitives retain
all vertices, input registers and counters. Local-to-host staged bytes retain
their subpixel byte cursor instead of being regenerated.

The codec preserves presentation buffers so it can compare subsequent host
observations of this model. Diagnostic history rings, log counters, mutexes,
VRAM handlers, host pointers, vtables and backend ownership are excluded.
No GPU state or full global-machine state is represented by this variant.
Frontend, backend and presentation writers must be quiescent before capture.

## Tests and remaining gates

The device suite exercises every byte split in UNPACK, MPG and DIRECTHL example
streams; masked PATH3 retention; queued GIF metadata and drain continuation;
in-flight host-to-local transfers; partial local-to-host reads; loaded CLUT and
half-built sprite continuation; stale texture visibility until TEXFLUSH;
privileged atomics, float payloads and latched presentation buffers. It also
checks checksummed semantic corruption and transactional target rejection.

These synthetic tests establish restoration of this model in these cases.
An original Monster House call now has an enclosing current-model replay,
documented in `nexo-observed-vif-case-v1.md`. Complete external-input closure,
independent reference and second-architecture replay remain unfinished gates.
Existing normalized VU captures cannot be relabeled as original VIF captures.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-device-state-v1.md -->

---

<a id="anexo-18"></a>

# ANEXO 18 — schemas/nexo-ee-aot-v0.md

Origem: [schemas/nexo-ee-aot-v0.md](schemas/nexo-ee-aot-v0.md). Linhas originais: **119**. Bytes: **6666**. SHA-256: `7ee40403e17ddb6d80d4afbde5822ddf0c8a30af72b8fe164a29b5f4a0388013`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-aot-v0.md -->
# Finite EE overlay catalog V0

This experimental catalog migrates **observed** diagnostic EE snapshots into
offline compiled callbacks. It is partial preparation for M3 in the root README.
It does not qualify code publication, closure, independent fidelity, a complete
game, Android, or a fully native package. The diagnostic VU backend remains.

## Artifact recovery

`lab/extract_ee_overlay.py` reads an identified ELF64 little-endian Linux x86-64
`ET_DYN` artifact. It parses ordinary symbol tables and RELA relocations without
loading the library or executing its getter. The recovered producer layout has
32-byte bindings and the identified `snapshot` and `bindings` objects. Native
callback symbols must belong to file-backed executable section ranges.

```sh
python lab/extract_ee_overlay.py observed.so new-case --entry 0x10000
```

The entry must come from the producer observation. Recovery emits
`snapshot.bin` and `bank.json`: physical base, observed root, image/artifact
SHA-256, byte length, and every recovered PC/source dependency. The file is
bounded to 128 MiB, the snapshot to 64 KiB, and bindings to 32,768. Output must
be fresh. A failed write can leave an incomplete output; it is not a case without
both files. `live_abi_independently_verified=false` is intentional: identifying
the artifact layout does not independently verify the running ABI.

## Offline catalog generation

```sh
python lab/generate_ee_bank_catalog.py new-case other-case \
  --generator build/ps2xRecomp/ps2_native_overlay --output new-catalog
```

The generator admits the image hash/dimensions before invoking the existing
overlay translator against a private copy of those bytes. Every regenerated
binding must agree with the recovered PC/source dependency. Cases are sorted by
their content key, so input order does not affect the catalog. Each bank gets its
own C++ namespace; versions at the same guest address coexist without symbol
collisions. The tool emits up to 512 banks, an index, and `catalog.json` with
source hashes, bank identities and generator/script hashes. No C++ compilation
occurs in the game runtime.

New catalogs use the explicit `normal-entry-v1` dependency policy. Legacy bank
sources remain identical; the index refines copied descriptors after checking
callback identity and local backedges. The sidecar, migration commands, invocation
adapter and limitations are in
[`nexo-ee-entry-dependencies-v1.md`](nexo-ee-entry-dependencies-v1.md).

A complete manifest is written last. Failed generation can leave sources in a
fresh output directory; those sources are not an admitted catalog. There is no
automatic overwrite of previous cases. Explicit `--extend` validates an existing
catalog and preserves all unchanged source timestamps, as described in
[`nexo-ee-miss-v1.md`](nexo-ee-miss-v1.md). The manifest records producer
identity, not a qualified semantics/build fingerprint or a cache correctness
proof. Runtime headers and compiler settings still require separate provenance.

## Runtime admission

```sh
cmake -S . -B build -DPS2X_BUILD_NEXO_LAB=ON -DPS2X_FAST_ITERATION=ON \
  -DPS2X_RUNTIME_AOT_EE_OVERLAYS=ON \
  -DNEXO_EE_BANK_MANIFEST=/absolute/new-catalog/catalog.json
```

CMake requires schema 1, `compiledEeProgram`, one generated index, one source
per bank, unique generated basenames, and exact source hashes. The catalog
compiles with `-O0 -fno-lto` on GNU/Clang; the runtime selection bit belongs only
to a separate backend object. The diagnostic EE driver implementation is omitted
from that backend when AOT is selected. IOP selection remains a separate option.

The dispatcher owns source image/entry metadata and uses sparse 4 KiB pages of
aligned PC slots. Each slot chains finite precompiled candidates. Lookup compares
the candidate's full declared instruction footprint against physical EE RAM and
returns an existing callback only on equality. It does not decode guest opcodes,
interpret instructions, call a compiler, or load a generated DSO.

Limits are 512 banks, 64 KiB per image, 32,768 bindings per bank, and 2,097,152
total bindings. Invalid dimensions, misidentified pointers, unaligned footprints,
null callbacks and duplicate PCs within a bank reject the directory. Lookup
distinguishes `Ready`, `MissingEntry`, `CodeChanged`, `MisalignedPc`, `OutsideRam`
and `NoRam`. Invocation admission additionally rejects `UnsupportedEntryContext`
for null or pending-delay contexts and rechecks bytes at the actual context PC.
This V0 admits physical addresses only; other aliases fail admission.

Actual runtime misses emit `EE:UNSEEN_CODE` and request a stop. This overrides
diagnostic continue/skip policies, including the earlier branch-report path.
`hasFunction` remains a query. Existing static/core and loaded-module tables have
their original precedence and are not newly qualified by this change.

## Explicit unresolved obligations

- Guards observe RAM at query and **again at invocation**. Instruction-cache visibility, DMA/store
  publication epochs, alias coherence and changes during a running block are
  unqualified. Matching RAM bytes alone is not a fetch-state proof.
- Entry context, temporal device contracts, hidden kernel state and independent
  R5900 fidelity are not qualified. This uses the existing EE translator.
- Observations cannot prove that the finite catalog covers all future code.
  Unseen code stops; automatic family synthesis and full campaign exploration
  remain open.
- Opt-in miss capture now preserves copied RAM, optional canonical fields of the
  identified EE context model and candidate footprint differences. Offline byte
  preparation is implemented. A complete canonical reproducible EE/system miss
  checkpoint and full-machine replay remain unimplemented.
- Commercial snapshots and generated callbacks remain ignored local artifacts;
  the repository contains the tools and synthetic tests.
- Passing the synthetic tests or displaying game logos/menus does not establish
  gameplay, save correctness, rendering/audio fidelity, 60 FPS, Android execution
  or universal ISO conversion.

## Checks

`nexo_ee_aot_tests` exercises version replacement, owned source identity, interior
entries, malformed footprints and strict runtime call misses. The extractor tests
compile a synthetic DSO and corrupt its ranges. `nexo_ee_bank_catalog_tests`
executes two generated native arithmetic/JR/delay-slot variants, checks independent
interior entry, rejects stale code, and checks deterministic generation, recovered
binding agreement, source hashes and unsafe manifests. Headless isolation tests
cover removing an inherited diagnostic driver in the owned child process.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-aot-v0.md -->

---

<a id="anexo-19"></a>

# ANEXO 19 — schemas/nexo-ee-data-family-candidates-v1.md

Origem: [schemas/nexo-ee-data-family-candidates-v1.md](schemas/nexo-ee-data-family-candidates-v1.md). Linhas originais: **120**. Bytes: **6962**. SHA-256: `64ad64f5686438fda12d021a39a8a57f612342a9ebff07d8524f6743ab8b396c`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-data-family-candidates-v1.md -->
# EE data-family candidates, laboratory v1

`lab/discover_ee_data_families.py` groups bounded observed dependency regions
during conversion. It writes an offline proposal, not a runtime program, compiled
bank, certificate or execution authorization. Every report retains
`strict_approval`, `closure_proved`, `producer_invariant_proved` and
`native_execution_validated` as false.

## Input and command

Use hash-checked copies already owned by a catalog:

```sh
python lab/discover_ee_data_families.py \
  --catalog /path/to/catalog \
  --output /path/to/fresh-candidates.json
```

Alternatively, repeat `--case /path/to/prepared-case`. Each case contains
`snapshot.bin` and `bank.json` as specified by the EE extraction/miss preparation
tools. Input schema, exact snapshot hash/size, aligned physical RAM bounds,
strictly sorted unique bindings and requested-entry coverage are validated.
Bindings are byte dependencies reported by those tools; this detector does not
independently prove they are complete instruction/fetch footprints or reached
code. A source region is not necessarily a whole function or a basic block.

Paths including parents must not be symlinks. Output must be fresh; repeated
submissions do not replace an existing report. The catalog and game executable
are never modified. Catalog mode validates the owned input ledger before analysis.

## Restricted discovery

The grouping key is the complete region's normalized bytes, not just a digest.
The original classifier had two operand types. Current reports declare
`data_operand_profile: 2`, which additionally supports the ordinary fields below.
Register selection and control instructions are never normalized.

| Kind | Fixed fields | Candidate field | Reported value |
|---|---|---|---|
| `lui-u16` | LUI opcode, zero reserved source register, destination register | Low 16 bits | Unsigned integer 0..65535 |
| `sw-s16` | SW opcode, source/base and value registers | Low 16 bits | Signed integer -32768..32767 |
| `addiu-s16`, `slti-s16`, `sltiu-s16` | Exact opcode and registers | Low 16 bits | Signed integer |
| `andi-u16`, `ori-u16`, `xori-u16` | Exact opcode and registers | Low 16 bits | Unsigned integer |
| `lb/lh/lw/lbu/lhu/lwu/ld/lq-s16` | Exact load opcode and registers | Low 16 bits | Signed displacement |
| `sb/sh/sd/sq-s16` | Exact store opcode and registers | Low 16 bits | Signed displacement |

All other bits, including branches and jump targets, remain exact. Changes in
opcode, register selection, region length or fixed control encoding split groups.
By default, only fields that actually differ within a group become proposed parameters.
Eligible immediates that remain constant retain a full `0xffffffff` guard mask.
An identical region at several addresses alone does not become a data-family
candidate. Repeated copies at the same address with the same bytes are deduplicated
while retaining all source identities.

The explicit batch options `--minimum-variants 1 --operand-policy typed` also
propose singleton structures and parameterize every eligible data immediate.
Opcode/register/control fields remain fixed. These broader domains are proposals
without producer or independent fidelity approval. `--root-only` retains only
bindings at their dependency start; `--terminal-only` retains regions with a
syntactically complete terminal transfer for subsequent frontend validation.
Skipped bindings and nonterminal regions are counted. The latter restriction
can omit executable linear continuations and is not a closure algorithm.

This is a syntactic partition, not an assertion that an immediate is causally
unrelated to control flow. A parameter may feed an indirect destination or alter
observable memory behavior. Those obligations remain open. Direct jump semantics,
relative-PC expressions, exception/call locations, guest timing, entry/delay
context, writes during execution and aliases also require separate validation.

## Report

Top-level `schema_version` is 1, `status` is `CANDIDATES_LABORATORY`, and `counts`
describe submitted cases, bindings, unique per-case regions, scanned words and
oversized skipped regions. `analyzer_sha256` identifies the exact tool source
used for the report, without certifying its semantics. `families` is sorted by
`shape_sha256`.
`discovery_policy` records the minimum distinct byte variants, operand policy
and root/terminal filters. Retained older reports omit that field.

Each candidate contains:

- `word_count`: length of the proposed structure, in little-endian 32-bit words.
- `guard_words`: exact words with candidate parameter bits zeroed.
- `guard_masks`: `0xffff0000` for a proposed supported immediate; otherwise
  `0xffffffff`. No arbitrary masks are accepted from input.
- `parameters`: ordered `word_index`, `kind`, and `bits: 16` records.
- `normal_entry_offsets`: the union of actual declared binding offsets for the
  observed regions; it is not inferred from every instruction. Each observation
  also retains its own offsets. These are proposals, not entry-ownership proofs.
- `observations`: physical `pc`, exact `word_sha256`, ordered extracted
  `parameters`, and sorted `origins`. Each origin gives exact image and metadata
  SHA-256 identities and the image base.
- `shape_sha256`: hash of the versioned classifier tag followed by little-endian
  guard words and guard masks. It identifies a candidate, never a semantic proof.
  Profile 2 uses `ee-data-shape-typed-v2`; retained profile-1 artifacts keep their
  older classifier tag and exact analyzer identity.

For an observed region, replacing each parameter word's low 16 bits with
`parameter & 0xffff` reconstructs its exact bytes. This round trip is a data
extraction check. It does not validate a native kernel or unobserved parameters.
Reports contain observed values, not an approved domain or generalized bound.
The analyzer has no instruction execution, guest decoder at runtime, compiler
invocation, external model request or paid service use.

## Bounds and next stage

Limits: 512 cases; 64 KiB per image; 8 MiB per metadata document; 64 MiB aggregate
metadata; 32,768 bindings per case and 2,097,152 aggregate; 131,072 dependency
regions; 4,194,304 scanned words. Regions longer than 128 words are explicitly
skipped, counted and left unqualified. Exceeding other aggregate limits aborts
publication. These are laboratory search budgets, not coverage limits that can
be silently waived for strict approval.
At most 32,768 candidate families and 64 MiB of serialized report are permitted.
Serialization is charged before creating the publication path.

Next steps are producer/write slicing, restricted native synthesis using the
existing EE semantic emitters, context/relative-PC guards, differential checks
and independent fidelity, then establishing admissible parameter domains and
reachable-caller coverage. Candidate discovery alone does not prevent a game
stop and does not satisfy M9 or any complete-game gate.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-data-family-candidates-v1.md -->

---

<a id="anexo-20"></a>

# ANEXO 20 — schemas/nexo-ee-entry-dependencies-v1.md

Origem: [schemas/nexo-ee-entry-dependencies-v1.md](schemas/nexo-ee-entry-dependencies-v1.md). Linhas originais: **115**. Bytes: **6831**. SHA-256: `8fcfb8417aa7aa3574da0bd5f02897eeeb6e74a29af4e565ad50d6174e2864fa`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-entry-dependencies-v1.md -->
# EE normal-entry dependencies V1

This is a laboratory refinement of the finite EE AOT catalog, not a complete
R5900 entry, instruction-fetch, publication or semantic-cache contract. It fixes
guards that rejected ordinary interior entries because skipped prefix words had
changed. The callback bodies, immutable snapshots and original binding ABI stay
identical; only the active catalog's dependency descriptors change.

## Entry admission

The current translator emits an independent resume label for every instruction.
For an ordinary entry at `pc`, the linear prefix before that label is skipped.
Its declared dependency begins at `pc`, except when the same callback can reach
an earlier local static branch/jump target. In that case it begins at the earliest
such target or `pc`, whichever is lower. It ends at the original block end.
This remains conservative: bytes that cannot execute may still be included.

A separately entered terminal delay-slot label executes that instruction and
returns to the next PC; it does not repeat its preceding branch. Its dependency
is its own four-byte instruction. This is different from resuming a pending
architectural branch. The runtime invocation adapter rejects a null context or
`context.in_delay_slot=true`, preserving guest RAM and context. It rechecks bytes
at invocation, including changes since the earlier resolver query. It stops on
rejection; there is no guest interpreter, decoder or runtime compiler fallback.

`Dispatcher::lookup` and `diagnose` are RAM-only queries. `Ready` from them does
not promise that an execution context can be admitted. Runtime resolution returns
an adapter, whose invocation selects by the actual context PC and calls
`admitNormalEntry`. Complete hardware entry-context semantics remain unqualified.
Existing static/core and loaded-module tables retain their prior behavior.

The C++ API accepts `OverlayDependencyContract::NormalEntry` (default) or
`LegacyWholeBlock`. The CLI defaults to normal dependencies; its optional sixth
argument `--legacy-footprints` reproduces the original whole-block descriptors.
The separate diagnostic live DSO driver explicitly requests legacy footprints,
because that consumer does not use the new invocation adapter. It remains a
development baseline and is omitted from the selected AOT EE backend.

## Offline producer and immutable banks

New catalog generation emits both descriptor variants from the same snapshot.
It checks identical callback/snapshot/getter text, identical callback names and
entry sets, and dependency ranges contained in the legacy footprint. Bank C++
sources retain the legacy descriptors and their byte identity. The index owns
copies of bindings and applies the normal dependencies to those copies once.
Changes to this index therefore need not rebuild existing bank objects.

Recovered cases without `dependency_contract` mean `whole-block-v0`; their
recorded bindings are compared against the legacy emission. Prepared cases now
record `normal-entry-v1` and are compared against the normal emission. Ordinary
extension preserves the catalog's policy and requires the same generator hash.

Explicit migration:

```sh
python lab/generate_ee_bank_catalog.py all-previous-case-directories \
  --generator build/ps2xRecomp/ps2_native_overlay \
  --output existing-catalog --extend --migrate-entry-guards
```

The old manifest and every old source hash are checked first. Migration allows
a different producer binary only when every previously compiled bank source
regenerates byte-for-byte identically. A captured case pinned to the old producer
is admitted only for a bank already listed in that verified old catalog. Any
old bank removal or source rewrite rejects the update before publication.
The new manifest records the previous producer and catalog hashes. This proves
the stated source identity, not compiler/header identity or hardware fidelity.

## Dependency sidecar

`ee_entry_dependencies.json` is bounded to 64 MiB, with schema 1 and
`dependency_contract=normal-entry-v1`. Each bank names its generated C++ source
and contains compressed `runs`. Each run is `[first,last,begin,end]`:

- `first` is an included aligned entry PC; `last` is the excluded aligned bound.
- PCs in the run are consecutive four-byte entries; missing PCs start a new run.
- `begin=4294967295` means each entry's own PC; otherwise it is a fixed floor.
- `end` is the excluded dependency end. The complete range is compared to RAM.

Compression must reconstruct the exact emitted entry descriptors. The manifest
records its basename, SHA-256, total run count, index/source hashes and producer
identities. CMake rejects unknown policies, ambiguous scalar types, wrong names,
symlinks, excessive sizes/counts and changed hashes. CMake does not independently
prove a run's semantic meaning or its correspondence to the C++ index; those
are producer/test obligations, not an authentication or cache-correctness claim.

Publication requires an exclusively owned offline directory without concurrent
producers/builds. The sidecar is replaced before the index, and the manifest last.
Interruption can leave an incomplete update that fails subsequent identity
validation. This is not the root README's qualified publication protocol.

## Checks and remaining obligations

Synthetic native execution covers skipped-prefix changes, local backedges,
reachable-prefix rejection, standalone slots, mutation after resolver query,
pending-delay/null context rejection and guest RAM/context preservation. Migration
tests check old producer identity, immutable source timestamps, callback rewrite
rejection and lossless descriptor compression. The diagnostic DSO driver is
checked separately for its retained legacy descriptors.

The active 33-bank Monster House migration refined 524,557 entries into 58,945
runs. It took 9.135 seconds; rebuilding index/runtime/tests took 9.547 seconds,
preserving all 33 bank object hashes and timestamps. Relinking the existing game
objects took 10.969 seconds with `-fno-lto`, with zero original game compilations.
Both previously captured prefix-only failures passed saved-RAM guard replay.
These are measured local increments, not full ISO conversion times or fidelity
evidence. The isolated game reached its title, main menu and new-game file menu;
campaign gameplay and all final qualification gates remain open.
Attempting to start the selected game stopped at a new region, PC `0x1724d70`,
with `MissingEntry` and no candidates; the new miss was captured automatically.

Instruction-cache visibility, writes during callbacks, writer quiescence,
complete canonical machine replay, independent EE fidelity, universal coverage,
native VU in the actual game, Android, campaign/save/audio/rendering correctness
and sustained 60 FPS remain separate, unresolved obligations.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-entry-dependencies-v1.md -->

---

<a id="anexo-21"></a>

# ANEXO 21 — schemas/nexo-ee-family-catalog-v0.md

Origem: [schemas/nexo-ee-family-catalog-v0.md](schemas/nexo-ee-family-catalog-v0.md). Linhas originais: **149**. Bytes: **8955**. SHA-256: `6848ad74b3aac8982b0a96b36576a2adbc969a07b5be59993c7d5b654dc32d9c`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-family-catalog-v0.md -->
# Finite precompiled EE family admission, laboratory v0

This opt-in runtime experiment implements the catalog and admission step after
the [restricted synthesis frontend](nexo-ee-native-data-family-v0.md). A match
authorizes a laboratory callback under the stated RAM/context model. It does
not authorize a strict native package or claim a producer invariant, code
closure, instruction-cache equivalence, independent PS2 fidelity or campaign
completion. The delivered product requirements in the root README still apply.

## Offline publication

```sh
python lab/generate_ee_family_catalog.py \
  --candidates candidates.json --generator build/ps2xRecomp/ps2_native_data_family \
  --output fresh-catalog [--shape SHAPE_SHA256] [--case prepared-ee-case]
```

The publisher accepts 1..32768 bounded candidates (a report up to 64 MiB) and up to 16 prepared
root cases. The candidate report must have integer schema 1, laboratory status
and Boolean false approval. Words/masks must be numeric uint32 values, with
1..128 words per structure. Normal entries are aligned byte offsets in the
complete guard. Current proposals declare their observed entry offsets; retained
older reports keep the all-resume-label convention. Prepared
cases expose only their captured requested entry. The synthesis frontend still
rejects unsupported control, operations, masks and reserved fields.

Identical normalized word/mask structures merge their entry offsets. The
publisher derives its own filename digest from these numeric inputs; proposed
shape identifiers cannot supply source expressions or paths. Each generated
body has a separate namespace, descriptor and immutable words/masks/entry
arrays. A separate index exports `compiledEeFamilyProgram()`.

Manifest schema 2 groups bodies into source units, with at most 32 families and
1 MiB per body unit; the separate descriptor index has an 8 MiB bound. The
manifest has a 16 MiB publication bound. Stable hash buckets constrain insertion changes to their bucket.
`family_count`, `source_count` and the numerical `families` ledger describe the
result. CMake retains schema-1 support for older one-family-per-file catalogs.
Verified copies use stable content-addressed paths under the build's
`ee-family-source-cache`, so fresh job directories do not invalidate unchanged
objects. Header/compiler dependencies still apply; corrupted cache bytes fail.
Sources are checked again before publication and the cached copy is hashed
after copying, so a changed source cannot silently establish a new cache identity.
CMake extracts source-name/hash indexes once, avoiding a full family-ledger JSON
parse for every source query. `--workers 1..16` bounds concurrent converter
processes; ordered results preserve source bytes across worker counts. Buckets
split on both their family-count and byte budgets.

`--entry-policy root-only` limits proposals to offset-zero bindings, and
`--terminal-only` declines regions without a complete terminal transfer. These
are explicit laboratory coverage limits, not closure proofs. All normal labels
remain supported by the generated body.

`--root-data-parameters` proposes eligible data fields in a prepared root even
when seen only once. It keeps opcode/register/branch bits fixed and records this
policy in provenance. A captured data address is then a live typed operand.
The broader parameter domains still need producer, alias/fetch and independent
fidelity evidence before release approval.

The manifest records all generated source hashes, the converter before/after
identity, publisher identity, input report hash, selected shapes and exact
prepared-case metadata/image hashes and bounded dependency locations. Declined
structures are recorded. The output directory must be fresh. `catalog.json` is
written after the source files; interrupted publication without that manifest
is not a complete catalog. Publication assumes one owner of these paths.

`strict_approval` and `closure_proved` are Boolean false. These fields must not
be replaced with a success claim because a source file was emitted. Source
hashes bind build inputs; they are not proofs of guest behavior or a sandbox
for arbitrary native source supplied outside this trusted offline generator.

## Runtime descriptors and lookup

```cpp
using Function = void (*)(uint8_t*, R5900Context*, PS2Runtime*, uint32_t base);
struct Family {
    Function function;
    std::span<const uint32_t> words, masks, normalOffsets;
};
struct Program { std::span<const Family> families; };
```

The dispatcher validates the budgets, callback, sizes, masks and unique entry
offsets, then owns copies of the identity arrays and its entry index. Duplicate
complete structures are rejected rather than allowing competing callbacks.
Only full-word masks or low-16 data masks on the 20 supported operand classes are accepted. The
frontend independently checks support for the complete native body.

Lookup reads the word at an aligned physical PC and uses exact/upper-16 indexes
to find potential structure/entry-offset pairs. It computes `base=pc-offset`,
checks the complete bounded region, then compares every guarded word. The
callback and base are returned only for one matching pair. Two matching pairs,
including overlapping regions in the same family, produce `Ambiguous` and no
callback. There is no priority heuristic. No opcode is executed by this lookup.

`admitNormalEntry` also rejects null or pending architectural delay contexts.
The backend resolves existing concrete AOT entries first, then families. Its
returned adapter repeats admission using the **actual invocation PC, context
and current RAM**, rather than retaining a stale query result. The generated
wrapper again checks its complete guard and extracts current data parameters
before running its fixed compiled body. Rejection never requests a runtime
guest compiler or an EE interpretation fallback in this profile.

This is a serial RAM admission model. Whole-region checks do not establish
instruction-cache fetch identity, writer quiescence, DMA/concurrent safety,
self-modification inside an executing body, alias equivalence or kernel/device
timing. Masked immediates can influence memory and future code; they are not
automatically proven data-only domains. Unknown or ambiguous code still stops.

## Build boundary and evidence

`PS2X_RUNTIME_EE_DATA_FAMILIES` defaults OFF. Enabling it requires both the AOT
EE backend and the NEXO laboratory, with `NEXO_EE_FAMILY_MANIFEST` pointing to a
published catalog. CMake checks field types, counts, laboratory flags, source
names, hashes, sizes and ordinary paths. The matcher/backend and generated
family objects are separate build targets. Their debug profile uses no LTO;
changing this catalog does not regenerate the original game's sources.

Tests cover copied identities, relocations, live data, interior entries, stale
queries, changed invocation PCs, pending architectural slots, ambiguity,
descriptor errors, publication provenance and build-time rejection. Callback
mocks in admission tests record selection only. The existing generated-body
comparisons are conservative model regressions with shared semantic emitters.

The initial Monster House experiment published two structures: an observed
eight-word suffix with two typed parameters and an eleven-word fixed prefix
ending at its indirect call. Publication took 0.093 seconds; the incremental
native build took 13.861 seconds and the game link 16.428 seconds, retaining all
original game objects. The headless New Game route passed the previous
`0x184f448` miss and traced its `0x184f474` continuation, then stopped at an
uncovered callback `0x184f498`. No new address-specific TOML or bank was added.

This proves limited execution progress under the current laboratory runtime.
Inputs were supplied by the agent, not an autonomous campaign validator. VU
remains diagnostic, and game fidelity, full gameplay, Android and sustained
60 FPS are unqualified. The next discovery work must cover whole loaded code
structures and unsupported control flow in batches, with model regressions,
producer-domain evidence and independent validation before release approval.

The later batch experiment admitted 1,622 structures in 69 stable source units.
Typed root operands covered the relocated callback and its continuation; the
owned New Game route then stopped at another module entry, `0x1a51b70`.
Root-only/terminal-only policy, parameter domains, module coverage and whole-game
qualification remain open. See the dated batch evidence in `lab/README.md`.

`lab/prepare_ee_family_batch.py` owns and deduplicates previous cases, prepares
fresh captures, proposes typed singleton structures and publishes this catalog
in one offline command. `--previous-batch` verifies the previous receipt,
manifest and owned case hashes before reusing them. Failed jobs retain a false
approval diagnostic receipt. The tool does not launch games or certify closure.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-family-catalog-v0.md -->

---

<a id="anexo-22"></a>

# ANEXO 22 — schemas/nexo-ee-miss-batch-v1.md

Origem: [schemas/nexo-ee-miss-batch-v1.md](schemas/nexo-ee-miss-batch-v1.md). Linhas originais: **57**. Bytes: **3507**. SHA-256: `cc6a0ad21a278af94197d5c09fb3f6431f79fa49f51eedb766367ff879c38130`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-miss-batch-v1.md -->
# Offline EE miss batch V1

This laboratory command consumes bounded captured EE misses and extends an
existing finite native catalog. It derives roots and bytes from each capture;
there is no address list or title-specific repair rule. It is preparation for
the autonomy work in README sections 22 and 31, not strict approval or a proof
of universal closure, semantics, publication or complete machine replay.

`lab/admit_ee_misses.py --catalog DIR --capture RECORD ... --generator TOOL
--output FRESH_JOB` prepares at most 16 complete records before modifying the
catalog. Any invalid preparation rejects the whole batch. Duplicate bank
identities retain the existing case. The existing catalog publisher verifies
source identities, preserves all prior bank C++ sources and their timestamps,
and publishes its manifest last. The game never runs this command or a compiler.

Older catalogs need one bootstrap with `--case DIR` for every previous case.
New catalog manifests own `case_inputs`: per-bank relative directories under
`ee_cases/`, with snapshot and metadata SHA-256. Directory names contain bank
and metadata hashes. Reads reject symlinks, wrong paths, dimensions, hashes and
incomplete ledgers. New directories are published before the manifest; existing
input copies are immutable. They are laboratory artifacts and remain ignored.

`case_producers` retains a prepared case's original producer identity across
later extensions. It applies only to an already verified bank source; a foreign
new case cannot reuse that exception. Every retained callback source must still
regenerate exactly. Producer hashes identify claimed provenance, not authenticity
or semantic correctness. Build/compiler/header identities need separate evidence.

The fresh job report records input/output identities, prepared/duplicate/new bank
counts and terminal status. Status is `EXTENDED_LABORATORY`,
`NO_NEW_BANKS_LABORATORY` or `FAILED`; `strict_approval`, `closure_proved` and
`complete_machine_checkpoint` remain false. An interruption leaves incomplete
artifacts that must pass identity checks before reuse; there are no concurrent
producer/build guarantees. Compilation and game exploration are separate steps.
`prepared_new_banks` counts candidates; `new_banks` is set only after successful
extension and `manifest_published` records that terminal manifest step. A failed
preparation therefore reports zero published banks. Bootstrap cases must match
exactly the existing catalog, and cannot add unobserved cases through that option.

Example after the one-time bootstrap:

```sh
python lab/admit_ee_misses.py --catalog /path/to/catalog \
  --capture /path/to/ee-miss-000001 --capture /path/to/ee-miss-000002 \
  --generator build/ps2xRecomp/ps2_native_overlay --output /path/to/fresh-job
cmake --build build --target ps2_runtime ps2_ee_compiled_catalog --parallel 4
```

The build command assumes the existing selected native catalog configuration.
CMake validates the compiled source/dependency-plan hashes; owned input validation
belongs to the offline publisher. Neither mechanism authenticates the capture or
proves semantic cache correctness. A successful batch does not approve a package.

Tests must cover invalid all-or-nothing preparation, duplicate inputs, automatic
reuse of owned cases, producer migration lineage, unchanged bank timestamps,
corrupt/escaping/symlinked owned inputs, output conflicts and reported failure.
Actual game execution and every final qualification gate remain separate.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-miss-batch-v1.md -->

---

<a id="anexo-23"></a>

# ANEXO 23 — schemas/nexo-ee-miss-v1.md

Origem: [schemas/nexo-ee-miss-v1.md](schemas/nexo-ee-miss-v1.md). Linhas originais: **153**. Bytes: **8983**. SHA-256: `3d3d8d63da039d7c22ac8c85fca319e81f47cd605591a09c8d1581fd6d1597cd`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-miss-v1.md -->
# Observed EE miss and model context, version 1

This laboratory record diagnoses a finite AOT guard rejection. It preserves
the identified EE context model and a RAM copy for **guard replay**. It does
not establish writer quiescence, architectural fetch state, a full machine
checkpoint, independent R5900 fidelity, code closure or a qualified game.

## Capture contract

With the opt-in laboratory build, set `PS2X_EE_MISS_CAPTURE_DIR` on an owned
test process. A missing AOT invocation creates an exclusive `ee-miss-NNNNNN`
directory, with owner-only permissions and a maximum of 16 attempts per process.
Existing records are not overwritten. Failures are reported and contained;
capture does not change guest RAM or registers. Capture is absent unless the
environment variable is explicitly set. An AOT miss retains its stop policy.

The record contains:

- `ee-ram.bin`: 33,554,432 physical RAM bytes, if RAM is available.
- `snapshot.bin`: an aligned window of at most 65,536 bytes around the target,
  copied from that saved RAM. A target within the final 512 bytes of a 64 KiB
  region uses a 32 KiB overlap. The window is cropped at the end of physical RAM.
- `ee-context.bin`: the context model below, when the invocation supplies one.
  The contextless lookup API explicitly records absence.
- `expected-N.bin`: the immutable declared footprint for each finite directory
  candidate at the target, in diagnostic iteration order.
- `request.json`: schema 1, written last after all blob writes complete.

The request identifies processor `EE`, admission `missing`, physical target PC,
source PC, numeric `GuestBranchKind`, operation, module ownership/key, directory
availability, diagnostic lookup status, versions checked, captured components,
window dimensions and each candidate's bank index, dependency start/size,
expected filename, mismatch byte count and first mismatch offset. Zero mismatches
use a null offset. Text fields are capped at 4,096 bytes with explicit truncation.
Directory candidates are bounded by 512 banks. Bank indices are local to the
identified catalog, not permanent global identifiers.

`complete=true` means the listed record was written. Both
`quiescence_qualified=false` and `complete_machine_checkpoint=false` remain
explicit. The diagnostic comparison uses the saved RAM copy, not a second live
read; the original admission and this later copy can differ if writers run.
EE context copying is not synchronized with all other processors and devices.
No claim of an atomic world checkpoint or resumable whole-game state is made.

## Context model envelope

| Absolute offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 8 | Magic `NEXOEE` followed by two zero bytes |
| 8 | 4 | Format version, u32, exactly 1 |
| 12 | 4 | Identified model profile, u32, exactly 1 |
| 16 | 4 | Payload length, u32, exactly 1,619 |
| 20 | 4 | IEEE CRC32 of bytes 0–19 concatenated with bytes 24–end |
| 24 | 1,619 | Ordered model fields below |

The envelope is exactly **1,643 bytes**. All integers use little-endian fixed
widths. Float fields preserve their binary32 bits without numeric conversion.
SIMD values contain four u32 bit lanes in lane 0 through lane 3 order. Arrays
use increasing indices. The delay boolean is one byte, exactly 0 or 1. There
are no ABI padding bytes or host pointers. Unknown headers, checksums, length,
booleans, truncation and trailing bytes are rejected by the typed codec.

The payload order is:

1. `r[32]`, four u32 lanes each.
2. `pc` u32; `insn_count`, `hi`, `lo`, `hi1`, `lo1` u64; `sa` u32.
3. `vu0_vf[32]`, four bit lanes each; `vi[16]` u16.
4. `vu0_q`, `vu0_p`, `vu0_i` binary32; `vu0_r`, `vu0_acc`, four bit lanes each.
5. `vu0_status` u16; `vu0_mac_flags`, `vu0_clip_flags`, `vu0_clip_flags2` u32.
6. `vu0_cmsar0`, `vu0_cmsar1`, `vu0_cmsar2`, `vu0_cmsar3` u32.
7. `vu0_vpu_stat`, `vu0_vpu_stat2`, `vu0_vpu_stat3`, `vu0_vpu_stat4` u32.
8. `vu0_tpc`, `vu0_tpc2`, `vu0_fbrst`, `vu0_fbrst2`, `vu0_fbrst3`,
   `vu0_fbrst4`, `vu0_itop`, `vu0_top`, `vu0_info`, `vu0_xitop`, `vu0_pc` u32.
9. `vu0_cf[4]` binary32.
10. `cop0_index`, `cop0_random`, `cop0_entrylo0`, `cop0_entrylo1`, `cop0_context`,
    `cop0_pagemask`, `cop0_wired`, `cop0_badvaddr`, `cop0_count`, `cop0_entryhi`,
    `cop0_compare`, `cop0_status`, `cop0_cause`, `cop0_epc`, `cop0_prid`,
    `cop0_config`, `cop0_badpaddr`, `cop0_debug`, `cop0_perf`, `cop0_taglo`,
    `cop0_taghi`, `cop0_errorepc` u32.
11. `llbit`, `lladdr` u32; `in_delay_slot` bool8; `branch_pc` u32.
12. `cop2_ccr[32]` u32; `f[32]`, `f_acc` binary32; `fcr31` u32.

The absolute PC offset is 536; the delay boolean is at 1,374. This model includes
the declared VU0 macro fields, not complete VU pipeline state. TLB/cache state,
kernel continuation, scheduler, IOP, DMA, devices and event queues are outside
this context envelope. Portability of these bytes is tested on the current
x86-64 host only; a second architecture remains unqualified.

## Offline preparation and extension

```sh
python lab/prepare_ee_miss.py /path/to/ee-miss-000001 \
  --generator build/ps2xRecomp/ps2_native_overlay --output fresh-case
python lab/generate_ee_bank_catalog.py old-case-1 old-case-2 fresh-case \
  --generator build/ps2xRecomp/ps2_native_overlay --output existing-catalog --extend
```

Preparation admits only complete physical RAM byte cases with `MissingEntry` or
`CodeChanged`, without loaded-module ownership. It verifies window equality
against saved RAM. A supplied model context must pass the envelope checks,
have PC equal to the target and be outside a pending delay slot. Unsupported
entry contracts are rejected, not silently reset. Missing context is allowed
for byte generation and stays explicitly unqualified.

The generator executes offline against a private copy; preparation checks
aligned dependency ranges, unique entries, the requested root and a stable
producer executable hash. Output is fresh `snapshot.bin` and `bank.json`, with
RAM, context, request, image, generator and preparer hashes. These identify the
observed producers and bytes; they do not authenticate capture origin or prove
semantics. Catalog regeneration checks a recorded generator hash when supplied.
New prepared cases explicitly record `dependency_contract=normal-entry-v1`.
Old untagged cases retain `whole-block-v0` metadata; catalog generation verifies
them against the original descriptors before applying normal refinements.

Extension requires all previous cases plus the new case, validates old source
hashes and generator identity, and refuses removal or rewriting of prior banks.
Unchanged source files retain their bytes and timestamps. Only new bank sources,
the changed index and the manifest are replaced; the manifest is published last.
Use an exclusively owned offline directory without concurrent producers/builds.
Interruption can leave an incomplete update: reuse requires fresh hash validation,
and conflicting unrecorded artifacts require a fresh catalog. This is an
incremental build aid, not a qualified semantic cache or publication protocol.

An explicit `--extend --migrate-entry-guards` can refine the index while retaining
every prior bank source exactly, including when changing the producer binary.
It verifies all old identities and refuses any old callback source rewrite.
The bounded hashed dependency sidecar and exact entry restrictions are described
in [`nexo-ee-entry-dependencies-v1.md`](nexo-ee-entry-dependencies-v1.md).

`admit_ee_misses.py` now prepares bounded batches and reuses verified input copies
owned by the catalog. Its retained producer ledger allows later ordinary
extensions after a generator migration, only for already verified bank sources.
The command, duplicate handling and failure-report scope are in
[`nexo-ee-miss-batch-v1.md`](nexo-ee-miss-batch-v1.md).

The game runtime never invokes this preparation, generator or compiler. Finite
bank admission still compares each callback's complete **declared** footprint.
Requested entry generation can yield a different block boundary; it does not
erase mismatch evidence or disable guards. Automatic family synthesis, complete
entry-context contracts and unseen-path closure remain future obligations.

## Checks

The C++ codec tests mutate all 409 declared model cells/lanes independently,
compare original/restored member bits and reject corrupt encodings. An independent
declaration inventory test rejects model changes without test inventory updates.
Capture fixtures compare all guest RAM and canonical context before/after;
they cover missing/changed/module-owned cases, absent RAM/context, disabled
capture, exclusive records, event budget and contained write failure.
Preparation tests cover RAM/window mismatch, corrupt context, PC/delay contracts,
existing outputs and consumption by the catalog generator. Catalog tests reject
producer mismatch and prove extension preserves prior source bytes/timestamps.
These checks establish the stated laboratory behavior, not hardware fidelity.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-miss-v1.md -->

---

<a id="anexo-24"></a>

# ANEXO 24 — schemas/nexo-ee-native-data-family-v0.md

Origem: [schemas/nexo-ee-native-data-family-v0.md](schemas/nexo-ee-native-data-family-v0.md). Linhas originais: **131**. Bytes: **7294**. SHA-256: `0bd5c9b5e8fb9fd77cd4ec618ffcd543eb2c99f7aaa92ea6dbdd084f8051032a`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-ee-native-data-family-v0.md -->
# Native EE data-family synthesis, laboratory v0

This frontend implements a restricted conversion step of README §12.6. It
produces C++ ahead of time using the existing EE instruction and delay-slot
emitters. It does not approve closure, discover a producer invariant, prove
fetch/cache/write/alias semantics. Its separate experimental catalog integration
is described in [finite family admission](nexo-ee-family-catalog-v0.md).

## Converter interface

```cpp
std::string ps2recomp::generateNativeDataFamily(
    std::span<const uint32_t> words,
    std::span<const uint32_t> masks);
```

Each input has 1..128 numeric words and the counts must match. Word values
represent a fixed candidate structure, not a program interpreted at runtime.
Masks may be `0xffffffff` or `0xffff0000`. The latter is allowed only for one of
the 20 data classes in `ps2_native_data_operands.h`: LUI(rs=0), ADDIU, SLTI/SLTIU,
ANDI/ORI/XORI and ordinary integer loads/stores. Opcodes, register selection and control encodings
remain exact. Parameters are extracted as uint16 values; SW emission explicitly
sign extends its value through int16/int32 before 32-bit address addition.

Initially, supported regions are linear integer/ordinary memory instructions,
optionally ending in one J/JAL, register JR/JALR or integer conditional branch and its
complete architectural slot. Conditional destinations, links, internal targets
and loop/checkpoint locations use relative PCs. Branch-likely and REGIMM link
forms retain the existing conservative slot/link policy.
Direct J/JAL preserve their fixed target bits and construct the destination from
the actual architectural PC high nibble. A destination is mapped to a local
label only after comparison against the actual relocated region; local
backedges retain checkpoints. External destinations use the existing runtime
directory. JAL links relocate before its slot. No jump target
field is a parameter in this profile.
Local indirect-target specialization, coprocessor branches, syscalls,
other unsupported data/device operations and unsupported reserved fields are
explicitly rejected. Rejection is an open synthesis obligation, not a success
stub or a request to interpret the rejected bytes.

The CLI consumes two bounded little-endian files (at most 512 bytes each):

```sh
build/ps2xRecomp/ps2_native_data_family words.bin masks.bin fresh-family.cpp
```

It needs an exclusively owned output path and rejects an existing output or
symlink. The tool belongs in the converter. It is not included in a delivered
native game's guest execution path.

## Generated function

```cpp
void ps2native_data_family(uint8_t* ram, R5900Context* context,
                          PS2Runtime* runtime, uint32_t familyBase);
```

The generated wrapper rejects null resources, misaligned/outside physical RAM
bases, PCs outside the complete region, misaligned PCs and pending architectural
delay context before executing effects. It checks every expected guarded RAM word
and extracts the exact live low-16 data fields into a fixed-size temporary array.
The original guest bytes are preserved. Guard mismatch throws `invalid_argument`.

The following body contains fixed native operations. There is no runtime guest
decoder, opcode dispatch loop, guest-to-host compiler or new executable memory.
The comparison loop in the wrapper only checks identity and loads typed data.
Canonical instruction addresses become `ADD32(family_base, offset)` expressions
at precise PC, architectural delay, branch-source and link/return locations.
Every instruction has a normal resume label; an independently entered terminal
slot executes without replaying its preceding branch or link update. Pending
architectural delay context remains unsupported rather than being reclassified.

Masks alone are not a proof that a parameter is unrelated to control or code
writes. Store destinations may alias instructions; caller targets and devices
may change the world. RAM guard success does not prove instruction-cache fetch
identity. Matching observations does not establish universal domains, reachable
caller coverage, scheduler/device timing or independent PS2 correctness.

## Evidence and remaining integration

The synthetic build-time fixture compiles three families and 36 concrete
conservative reference regions. Execution comparisons cover all **168 normal
entries**, including signed-immediate boundaries, relocated PCs, saved-frame
register restoration, JALR link-before-slot behavior and standalone slots. They
compare all fields of the identified EE context codec and every byte of 32 MiB
RAM. Guard tests verify rejected contexts/structural bytes have no effects.

That initial fixture is now expanded to **47 structures / 387 concrete fixtures /
1,518 normal-entry comparisons**: all 20 data classes at signed/unsigned
boundaries, both branch outcomes, branch-likely annulment, REGIMM links, external
positive/negative targets, internal backedges, variable decrements and
parameterized ADDIU-to-zero slots/body operations. A new slot test first failed
because canonical zero erased the concrete emitter's delay metadata; emission
now retains its parameter-dependent decision. Every
comparison checks the identified context and 32 MiB RAM against the shared
emitter reference. This remains a model regression, not independent PS2
equivalence or whole-machine replay.
Direct J/JAL fixtures also compare links, ordinary slots and independently
entered slots at three physical bases and four signed operand boundaries.
Generation checks cover an absolute target of zero without a false local jump.
Actual local J/JAL destinations also exercise a slot entered a second time as
an ordinary instruction, including an observable JAL link update.

The fixture generator also accepts an exact proposal from the candidate report:

```sh
python lab/tests/generate_ee_data_family_fixture.py \
  build/ps2xRecomp/ps2_native_data_family build/ps2xRecomp/ps2_native_overlay \
  observed-fixture.cpp --candidate-json candidates.json --shape SHAPE_SHA256
```

The eight-word saved-frame suffix found at eight captured locations, including
`0x184f474`, passed **64 normal-entry comparisons** using one family function and
the eight observed byte variants/bases. Contexts are constructed test inputs;
this is not a complete replay of the missing-entry game checkpoint. It is not
independent hardware validation, since both paths share EE semantic emitters.

All 34 prior concrete bank sources were regenerated with both the archived and
new converter and remained byte-identical. This guards against accidental
default-generator changes; it does not certify semantic cache correctness.

The bounded immutable catalog and invocation recheck now exist as an opt-in
laboratory experiment. The first game run passed its previous initializer miss
and stopped at the next uncovered callback; full code coverage is still open.

Next: cover whole loaded structures and unsupported control flow in batches;
trace the materializer and
establish admissible parameter domains; validate fetch/writer/alias/timing and
independent fidelity. Complete game dispatch, campaign, native VU, Android and sustained
60 FPS remain unqualified. Source generation or model equality alone cannot
authorize a strict native package.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-ee-native-data-family-v0.md -->

---

<a id="anexo-25"></a>

# ANEXO 25 — schemas/nexo-iop-aot-v0.md

Origem: [schemas/nexo-iop-aot-v0.md](schemas/nexo-iop-aot-v0.md). Linhas originais: **194**. Bytes: **11397**. SHA-256: `3aacfbeda31b6efbf21775b190a32caefb34a3e7f0bfad1c303d46514ca49eda`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-iop-aot-v0.md -->
# NEXO: primeiro caminho IOP AOT (V0)

## Objetivo e autorização

Implementar a tradução conservadora do código IOP prevista no README, seção 15,
e preparar M4. O README e a execução autônoma desse plano já foram autorizados.
Este passo é um contrato interno de laboratório; não conclui M4 nem certifica
um jogo completo. Não altera o README principal.

## Contrato e limites

- A conversão acontece offline. Cada palavra de instrução é um parâmetro C++
  constante; não há decodificação de opcode de bytes convidados durante sua execução.
- O primeiro banco recebe uma visão **já relocada** de RAM, endereço físico
  alinhado e bytes. Todas as posições alinhadas dessa visão recebem entradas,
  inclusive entradas interiores e delay slots. Não há lista manual de callbacks.
- A entrada associa PC físico, palavra esperada e função nativa especializada.
  A verificação da palavra na memória é uma guarda de identidade, não um decoder.
- O banco é copiado e validado na construção. Código ausente, modificado ou
  PC desalinhado produz `UNSEEN_CODE`, com PC e palavra observada, sem fallback.
- O modo nativo usa o kernel, memória e contratos de importação existentes. Uma
  importação não resolvida falha; não se transforma em retorno de sucesso.
- Estado de registradores, COP0, HI/LO, branch e load pendentes é preservado
  entre passos. A semântica inicial deriva do modelo IOP identificado; sua
  igualdade com esse modelo **não** prova fidelidade ao R3000A físico.
- O build estrito exclui o interpretador IOP. Seu símbolo de execução não deve
  existir no executável que valida exclusivamente módulos nativos.
- O modo diagnóstico anterior continua disponível em um build separado. Um
  construtor com banco nativo sempre exige AOT, inclusive nesse build.
- Reset limpa falhas, contadores e RAM, mantendo o banco compilado configurado.
- Falha durante startup não publica módulo carregado; falha durante RPC não
  publica resultado bem-sucedido. A falha é persistente até reset.
- Relocation não suportada e startup que termina apenas por esgotar seu budget
  também produzem falhas, antes de publicar um módulo bem-sucedido.

Ainda faltam: integração das famílias IRX no jogo, descoberta de código latente,
snapshots canônicos independentes da ABI, serviços e tempo qualificados,
lifecycle completo de substituição de módulos e cobertura do corpus comercial.
O banco absoluto original desta etapa
não deve ser anunciado como suporte universal a IRX.

A auditoria do modelo de instruções também precisa cobrir JALR com registradores
aliased/rd zero, merges LWL/LWR com load pendente, exceções em delay slots de
branches não tomados e alinhamento de fetch. A primeira tradução preserva o
modelo identificado; esses casos ainda não têm qualificação independente.

## Estrutura e estilo

- `ps2xIOP/src/emulator/core/iop_native.*`: despacho interno e falhas tipadas.
- `lab/generate_iop_bank.py`: conversor offline determinístico e semântica gerada.
- `ps2xIOP/tests/iop_native_tests.cpp`: estado, código alterado e integração IRX.
- `ps2xIOP/CMakeLists.txt`: build diagnóstico/estrito e geração do banco de teste.

```cpp
if (!native.execute(cpu))
{
    // O chamador recebe a falha; nenhuma instrução genérica é executada.
    return false;
}
```

Sem dependências externas novas. Tipos e nomes seguem o código IOP existente.
Não usar LTO neste ciclo de desenvolvimento.

## Ordem de implementação e critérios

1. Teste primeiro: módulo ELF absoluto com branch/delay slot deve falhar no
   caminho nativo ainda ausente. Banco completo deve executar sem interpretar.
2. Separar helpers de estado/exception do interpretador; gerar funções com
   instruções constantes e diretório de todas as palavras do banco.
3. Integrar a execução estrita ao carregador e ao subsystem, preservando as
   chamadas de serviço e o scheduler existentes.
4. Testar load delay, HI/LO, COP0, overflow, memória, budget por passo, entradas
   interiores, aliases, self modification, módulo desconhecido e import ausente.
5. Build separado sem interpretador; inspecionar símbolos e executar os testes.
6. Regressão dos testes IOP e do runtime. Registrar tempos de build incremental,
   separadamente de tempo de conversão, FPS e duração de campanha.

## Comandos de verificação

```sh
cmake -S ps2xIOP -B build/iop-aot-diagnostic -DPS2X_IOP_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto'
cmake --build build/iop-aot-diagnostic --parallel 4
ctest --test-dir build/iop-aot-diagnostic --output-on-failure
cmake -S ps2xIOP -B build/iop-aot-strict -DPS2X_IOP_BUILD_TESTS=ON -DPS2X_IOP_ENABLE_INTERPRETER=OFF -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CXX_FLAGS_RELEASE=-O1 -DNDEBUG -fno-lto'
cmake --build build/iop-aot-strict --target ps2_iop_native_tests --parallel 4
ctest --test-dir build/iop-aot-strict -R ps2_iop_native_tests --output-on-failure
nm -C build/iop-aot-strict/ps2_iop_native_tests
```

## Fronteiras

Sempre: validar bancos, manter erros reproduzíveis, testar antes de commit.
Esclarecer com o usuário somente se surgir requisito externo ao plano autorizado.
Nunca: fallback interpretado no modo nativo, alterar vendors, publicar ISO/assets,
usar APIs pagas, afirmar M4 completo a partir apenas destes testes sintéticos.

## Próximo passo: frontend do carregador e corpus comercial

Adicionar um inspector offline que usa o carregador identificado, rejeita
relocations incompletas e produz RAM já relocada mais metadados de base, entry,
GP e tamanho. Um probe separado liga esse banco ao subsystem estrito, carrega
o IRX original e registra startup, contadores, diagnósticos e RAM final.
Um probe diagnóstico separado usa o interpretador como referência provisória.
Comparar retorno, contadores e RAM é uma verificação limitada: não qualifica
estado oculto, hardware, serviços nem a campanha. Chamadas externas não
implementadas pelo host do probe devem falhar, sem simular sucesso.

Os módulos comerciais e o C++ derivado ficam em `build/`, fora do Git. Antes
do corpus comercial, testar CLI, limites e relocations em um ELF sintético.
Depois exercitar IRX reais da ISO já inventariada. O binding de diferentes
bases ainda será uma etapa seguinte: este probe não exige que o jogo use
permanentemente a base escolhida para o teste.

## Famílias relocáveis de IRX

O próximo banco contém a identidade completa dos bytes IRX e uma entrada por
palavra do span carregado. O inspector exporta as máscaras das escritas de
relocation e a imagem fonte. O gerador distingue três casos:

- Palavra fixa: função já especializada, com guarda de igualdade exata.
- J/JAL com R_MIPS_26 ou operação I com imediato de 16 bits: operação e
  registradores constantes, operando ligado pelo carregador antes do startup.
- Relocation de palavra inteira ou forma não admitida: posição sem função
  executável; entrar nela produz falha, nunca decodificação genérica.

A máscara não pode alterar opcode nem campos que escolhem a operação. As
funções parametrizadas são geradas offline e recebem apenas operandos; opcode,
registradores e formas de controle continuam constantes em C++.

O despacho copia imagens e descritores na construção. Depois das relocations,
confere identidade da imagem, dimensões, máscaras e bits fixos e publica um
diretório ligado à base efetiva. A palavra completa ligada fica como guarda
contra alterações posteriores, incluindo alterações apenas no operando.
Uma nova ligação que sobrepõe uma anterior invalida o diretório anterior inteiro.
Reset remove ligações e falhas, preservando as famílias compiladas e o banco
absoluto configurado. Uma falha de ligação impede startup e publicação do módulo.
Unload retira o diretório do módulo; a substituição completa do estado de kernel,
threads e serviços permanece uma obrigação independente.

Testar primeiro dois endereços, parâmetros HI/LO e J, dados R_MIPS_32 não
executáveis, identidade completa, ownership, substituição, reset e alteração
posterior. Depois gerar a família HKSIF original e carregar duas instâncias
consecutivas pelo subsystem estrito sem escolher seus endereços manualmente.
Essa ligação usa o carregador identificado como materializador confiável;
não prova fidelidade independente, fechamento de código gerado pelo jogo,
publicação concorrente/epochs nem substituição completa do kernel.

## Catálogo de múltiplos módulos e funções compartilhadas

Gerar um registro que reúne famílias de várias imagens, sem recompilar o
executável a cada seleção de IRX. Cada imagem mantém seu diretório, identidade
completa e guardas. Máscaras e bits fixos iguais devem normalizar para o mesmo
descritor, independentemente da base usada no inspector.

Para operações I e J/JAL, também compartilhar a forma compilada quando o
imediato/target é constante no arquivo original. Nesse caso a guarda continua
exigindo a palavra completa, pois a máscara de relocation é zero. Passar o
operando ao callback não permite variar opcode/registradores nem relaxa a
identidade do código. Formas sem operando compartilhável continuam fixas.

Emitir funções em um pool separado, instanciado uma vez por forma, dividido em
64 arquivos por hash estável. Os diretórios somente referenciam as funções;
não incluem suas definições. Acrescentar um módulo deve preservar arquivos
anteriores que não dependem dele. Identidades duplicadas podem ser deduplicadas
apenas quando tamanho, máscaras e todos os bits fixos coincidem.

O manifesto JSON declara somente basenames C++ gerados, schema e inventário.
CMake valida o schema, os nomes, a existência e os hashes dos arquivos,
incluindo o header de semântica. Mudanças nesses arquivos exigem nova validação
antes do build, mesmo sem alteração no manifesto. A configuração
estrita usa esse catálogo sem interpretador IOP. A integração experimental ao
PS2Runtime exige explicitamente catálogo e interpretador desabilitado.

Validar primeiro deduplicação, bases, estabilidade incremental, guardas e links
com dois IRX sintéticos. Depois compilar o catálogo dos 11 IRX externos da ISO
e observar todos os startups, incluindo falhas, sem inventar serviços. Registrar
resultados por módulo; não declarar M4/jogo aprovado a partir de startup.

## Dependências dos imports e sequência de módulos

Antes de substituir um stub por serviço, exigir identidade admitida para o
stub e seu delay slot, além do intervalo de metadados e stubs precedentes
consultado pelo decoder identificado. Nome de biblioteca conhecido não pode
contornar um banco vazio. Mudança de versão, nome ou ordinal deve falhar antes
de efeitos de serviço. Dados relocados podem manter guardas de identidade,
mas nunca recebem callbacks executáveis por essa razão.

O probe de runtime aceita uma sequência limitada de módulos e um orçamento
de ciclos EE após cada carga. Exercitar a ordem observada no jogo e o trabalho
das threads, preservando snapshots de contadores, retornos e RAM. Igualdade com
o modelo identificado não aprova seus serviços, avisos de erro do convidado,
interrupções, clocks ou estado oculto. Publicação/epochs e contratos independentes
de serviços continuam sendo obrigações separadas.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-iop-aot-v0.md -->

---

<a id="anexo-26"></a>

# ANEXO 26 — schemas/nexo-observed-vif-case-v1.md

Origem: [schemas/nexo-observed-vif-case-v1.md](schemas/nexo-observed-vif-case-v1.md). Linhas originais: **162**. Bytes: **8881**. SHA-256: `b0634250b441d47d7948fc6b16bf3e6e5030ca1ee575eccf3c1666dca83944df`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-observed-vif-case-v1.md -->
# NEXO observed VIF call, version 1

This laboratory case preserves the literal argument of one completed
`PS2Memory::processVIF1Data` call and the surrounding **current runtime model**.
It is not an independent hardware trace, complete PS2 machine checkpoint,
proof of code closure, or final native game package. Successful replay against
the recording has assurance `tested_only`.

## Acquisition and ownership

In a lab-enabled runtime, set `PS2X_CAPTURE_SCENE` to a private directory and
create `.vif-request` there. The request survives calls without an observed VU
callback. The first successfully observed VU-bearing call writes a new case
directory. `.complete` contains exactly one byte, `01`, and is written last;
only then is the request removed. Failure preserves the request and does not
publish a completed case. A partly written directory is not evidence.

Capture begins before pending VIF command bytes are prepended. Original input
is copied; preexisting pending bytes belong to the input checkpoint. No
synthesized MSCNT command or normalized VU capture substitutes for this input.
Arguments aliasing VU code, VU data or GS VRAM are rejected because replay does
not preserve that mutable alias. Recursive captured VIF calls are marked
incomplete. Non-strict acquisition errors preserve the guest's failure behavior.

The normal synchronous runtime CPU worker owns the call. A per-memory lease
blocks the runtime's host presentation function across before-state, execution
and after-state acquisition. The observer is local to its thread and filters
the memory instance; it does not replace callbacks. **All other writers must
remain quiescent.** This lease does not pause arbitrary custom callbacks,
external threads, DMA producers or an independently operating renderer. The
format has no general external-input or floating-environment closure proof.

## Case files

| File | Contents |
| --- | --- |
| `capture.json` | Schema `nexo.observed.vif.call.v1`, counts, provider, horizon and scope |
| `vif-input.bin` | Literal nonempty input argument; maximum 32 MiB |
| `input-state.nexo` | Composite boundary immediately before parsing |
| `output-state.nexo` | Composite boundary after the completed call |
| `events.nexo` | Ordered VU callback boundaries and GIF submissions/deliveries |
| `bank-N.bin` | Full 16 KiB VU1 code identity before an executed callback |
| `.complete` | Single byte `01`, published last |

Bank indices are contiguous from zero and assigned by first observation.
Identical banks are deduplicated using their complete bytes. Only executed
identities are represented; an uploaded but unexecuted bank need not appear.
At most 256 VU callbacks are recorded. Current replay callbacks have a 65,536
cycle horizon. The finite collection converter verifies file inventory,
unique identities, counts and horizon before invoking its separate inspector.

An enclosing immutable recording manifest must bind these files and the
producer's source, binaries and build configuration with SHA-256. CRC32 checks
internal corruption; it provides no provenance or correctness attestation.

## Composite state

The common 24-byte little-endian envelope and CRC are defined in
`nexo-device-state-v1.md`. Composite magic is `NEXOVCS` followed by zero,
variant 1; maximum size is 272 MiB including the envelope.

Payload order:

1. CPU-visible VU FBRST and VPU status, each `u32`.
2. VU1 code generation, `u64`.
3. VIF1, GIF arbitration, CPU GS and VU1 canonical states, each a length-prefixed
   blob, with bounds 64 MiB, 64 MiB, 128 MiB and 80 KiB respectively.
4. VU1 code and VU1 data blobs, each exactly 16 KiB.

Component decoders retain their own magic, variants and validation. GS includes
VRAM, partial vertices/transfers, loaded CLUT and stale texture-page state,
readback progress, presentation values and privileged registers. VU includes
hidden readiness, flag, branch, Q/P/EFU and pending-write state.

Replay restores a fresh, private, windowless runtime. Component failure destroys
that instance before returning a result; it does not publish a partially
restored caller-owned machine. Code generation is restored exactly. Device
routing and callbacks belong to the newly initialized runtime. The recorded
active CPU's relevant VU fields become the private runtime CPU's fields.

This boundary excludes the complete EE/IOP machines, general RAM/scratchpad,
DMA/scheduler/interrupt continuations, audio, peripherals and GPU state.
Custom callbacks with additional dependencies cannot be qualified by this
boundary merely because a completion marker exists.

## Ordered event trace

Magic is `NEXOVTR` followed by zero, variant 1, maximum 64 MiB. The payload
starts with an event count `u32`. Each event contains, in order:

`kind:u8, absoluteVuCycle:u64, arguments[5]:u32, data:blob`.

| Kind | Arguments | Data |
| --- | --- | --- |
| 1: before VU callback | VIF opcode, PC, TOP, ITOP, bank index | Empty |
| 2: GIF submission | Path ID, drain-immediately, DIRECTHL, zero, zero | Submitted packet bytes |
| 3: GS delivery | Five zeros | Packet actually delivered by the GIF arbiter |

Submission is observed before path masking/arbitration; delivery is observed
at the initialized runtime's GS receiver. Preserving both distinguishes a
queued or masked submission from one processed by GS. Event time is the
absolute VU model clock, not a certified global PS2/device clock. Maximum
event count is 262,144. No host addresses are serialized.

## Finite native collection and reuse

`generate_vif_banks.py` is a conversion tool. It inspects every observed bank
in a batch and emits one C++ translation unit per identity plus a static
registry. The game/replay runtime does not invoke the inspector or compiler.
The native callbacks require an exact full-code identity and reject unknown
identities/entries with `UNSEEN_CODE`; there is no reference fallback.

Metadata cache identity covers code bytes, inspector binary, C++ emitter and
collection converter. Cached metadata has an integrity hash and is validated
again against code words, widths, control bits and descriptor fields. Invalid
cache entries are regenerated. Identical C++ bytes retain their modification
times so existing object files remain reusable. Cache reuse has assurance
`unknown`; it neither broadens closure nor independently approves semantics.

On Linux GNU/LLVM ABI, the native replay links eight fatal wrappers for generic
VU interpreter/numeric entry points. These guards apply to this execution path;
the broader linked archive still contains legacy interpreters, and this is not
a whole-package reachability proof.

CLI stdout is one JSON result; runtime diagnostics go to stderr. Comparison
reports full-state/event equality and the first differing byte per component.
For equally sized state envelopes, derived CRC differences are skipped when
reporting a first payload divergence. Timing covers the VIF call, VU callbacks,
GIF/CPU-GS work and trace recording; it excludes file input, machine construction,
restore, final encoding and comparison. It is not game FPS.

## Optional host profiling

`nexo_vif_replay case [iterations] --profile` and the native CLI add
`host_profile` to their JSON. Default execution leaves profiling disabled and
all profiling counters zero. Profiling does not change this canonical format
or insert wall clocks into the event trace.

Three RAII scopes bracket VU callbacks, GIF submission and actual GS receiver
execution. Each reports call count and inclusive/exclusive wall nanoseconds.
Exclusive time subtracts directly nested measured scopes belonging to the
same observation; inclusive values must not be added together. Observations
and nesting are thread-local and filter the owning memory instance. Scopes
must be nested in stack order and destroyed before their observation owner.
Exception unwinding closes the scope and restores its previous parent.

The GS scope starts after delivery observation and encloses `processGIFPacket`.
The VU scope starts after bank/callback observation. Time spent copying packet
bytes, constructing timing scopes or performing other work outside a child
scope can remain in its parent's exclusive value or the VIF residual. These
are instrumented callback wall measurements, not isolated ISA throughput or
certified hardware timings. Host contention and instrumentation overhead must
be reported. State/events must still match when profiling is enabled.

## Remaining acceptance gates

Root README section 31.1 still requires an identified independent reference
and qualified optimization. M1 also requires replay on a second architecture.
Broader external-input closure, strict final EE/IOP/VU AOT, GPU compatibility,
whole-game progression/audio/controls/saves and physical Android qualification
remain separate gates. One finite observed case cannot approve a whole game
or unseen ISOs.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-observed-vif-case-v1.md -->

---

<a id="anexo-27"></a>

# ANEXO 27 — schemas/nexo-vu-aot-v0.md

Origem: [schemas/nexo-vu-aot-v0.md](schemas/nexo-vu-aot-v0.md). Linhas originais: **40**. Bytes: **2487**. SHA-256: `f5e0d68e2efbda53342b9a1bd0cdd39cb8fcefbea355932932457d209af1a1ae`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-vu-aot-v0.md -->
# NEXO VU AOT V0 laboratory contract

V0 compiles a finite, identified microcode bank before execution. Each native
entry binds a static dependency descriptor and operation functions specialized
by compile-time instruction constants. The native scheduler consumes data and
this immutable bank; it does not fetch or decode a guest instruction stream.

The converter frontend may use the current decoder to derive descriptors. It
belongs to a separate target and is excluded from the native execution path.
The first implementation shares the current pipeline/device helpers, while
specializing opcode-dependent numeric/flag helpers as well as upper/lower
operations. Calling a generic `execUpper`, `execLower`, `execute`, `resume` or
interpreter `run` is prohibited on that path.

Preserved state includes writeback ordering, flags, Q/P and resource readiness,
ACC forwarding, upper/lower shadowing, VI branch history, delay slots, D/T/E
termination and XGKICK's future reads. Canonical checkpoints must match the
reference after every tested pause/resume boundary, not just at program end.

Unknown or uncompiled entries emit `UNSEEN_CODE`; there is no interpreter or
runtime guest compiler fallback. The replay adapter checks the captured code
against the bank's immutable byte identity before invoking native execution.
Byte comparison/hash identification is not guest instruction execution.

Address validation checks alignment and `pc <= code_size - 8` before indexing
the table. Adding eight to an untrusted 32-bit PC is not a safe bounds check:
the addition can wrap. The native API rejects this case even when invoked
directly with mutable machine state, outside the canonical checkpoint decoder.

The current laboratory executable uses eight Linux GNU/LLVM link wrappers to
reject generic execution and opcode-dependent FMAC entry points. Tests activate
the wrappers deliberately, then compare native and reference continuations.
Undefined-symbol inspection of the bank and scheduler objects is an additional
regression check. Neither mechanism proves that all interpreter code is absent
from the executable; the prototype links shared legacy runtime components.

Initial approval is `tested_only` against the existing implementation. It does
not establish independent hardware correctness, code closure across uploads,
the complete VIF/GIF/GS boundary, final package interpreter absence, universal
compatibility or sustained speed. Those remain separate gates in the root plan.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-vu-aot-v0.md -->

---

<a id="anexo-28"></a>

# ANEXO 28 — schemas/nexo-vu-capture-v1.md

Origem: [schemas/nexo-vu-capture-v1.md](schemas/nexo-vu-capture-v1.md). Linhas originais: **48**. Bytes: **2573**. SHA-256: `e7511d58280ccf6f30171964938a130b062d90654e0549ad011a8e603958744e`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-vu-capture-v1.md -->
# NEXO current-runtime VU capture, version 1

Opt-in laboratory builds (`PS2X_BUILD_NEXO_LAB=ON`) consume a local
`PS2X_CAPTURE_SCENE/.vu-request` marker at the next VU1 MSCAL or MSCNT boundary.
Execution must be synchronous with code/data writers. A capture contains:

| File | Meaning |
|---|---|
| `input-state.nexo` | Canonical VU state after call-boundary normalization, before execution |
| `code.bin` | Raw microcode memory at that boundary |
| `input-data.bin` | Raw VU data memory at that boundary |
| `output-state.nexo` | Canonical VU state after the execution budget/end/stop |
| `output-data.bin` | Raw VU data memory after execution |
| `path1-events.nexo` | VU-to-GIF submissions during this execution, in causal order |
| `trace.txt` | Diagnostic call kind, budget, stop reason, and bounded issue history |

`.nexo` VU state files follow `nexo-vu-state-v1.md`. Microcode and data are byte
memories, not native C++ object images. Legacy `input-state.bin`,
`output-state.bin` and `trace-state.bin` remain local-ABI diagnostics and are
**not** portable replay inputs. A surrounding manifest must bind portable
files and the exact producer implementation with SHA-256.

Restore the canonical input, then use continuation semantics for both capture
kinds. The captured MSCAL input already has freshly reset pipelines; resetting
it again repeats normalization unnecessarily. The existing fresh-call path
retains its absolute scheduler clock. The MSCNT input preserves
in-flight writes, flags, scalar units, branch history and XGKICK bytes.

## PATH1 stream

All integer fields are little-endian. Header:

- Bytes 0..7: ASCII `NEXOGIF` followed by a zero byte.
- Bytes 8..11: `u32` version, exactly 1.
- Bytes 12..15: `u32` record count.

Each record is `cycleOffset:u64`, `packetSize:u32`, then exactly `packetSize`
raw bytes. The offset is the submission cycle minus the input snapshot clock.
Sizes must be multiples of 16, from 16 to 65,536. Offsets are nondecreasing;
several submissions can occur at the same cycle. The complete file is bounded
to 64 MiB. An empty stream still contains the 16-byte header. A recording error
prevents a complete portable capture from being published.

This observes **submission** before GIF arbitration. It does not establish
when GS consumes the packet, or snapshot pending GIF paths, VIF commands,
GS transfer state, VRAM or external concurrent writers. Those components
remain required for the complete first increment in README section 31.1.
Current replay comparisons must explicitly identify this narrower VU boundary.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-vu-capture-v1.md -->

---

<a id="anexo-29"></a>

# ANEXO 29 — schemas/nexo-vu-runtime-binding-v0.md

Origem: [schemas/nexo-vu-runtime-binding-v0.md](schemas/nexo-vu-runtime-binding-v0.md). Linhas originais: **60**. Bytes: **3367**. SHA-256: `fb3deec03d25387f6e18c19f355fc6e7f859f863be8c1bcaab5e4a2400154819`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-vu-runtime-binding-v0.md -->
# NEXO VU1 native runtime binding, laboratory V0

`bindNativeVu1(runtime, programs)` attaches a finite collection of previously
compiled VU1 banks to an initialized, windowless or normal memory session. It
uses the existing VIF MSCAL/MSCALF and MSCNT callbacks. The runtime and its memory
layout remain unchanged. Conversion and guest-code compilation are prohibited
in these callbacks.

## Ownership and installation

Bank identities, descriptors and entry arrays are copied into immutable owned
storage. Compiled function pointers must remain loaded until the runtime is
destroyed. Empty collections, non-VU1 units, invalid bank sizes and duplicate
byte identities are rejected before a previous installation is replaced.

The binding synchronizes initialized core subsystems before installing its
callbacks. Those callbacks own the bank collection for the memory session.
An explicit host memory reinitialization and subsequent core rebinding require
installation again. This laboratory adapter does not change the runtime's
unbound reference behavior or establish a strict final-package build.

## Selection and execution

Every invocation compares the current complete microcode bytes to a compiled
bank identity before modifying VU state. A previous candidate can be checked
first, but neither a pointer nor the code-generation counter substitutes for
the byte comparison. Raw mutable code views must not cause stale selection.
Duplicate identities are ambiguous even if function pointers happen to match.

An unknown identity throws `UNSEEN_CODE` before fresh-call normalization. There
is no reference interpreter fallback after installation. VIF may already have
latched TOP/ITOP and updated its own flags when this host contract failure is
raised; it is not a guest exception or a transactional rollback of the whole
VIF operation.

MSCAL/MSCALF use native fresh-call normalization; MSCNT preserves the current
clock and pending pipelines. Both propagate FBRST's D/T enable bits and publish
the resulting stop flags in the CPU-visible VPU status. TOP/ITOP come from the
normal VIF parser. A code change between calls selects the corresponding bank
without resetting continuation state.

`nexo_vu_runtime_replay` reconstructs a normalized VU checkpoint and synthesizes
MSCNT for integration. TOP/ITOP and FBRST D/T enable inputs come from that
checkpoint. It requires the callback's exact 65,536-cycle budget. TOP/ITOP values
outside VIF's 10-bit callback domain are rejected instead of silently masked.
Machine construction, bank ownership, file input, checkpoint restoration and
comparison are outside its measured interval. VIF processing, bank selection,
native VU execution and submission observation are inside it.

## Scope and acceptance

Validation uses the real VIF parser and PS2Runtime, canonical VU state and link
traps against generic VU execution. No EE instructions or user window are
needed by the fixtures. The EE lookup fixture contains only null entries.

The current VIF callbacks execute synchronously. Concurrent guest code writes
during a native invocation, fetch visibility beyond this model, VIF/GIF/GS
canonical replay, upload-family closure and final package interpreter absence
remain separate unfinished requirements. Bank identity checks establish which
compiled code is selected; they do not prove equivalence or code closure.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-vu-runtime-binding-v0.md -->

---

<a id="anexo-30"></a>

# ANEXO 30 — schemas/nexo-vu-state-v1.md

Origem: [schemas/nexo-vu-state-v1.md](schemas/nexo-vu-state-v1.md). Linhas originais: **88**. Bytes: **4834**. SHA-256: `3d49bc5660b3acada362420204adcd076a55bfce94364afdd29bf02ea118d9f1`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-vu-state-v1.md -->
# NEXO VU state format, version 1

This is a checkpoint of **all persistent execution state in the current
`VU1Interpreter` implementation**, at an instruction-pair boundary. It is not
a claim that this implementation models every hidden state of PS2 hardware.
Version 1 has an implicitly empty second-XGKICK request slot. Checkpoints with
an already-issued request waiting for PATH1 use the lossless extension in
`nexo-vu-state-v2.md`. Empty-slot states keep their existing version-1 bytes.
The producer must pause execution and concurrent writers before serialization.
The surrounding replay must identify the semantic implementation and capture
code, data, VIF, GIF, GS, and external events at the same causal boundary.

## Encoding

No pointer, padding, native `size_t`, C++ object image, or native boolean is
serialized. Integer fields use their stated width, little-endian. `f32-bits`
means a raw 32-bit pattern, including signed zeros, denormals and NaN payloads;
no floating-point conversion is performed. A `bool8` must be exactly 0 or 1.
Arrays are row-major. No alignment bytes occur between fields.

| Offset | Width | Header field |
|---:|---:|---|
| 0 | 8 | ASCII `NEXOVU` followed by two zero bytes |
| 8 | 4 | Format version, `u32`, exactly 1 |
| 12 | 4 | Unit, `u32`: 0 for VU0, 1 for VU1 |
| 16 | 4 | Payload byte length, `u32` |
| 20 | 4 | CRC-32/ISO-HDLC, `u32` |
| 24 | payload length | Fields in the order below |

The CRC covers bytes 0..19 and 24..end, excluding its own field. Polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`. CRC detects
accidental corruption; it is neither a signature nor proof of fidelity. Replay
manifests must additionally bind blobs with SHA-256. Readers reject unknown
versions, invalid unit, inconsistent length, trailing fields, invalid booleans,
invalid descriptors, checksum errors and clocks inconsistent with the state.
Valid pending deadlines and resource/readiness clocks cannot exceed the current
clock by more than 64 cycles. The current implementation's longest scalar
latency is 54; other issue/writeback latencies are shorter. This model-specific
bound prevents fabricated deadlines from driving unbounded pipeline flushes.
The clock must also retain room for that horizon without `u64` overflow.
The allocation/input bound is 80 KiB. Decode is transactional: failure cannot
change the destination execution state. A snapshot cannot change its unit.

## Payload order

1. Architectural fields:
   `VF[32][4]:f32-bits`, `VI[16]:i32`, `ACC[4]:f32-bits`,
   `Q,P,I:f32-bits`, `R,PC,MAC,CLIP,STATUS:u32`, `cycles:u64`,
   `ebit,haltAfterDelaySlot,dBitEnabled,tBitEnabled,stoppedByD,stoppedByT:bool8`,
   `TOP,ITOP:u32`, `branchPending:bool8`, `branchTarget,branchDelay:u32`.
2. Eight flag entries, each:
   `readyCycle,issueCycle:u64`, `mac,status,extraSticky,clip:u32`,
   `valid,writesMac,writesStatus,writesSticky,writesClip:bool8`.
3. FDIV entry, then two EFU entries, each:
   `readyCycle:u64`, `value:f32-bits`, `statusDi:u32`, `valid:bool8`.
4. Eight pending stores, each:
   `readyCycle:u64`, `address:u32`, `words[4]:u32`, `laneMask:u8`, `valid:bool8`.
5. Sixteen pending VF writes, each:
   `readyCycle,sequence:u64`, `value[4]:f32-bits`, `reg,laneMask:u8`, `valid:bool8`.
6. Eight pending VI writes, each:
   `readyCycle,sequence:u64`, `value:i32`, `reg:u8`, `valid:bool8`.
7. Eight pending ACC writes, each:
   `readyCycle,sequence:u64`, `value[4]:f32-bits`, `laneMask:u8`, `valid:bool8`.
8. XGKICK:
   `packet[65536]:u8`, `sourceAddress,totalBytes,copiedBytes,currentTagEnd,cycleCredit:u32`,
   `issueCycle:u64`, `active,currentTagEop:bool8`.
9. Readiness:
   `vfReady[32][4],viReady[16],accReady[4]:u64`.
10. Latest writes:
    `vfLatestWrite[32][4],viLatestWrite[16],accLatestWrite[4]:u64`.
11. Scheduler:
    `cycle,nextWriteSequence,efuResourceReady:u64`,
    `workingClip,currentUpperInstruction:u32`, `viBranchBackupValue:i32`,
    `viBranchBackupReg:u8`, `viBranchBackupValid,stopRequested,pendingHaltD,pendingHaltT:bool8`.

Inactive pipeline slots and the whole XGKICK buffer are included. That keeps
checkpoint identity reproducible without relying on padding or implicit reset
rules. Host bindings and derived decode caches are not execution state: restore
clears them, and the next execution binds them to the receiving machine.

## Scope of validation

The current tests compare pause/restore/resume against uninterrupted continuation
of the same runtime, including deferred vector/scalar/integer writes, flags,
branch history, EFU resources and in-flight PATH1 packets. These checks prove
regression properties on those cases. They are not an independent hardware
oracle, a VU AOT implementation, a complete VIF/GIF/GS replay, or approval of
the M1 two-architecture gate. Those remain explicit work in the NEXO migration.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-vu-state-v1.md -->

---

<a id="anexo-31"></a>

# ANEXO 31 — schemas/nexo-vu-state-v2.md

Origem: [schemas/nexo-vu-state-v2.md](schemas/nexo-vu-state-v2.md). Linhas originais: **45**. Bytes: **2466**. SHA-256: `a39f4d146a41485aa31e8c3308ddf55f273e4b4a7ed63f95a862befeb3351a4e`.

Este é o texto integral do documento de origem no snapshot. Seu estado e suas medições têm o escopo e a data descritos no próprio texto; conferir a síntese atualizada para o estado consolidado.

<!-- PS2NATIVE_SOURCE_BEGIN:schemas/nexo-vu-state-v2.md -->
# NEXO queued XGKICK state extension, version 2

This extends the current-model canonical VU checkpoint to cover an already
issued second XGKICK waiting for the first PATH1 transfer. It preserves the
second pair's pending Upper result and source operand across suspension.
It does not certify the complete hardware or device scheduling model.

## Encoding and domain

The header, field widths, endianness, CRC algorithm, unit rule, 80 KiB bound,
transactional restore, and original payload order are those of version 1.
The header version is 2. The complete version-1 payload is followed by:

| Absolute offset | Width | Field |
|---:|---:|---|
| 70,249 | 4 | Queued source byte address, u32 |
| 70,253 | 8 | Original issue cycle, u64 |
| 70,261 | 1 | Pending request, bool8, exactly 1 |

The complete envelope is 70,262 bytes. The queued source must be aligned to
16 bytes and below 16 KiB. Its issue cycle cannot exceed the checkpoint clock.
A queued request requires unit VU1 and an active preceding transfer.
It adds no host pointer, cached instruction, or eagerly copied second packet.
The second packet is read later from the supplied VU data memory.

An empty slot has zero address and issue clock, and is encoded using version 1.
Version 2 with pending=false is rejected, keeping one canonical representation
for each supported state. Version-1 restore explicitly gets the reset empty
slot from its fresh candidate; it cannot inherit a destination's pending work.
Unknown versions, trailing fields, malformed booleans, invalid descriptors,
future issue clocks, and checksum failures leave the destination unchanged.

## Scheduling interpretation

The second XGKICK latches its source at issue. Its Upper instruction issues in
the same pair, while the previous packet continues transferring. The next pair
waits for the preceding transfer's completion. On completion, the slot is
cleared and its request starts the next transfer; later pairs may issue while
that transfer runs. Packet order is preserved across arbitrary budget pauses.

The basis for this ordering is the hardware manufacturer's VU User's Manual
version 6.0, section 3.4.9 and the XGKICK instruction description, pp.52/196.
This source describes a stall on the following pair. [Manual mirror](https://studylib.net/doc/25815876/vuusersmanual.158394566).
Exact physical transfer rate, blocked GIF arbitration, E-termination timing,
and concurrent device scheduling remain separate obligations.
<!-- PS2NATIVE_SOURCE_END:schemas/nexo-vu-state-v2.md -->

<!-- PS2NATIVE_DOCUMENTATION_APPENDICES_END -->
