# PS2Native

Ferramentas experimentais de recompilação de PlayStation 2 com recuperação automática de código EE durante a conversão offline.

O projeto parte do [PS2Recomp](https://github.com/ran-j/PS2Recomp), mantém a [GPLv3](LICENSE) e desenvolve inspeção da ISO, análise MIPS, geração C++20, empacotamento e captura → compilação → relink → replay. A memória privada reutiliza casos por identidade da ISO, gerador e fontes.

**Estado real:** há testes próprios de recuperação e reuso; os experimentos comerciais ainda têm **0/5 menus qualificados**. Menus navegáveis, áudio fiel, controles, cadência e natividade integral continuam pendentes. Não há garantia de qualquer ISO, 90% de compatibilidade ou 60 FPS sustentados.

O [guia para avaliação](docs/REVIEWER_GUIDE.md) mostra como verificar o trabalho sem jogos ou BIOS. Consulte o [README principal](README.md), [roteiro](ROADMAP.md), [contribuições](CONTRIBUTING.md) e [proveniência](docs/PROVENANCE.md).

ISOs, ELFs/IRX extraídos, assets, capturas RAM/VRAM e código derivado de jogos ficam privados. Os testes públicos usam instruções e dados próprios do projeto.
