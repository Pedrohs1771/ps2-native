# PS2Native no Codex Cloud

O corpus local tem cinco imagens comerciais. Seus nomes originais, tamanhos e
SHA-256 estão em [`../test-data/corpus-manifest.json`](../test-data/corpus-manifest.json).
Os caminhos são relativos à raiz do checkout. Esse manifesto é **somente
metadados**: os bytes das ISOs não foram enviados ao GitHub ou ao Cloud.

O diretório `test-data/isos/` separa insumos privados de fontes e builds. Os
arquivos reais precisam ser fornecidos legalmente ao ambiente onde os testes
rodam. Não presumir que caminhos `/home/pedrohs/...` ou recibos em `build/`
existem no Cloud. Não usar o manifesto como prova de disponibilidade das imagens.

As cinco imagens somam 3.089.043.456 bytes, aproximadamente 2,88 GiB, antes de
extração e compilação. Todas excedem o limite de 100 MiB por arquivo do GitHub
com Git comum. Para conteúdo próprio ou com licença de redistribuição, Git LFS
é uma opção; seu checkout precisa baixar os objetos reais, não apenas os
ponteiros. Isso não garante espaço suficiente para builds ou acesso no Cloud.
[Limites do GitHub](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-large-files-on-github),
[Git LFS](https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage).

## Preparar e continuar

Selecione `Pedrohs1771/ps2-native` e a revisão desejada no ambiente Cloud.
Use as dependências e os targets mínimos de
[`VPS_CONTINUACAO.md`](VPS_CONTINUACAO.md), verificando os recursos disponíveis.
No Codex Cloud atual, a configuração permite preparar dependências e assets
com um Install script; publicar o ambiente captura o filesystem preparado.
Mudanças do ambiente devem ser republicadas e verificadas numa tarefa nova.
[Documentação oficial do Codex Cloud](https://learn.chatgpt.com/docs/environments/cloud-environments).

Com ISOs efetivamente disponíveis nesse diretório, execute na raiz do checkout:

```sh
python3 -m tools.ps2native.corpus_batch plan --iso-dir test-data/isos
```

Compare `iso_sha256` e `iso_bytes` com o manifesto. Caminhos absolutos no recibo
correspondem ao ambiente atual. O inventário apenas identifica os arquivos;
conversão e aprovação de menus são etapas distintas.

Sem ISOs, continue pelas fixtures próprias do projeto:

```sh
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_autoadaptation_execution
```

A segunda suíte requer o build descrito no guia da VPS. As fixtures não
qualificam compatibilidade comercial. O estado publicado continua experimental,
com 0/5 menus qualificados; siga `PROMPT_CONTINUAR_CODEX.txt` usando a raiz
do checkout atual.
