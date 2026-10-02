# PS2Native — estado de execução no PC

Atualização: 2026-10-02T10:02:40.505934-03:00.

Plano vigente: [README.md](../README.md). Acervo completo: [README_GERAL_COMPLETO.md](../README_GERAL_COMPLETO.md), snapshot documental das 09:34. Esta atualização registra execução posterior ao acervo.

## Avanço desta etapa

- VIF passa a enviar bytes GIF originais quando o backend mantém stream contínua; o receptor CPU conserva o contrato legado.
- Reset VIF feito pelo convidado limpa o parser e preserva a ligação ao receptor host. A regressão falhou antes da correção e passou depois.
- Ponte GS exercitada na RX 6600 com registro TLS Granite e readback por worker host.
- Runner atualizado relinkado em **11.359 s**, sem recompilar objetos originais do jogo.
- Menus e cena 3D do livro/fotos reapareceram no percurso GPU; permanecem defeitos de imagem/geometria.

## Resultados delimitados

| Verificação | Resultado |
|---|---|
| C++ geral | 487/487; 4,675 s |
| Snapshot de dispositivos | 14/14 |
| Captura VIF | 16/16 |
| VIF bytes originais | GREEN; contrato legado falha no controle negativo |
| VIF reset do convidado | RED → GREEN, receptor host preservado |
| Device/frontend GS físicos | GREEN na AMD Radeon RX 6600 (RADV NAVI23) |
| Percurso do jogo | 361.459 s; timeout delimitado, sem aprovação de campanha |
| Screenshots | 156 arquivos; 157 tentativas |
| Capturas de cena | 4 snapshots RAM/contexto/GS/VU, sem restore completo |
| Novo miss EE nesse percurso | 0 |
| Diagnóstico de thread Granite | 0 ocorrências no log novo |
| Cards/processos | Cinco cards restaurados por SHA-256; runner/Xvfb próprios encerrados |

O último contador acumulado registra 1.325.299 primitivas GPU, 50.856 passes e 18.186.502.144 bytes de readback. Esses números confirmam trabalho gráfico; não são FPS nem medida de fidelidade.

## Perfil e limites

- Linux x86-64 experimental: EE concreto/famílias e biblioteca IOP nativa reutilizada; VU ainda diagnóstico.
- `raster_backend=parallel-vulkan`; `present_backend=opengl-readback`. Apresentação Vulkan direta e redução de readback integral permanecem pendentes.
- O classificador baseado nas antigas imagens CPU manteve `logos` mesmo nos menus e na cena. Navegação adicional Left+X e Start foi enviada pelo helper ao display próprio; isso não demonstra conversão automática por jogo.
- A tentativa tardia de W falhou porque o runner já havia encerrado; D não foi enviado. Labels do roteiro como `after-w-input` não comprovam aplicação do comando.
- Nenhuma resposta causal de personagem/câmera, áudio audível, save/reload, 60 FPS, campanha, pacote final nativo ou Android foi aprovada.
- M6, M7, M8 e M9 seguem abertos. O snapshot CTest 78/78 anterior não substitui os testes por revisão descritos aqui.

## Evidência e reprodução

Job: `build/lab/gs-stream-corrected-1790945198323964871`.

- `progress.json`, `inputs.json`, `source-manifest.json`: escopo, revisão e identidades.
- `vif-reset-red.json` / `.log`, `vif-reset-green.json` / `.log`: regressão de reset.
- `cpp-tests.json`, `device-snapshots-after-reset.json`, `vif-captures-after-reset.json`, `gpu-device.json`, `gpu-frontend.json`: testes exercitados.
- `relink-reset.json`: comando, objetos substituídos, flags e identidade do runner.
- `game-run/launch-record.json`, `headless-test.json`, `cleanup.json`, inputs e capturas: percurso delimitado e restauração.

Runner SHA-256: `4f26fd9cc841b46ed114e24ea284f32fe786c692d9a72551ec80fe9ba21a3ffd`.

O wrapper usa um destino `game-run` novo e recusa repetir no mesmo diretório. Uma próxima execução deve criar outro job e conservar os registros anteriores. ISOs, assets, RAM/VRAM, capturas e binários continuam locais em build; não fazem parte do pacote fonte do projeto.

## Próxima ação

Classificar a cena 3D e reduzir a primeira divergência de geometria/estado com as quatro capturas existentes. Comparar VU/VIF/GIF e efeitos nativos antes de ampliar o catálogo, pois não houve novo miss EE. Fazer o próximo experimento de controles dentro do orçamento, com baseline e inputs registrados; a fixture atual precisa de navegação adequada ao renderer. Depois da imagem e controle, concluir apresentação Vulkan, VU AOT e auditoria do pacote integrado.
