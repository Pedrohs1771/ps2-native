# Beta v0.1 — menus nativos, áudio e controles

Objetivo ativo do usuário em 02/10/2026: converter automaticamente ISOs PS2,
chegar aos menus com áudio e controles corretos e apresentação host a 60 FPS;
quando o jogador iniciar uma partida, encerrar a execução convidada e mostrar
Coming soon. A meta de compatibilidade é 90% e continua aberta. Não qualificar
o objetivo por um percentual de builds ou por apenas cinco jogos.

## Corpus autorizado e identificado

Todos os cinco arquivos estão em `/home/pedrohs/Downloads` e tiveram SYSTEM.CNF
e boot ELF MIPS R5900 reconhecidos pelo inspector existente:

| ISO | Serial | Perfil |
|---|---|---|
| Metal Slug 4 (Europe) | SLES_533.80 | PAL |
| R-Type Final (Japan) (Taikenban) | SLPM_602.02 | NTSC; versão de demonstração |
| RoboCop (Europe) (En,Fr,De,Es,It) | SLES_513.74 | PAL |
| Sega Ages 2500 Vol. 21 — SDI & Quartet | SLPM_626.92 | NTSC |
| Monster House BR-USA T2.0 | SLUS_214.00 | NTSC; imagem traduzida |

Relatórios e hashes completos: `build/beta-v01-20261002/inspection-corrected/`.
As quatro primeiras ISOs são as da imagem fornecida. Não baixar outros jogos.

## Aceitação por ISO

1. ISO → pacote host sem edição de PCs/TOML pelo usuário.
2. Intro e menu principal corretos; não aprovar tela preta, frame de logo parado
   ou uma screenshot sem execução sustentada.
3. Navegar entre seleções e submenus e retornar, registrando inputs realmente
   enviados e efeitos causais. Teclado e controle devem usar o mesmo estado pad.
4. Áudio do menu correto: capturar o PCM/sink isolado e verificar conteúdo,
   duração, cadência e resposta aos controles. Estar com dispositivo aberto
   ou buffer não vazio não comprova que o áudio do jogo está correto.
5. Apresentação host a 60 FPS sustentados, com frame times e progresso convidado
   medidos. Preservar o tempo PAL/NTSC do jogo; repetir frame PAL no monitor
   não significa alterar a lógica de 50 para 60 Hz.
6. Na transição **qualificada** de iniciar/carregar partida, parar o convidado,
   liberar áudio/threads e apresentar Coming soon. Falha de boot, miss, timeout
   ou exceção não pode ser disfarçada como essa transição esperada.
7. Auditar EE/IOP/VU executados no percurso: não reivindicar AOT completo se
   o pacote final executa ISA interpreter/JIT/compilador durante a partida.

Menus podem usar geometria 3D, VU e efeitos GS. Não desabilitar triângulos ou VU
globalmente para separar menu de gameplay. Corrigir esses componentes somente
quando forem necessários ao percurso de menu; adiar cenários da campanha.

## Trabalho atual e ordem

- [x] Identificar as cinco imagens e seus boot ELFs com o inspector.
- [x] Inventário/deduplicação e união offline de capturas implementados em
  `tools.ps2native.corpus_batch`; 11 testes e uma união sintética real exercitados.
- [x] Converter e executar o primeiro título novo em headless delimitado:
  Metal Slug 4; isso ainda não aprova intro/menu.
- [x] Reduzir/corrigir o primeiro bloqueio observado com regressão geral:
  libgif, com replay que passou de tela preta para abertura animada.
- [ ] Instrumentar progresso, input, áudio e frame time por percurso.
- [ ] Integrar a política de transição Coming soon sem converter falhas em sucesso.
- [ ] Fechar o ciclo offline de miss → delta → relink → replay por pacote.
- [ ] Executar os demais títulos, conservando motivos de recusa e reuso.
- [ ] Validar o cache de capacidades/engine por assinaturas e contratos; impedir
  que um perfil de um título aprove automaticamente outro jogo da mesma engine.
- [ ] Publicar beta reproduzível somente com os estados de aceitação medidos.
- [ ] Ampliar catálogo representativo até comprovar a meta de 90%.

Um fingerprint de engine ajuda a selecionar regras já verificadas; não identifica
automaticamente todos os assets, microprogramas, overlays e serviços. Reusar
corpos/contratos válidos, mantendo bindings e identidades privados por imagem.
O mecanismo de adaptação deve continuar a aprender de contraexemplos reais.

## Recursos e retomada

Correções verificadas nesta retomada:

- `headless_native_test launch --duration N` delimita a sessão e distingue
  duração atingida, interrupção e saída/falha. Nenhum estado aprova o menu.
- Timeout de ferramentas POSIX encerra o grupo próprio de CMake/make/compiladores;
  a regressão real demonstrou o compilador órfão antes da correção.
- Descoberta ELF prefere chamadas alcançáveis, admite hints com janela de
  encoding válida e mantém palavras reservadas para recusa explícita. Não há
  prova de descoberta completa; chamadas indiretas continuam exigindo evidência.
