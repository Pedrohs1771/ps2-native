# Referências verificadas para bloqueios reais

Consultadas em 02/10/2026; metadados, commits e READMEs foram congelados localmente
em `build/retomada-20261002/references.json`. Esta seleção não certifica projetos
externos nem declara uma busca exaustiva em todo o GitHub.

| Referência primária | Uso no PS2Native | Limite |
|---|---|---|
| [PS2Recomp](https://github.com/ran-j/PS2Recomp) | Base R5900/C++, entradas, integração do runtime. | O upstream se declara experimental e tem configuração/stubs por título. |
| [PCSX2](https://github.com/PCSX2/pcsx2) | Referência independente de CPU/VU, áudio e dispositivos. | Usar na conversão/testes; não chamar seu intérprete de AOT do pacote. |
| [paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs) | Raster Vulkan e diagnóstico por GS dump/RenderDoc. | Já integrado experimentalmente; não promete igualdade bit a bit em interpolação. |
| [OpenGOAL/Jak](https://github.com/open-goal/jak-project) | Exemplo real PS2 de compilador, runtime e extração separados. | Especializado em GOAL/Jak, com trabalho manual de tipos; não é conversor geral. |
| [N64Recomp](https://github.com/N64Recomp/N64Recomp) | Tradução literal, overlays, relocações e lookup de funções. | Metadados e runtime continuam necessários; a ISA/hardware não são PS2. |
| [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) | Separação tradutor/runtime para ports. | Referência arquitetural, não backend PS2. |
| [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) e [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) | Distinguir geração de código de integração e distribuição de port. | XenonRecomp não fornece o runtime; PPC/Xbox 360 difere de R5900. |
| [BT3-Recomp](https://github.com/z3xox/BT3-Recomp) | Caso PS2 de build a partir da ISO, IOP e empacotamento. | Suporte declarado para ISO USA específica; não executado nesta auditoria. |
| [DC2-PS2RECOMP](https://github.com/Red-tv141/DC2-PS2RECOMP) | Investigar diagnóstico VU/GS e resultados de uma engine concreta. | Não presumir campanha aprovada; licença própria AGPL-3.0. |
| [PS2Recomp WoS Findings](https://github.com/InitialDad/PS2Recomp-WoS-Findings) | Guardar hipóteses refutadas e distinguir tiers de evidência. | O próprio projeto registra alegações antigas incorretas. |
| [PS2SDK](https://github.com/ps2dev/ps2sdk) e [ps2autotests](https://github.com/unknownbrackets/ps2autotests) | Contratos de bibliotecas e expectativas de hardware em programas pequenos. | SDK homebrew não prova todos os SDKs comerciais; selecionar o teste pertinente. |
| [RecompOne](https://github.com/BlackLabelHQ/RecompOne) | Outra implementação MIPS/contexto/runtime, em C#. | PS1; criação de port ainda exige integração. |
| [psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp) | Captura/compilação/cache de overlays e recibos por conteúdo. | Declara fallback interpretado e compilação durante execução; licença PolyForm Noncommercial no snapshot. Não transplantar esse caminho como AOT estrito. |
| [Alive2](https://github.com/AliveToolkit/alive2) | Validação de transformações escalares LLVM com contraexemplos SMT. | Não é prova pronta de R5900/VU/MMIO; não reescrever o backend neste beta. |
| [ps2recomp-workbench](https://github.com/phmdacosta/ps2recomp-workbench) | Índice de ferramentas, checkpoints e resultados negativos. | O README distingue scripts de engenharia reversa automática; licença não identificada. |

Aplicar primeiro: diagnóstico do primeiro bloqueio de menu, regressão sintética,
comparação independente no domínio suportado e build incremental. Técnicas de
síntese guiada por contraexemplos e validação de tradução são propostas de
engenharia para o ciclo de adaptação, não uma alegação de invenção comprovada.
Uma engine só pode reutilizar capacidades cujos contratos/assinaturas combinam;
mesmo assim cada ISO precisa de aceitação de menu, áudio e controles.

Antes de incorporar código, ler a licença e os avisos do arquivo específico.
O PS2Native conserva sua base GPLv3; o paraLLEl-GS identifica LGPLv3+, e há
referências acima com condições diferentes. O snapshot local de documentação
não é incluído automaticamente numa distribuição do projeto.

Para instruções persistentes do agente, a [documentação oficial de AGENTS.md](https://learn.chatgpt.com/docs/agent-configuration/agents-md)
explica descoberta e precedência. O arquivo curto na raiz organiza a retomada;
não é necessário alterar o modelo nem carregar todos os anexos a cada sessão.
