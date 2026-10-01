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

Ainda faltam: frontend IRX com identidade completa do módulo, binding de
relocations para diferentes bases, descoberta de código latente, snapshots
canônicos independentes da ABI, serviços e tempo qualificados, reset/substituição
de módulos e cobertura dos módulos comerciais. O banco absoluto desta etapa
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
