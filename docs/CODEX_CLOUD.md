# PS2Native no Codex Cloud

Use o repositório original `Pedrohs1771/ps2-native` e o [guia de avaliação](REVIEWER_GUIDE.md) para testes próprios sem jogos ou BIOS.

```sh
python3 -m unittest discover -s tools/ps2native/tests
python3 -m unittest lab.tests.test_ee_data_families lab.tests.test_ee_family_catalog lab.tests.test_prepare_ee_family_batch lab.tests.test_ee_context_inventory
```

CMake deve estar no PATH. A demonstração EE exige o build indicado no guia; `python3 tools/check_native_demo.py` recusa testes pulados.

O clone público não fornece corpus comercial nem extrações de ELFs/IRX. A antiga release de corpus não integra a distribuição pública. Metadados em `test-data/` documentam experimentos privados; não contêm os jogos nem autorizam redistribuição.

Mantenha insumos privados autorizados fora do Git e use um diretório novo por rodada. Não busque jogos externos. Build, replay, menu, áudio, input e natividade são resultados distintos. Estado atual: 0/5 menus qualificados.