- Regressões: 493 testes C++ e 50 Python passaram. O helper escolhe display livre
  acima de 90 sem o modo automático que ignora esse mínimo em wrappers modernos;
  uma execução real confirmou encerramento do runner, Xvfb e sink próprios.
  Recibos de build agora refletem PCH/workers enviados ao CMake.
  Em Metal Slug, a maior unidade
  gerada caiu de 45.045.617 para 3.117.911 bytes; o total de C++ caiu de
  363.748.453 para 166.824.472 bytes entre os comparativos v2 e v4. A quantidade
  de funções não representa compatibilidade e não deve virar meta de redução.
- O probe de Metal Slug decodificou MPEG, mas apresentou tela preta. Captura
  limitada do GIF demonstrou um cabeçalho DMA dentro do payload: os appends HLE
  atualizavam QWC cedo, e o fechamento convidado somava a contagem outra vez.
  Libgif agora fecha a contagem no terminate; o contrato live de VIF foi
  preservado. Duas regressões novas reproduziram o erro antes da correção.
  Recibo do delta: `build/beta-v01-20261002/metal-slug-runtime-delta-v1.json`.
  O runner de laboratório reutiliza 10.259 objetos e foi relinkado em 16,79 s;
  o pacote e o snapshot v4 anteriores permanecem intactos. O probe
  `metal-slug-gif-fixed-probe-v1` mostrou a abertura animada, com transferência
  GIF de 1.392.832 bytes onde havia somente 96 bytes incompletos. A intro tem
  progresso, mas cadência lenta; o menu ainda não foi alcançado/aprovado.
  O probe longo `metal-slug-long-probe-v1` terminou em 600,16 segundos: 30
  frames, abertura seguida de tela preta, nenhum menu aprovado. Os 594,944
  segundos de PCM do sink isolado tiveram somente zeros. Recibo:
  `build/beta-v01-20261002/metal-slug-long-probe-v1.json`.
  Isso classifica áudio como pendente; não confundir dispositivo aberto com som.
- Um perfil parcial do scheduler registrou 4.864.089 publicações de snapshot
  e 14.586.871 consultas à fila em 24,82 segundos. A variante de diagnóstico
  precisou finalizar gprof antes do `_Exit` do runner; não alterar a saída de
  produção por isso. A tentativa de reduzir alocações passou 491 testes, mas
  não melhorou o progresso no replay comparativo e foi descartada. Recibos:
  `metal-slug-scheduler-profile-v3.json` e
  `metal-slug-scheduler-comparison-v1.json`, no diretório de recibos acima.
  O trace de funções está com escrita desativada pelo helper, embora as
  chamadas compiladas continuem presentes. Não atribuir o atraso a escrita
  de logs que não aconteceu.
- A consulta nativa de fim de MPEG lê o primeiro word do work apontado pelo
  handle em `+0x40`. GetPicture HLE agora publica esse estado com o mesmo
  critério de EOF, fila drenada e prazo do último frame da consulta HLE.
  Duas regressões cobrem essa interoperabilidade e a duração final; 493 testes
  passaram. Recibo do delta: `metal-slug-runtime-delta-v3.json`. O v2 foi a
  tentativa descartada de scheduler; não retomá-lo. O replay delimitado de
  450 segundos está em `metal-slug-long-probe-v2`; conferir seu resultado
  antes de afirmar que a correção destravou o menu.

Novo build completo: `/var/tmp/ps2-native-beta-20261002/metal-slug-4-v4/`, fontes
`source-v4`, recibo `build/beta-v01-20261002/source-snapshot-v4.json`. Os builds
anteriores foram interrompidos para redução e estão preservados. O v4 completo
foi retomado com os mesmos insumos/objetos; integridade dos 95 arquivos passou.
Recibo: `build/beta-v01-20261002/metal-slug-build-resume.json`. Nenhuma nova ISO
tem menu aprovado ainda. O runner original não contém a correção libgif posterior.

`/home` estava com 4,9 GiB livres e `/var/tmp` com 4,1 GiB. Fazer as novas
conversões em `/var/tmp/ps2-native-beta-20261002/`, com snapshot de fontes e até
quatro compiladores simultâneos (já exercitados na retomada), preservando os
37 GiB de evidência/build existentes. Conferir espaço/RAM antes de cada título.
Não usar `/tmp` ou `/dev/shm` para grandes builds: são memória compartilhada
com o desktop. Manter recibos pequenos e referências desses jobs no checkout.

O objetivo anterior de campanha é posterior a este beta. A recuperação EE
mais recente já está compilada com 18.193 famílias/41 capturas; não refazê-la
nem repetir load de capítulo 3D sem relação com um bloqueio de menu.

Regras de projeto: `AGENTS.md`. Histórico: `docs/RETOMADA_MAHORAGA.md` e seções
pertinentes de `README_GERAL_COMPLETO.md`. O goal permanece ativo até existir
evidência para todos os requisitos, incluindo a meta de compatibilidade.
