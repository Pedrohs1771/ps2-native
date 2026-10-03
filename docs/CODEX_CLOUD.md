# PS2Native no Codex Cloud

O corpus local tem cinco imagens comerciais. Seus nomes originais, tamanhos e
SHA-256 estão em [`../test-data/corpus-manifest.json`](../test-data/corpus-manifest.json).
Os caminhos são relativos à raiz do checkout. Esse manifesto registra os metadados
das imagens completas. Para permitir que o agente no Cloud trabalhe sem depender de
downloads externos ou montagem de ISOs de gigabytes, os pacotes leves de teste contendo
apenas os ELFs e módulos de sistema estão disponíveis diretamente no repositório em
`fixtures/` (ex.: `fixtures/metal_slug_4/`), totalizando menos de 20 MB e servindo
como fixtures de validação do compilador e do kernel.

Para a análise direta por ELF já configurada e o workflow de execução, veja
[ELF_CONTINUACAO_CLOUD.md](ELF_CONTINUACAO_CLOUD.md) e [fixtures/README.md](../fixtures/README.md).

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
