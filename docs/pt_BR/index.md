# Documentação do SoftGRE

SoftGRE é um daemon Linux que descobre remotos GRE IPv4 com Ethernet e provisiona
interfaces gretap locais. Interfaces VLAN conectam cada remoto às bridges Linux
configuradas. O kernel encaminha o tráfego; o daemon observa pacotes para descobrir
remotos e endereços MAC de origem.

## Primeiros passos

Use um host Linux com IPv4, bridge, 802.1Q, GRE/gretap, AF_PACKET e firewall de bridge. A
compilação exige compilador C11, Make, Autoconf, Automake, pkg-config e headers de
desenvolvimento da GLib. Em execução, são necessários GLib, ebtables e o suporte
correspondente do kernel.

```sh
sudo apt-get update
sudo apt-get install build-essential autoconf automake pkg-config libglib2.0-dev ebtables iptables iproute2
git clone https://github.com/jpereira/softgre.git
cd softgre
./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var
make -j2
make check
sudo make install
sudoedit /etc/softgred/softgred.conf
sudo /usr/sbin/softgred -f
```

Selecione a interface cujo endereço IPv4 termina o GRE. Configure o mapeamento de VLANs
para bridges antes de iniciar. Use `bridge-force=true` para criar bridges ausentes ou
crie-as com `ip link add br-gre type bridge`. Bridges existentes são ativadas e
preservadas ao encerrar. O daemon exige root. Execute em primeiro plano com um gerenciador
de serviços para coletar stderr.

## Arquitetura

A captura usa um socket Linux `AF_PACKET`, vinculado à interface configurada, sem modo
promíscuo. Um filtro BPF clássico seleciona GRE IPv4 no kernel, incluindo até duas tags
VLAN externas. O parser verifica limites de Ethernet, tamanhos e opções IPv4,
fragmentação, flags e protocolo GRE, VLAN interna e MAC de origem. Somente pacotes
destinados ao endereço IPv4 selecionado da interface de captura podem provisionar túneis.
Pacotes de saída são ignorados.

Para cada remoto novo, mensagens nativas `NETLINK_ROUTE` criam um gretap sem chave e uma
interface VLAN por mapeamento configurado. As mensagens têm confirmação do kernel,
atributos limitados e timeout de recepção de três segundos. Tabelas hash GLib indexam IPv4
remoto e MAC. Um único loop `poll` controla captura, clientes da API e sinais via
`signalfd`, evitando acesso concorrente a objetos removidos. A captura processa até 64
pacotes por lote e a API aceita até 16 conexões por iteração. A criação de interfaces e
regras para novos remotos ou MACs é síncrona.

Bridges e gretap do kernel encaminham o tráfego estabelecido. Pacotes repetidos de MACs
conhecidos usam consultas hash, sem executar comandos ou recriar interfaces. O ebtables
recebe vetores de argumentos explícitos, sem comandos de shell. Cada comando externo tem
limite de três segundos.

## Configuração

O arquivo padrão é `/etc/softgred/softgred.conf` ao compilar com `--sysconfdir=/etc`. Uma
instalação com prefixo padrão `/usr/local` usa `/usr/local/etc/softgred/softgred.conf`.

```ini
[global]
interface = eth0
tunnel-prefix = sgre
bridge-map = 1000@br-radius, 10@br-hotspot, 11@br-radius
bridge-force = true
pid-file = /var/run/softgred.pid

[service]
bind-in = lo
port = 8888
max-listen = 10

[debug]
enable = false
print-time = true
```

| Chave | Significado / padrão |
| --- | --- |
| `global.interface` | Interface obrigatória de captura, com IPv4 local utilizável. |
| `global.tunnel-prefix` | Prefixo das interfaces, padrão `sgre`, máximo de seis caracteres. |
| `global.bridge-map` | Lista obrigatória de `VLAN@bridge` separada por vírgulas, até dez entradas. |
| `global.bridge-force` | Cria bridges ausentes, padrão `false`. |
| `global.pid-file` | Arquivo PID bloqueado, padrão no diretório PID definido na compilação. |
| `service.bind-in` | Endereço IPv4 da interface; padrão `lo`; `any` vincula todos. |
| `service.port` | Porta TCP, `1..65535`, padrão `8888`. |
| `service.max-listen` | Clientes simultâneos, `1..128`, padrão `10`. |
| `debug.enable` | Ativa logs de diagnóstico no nível um, padrão `false`. |
| `debug.print-time` | Horário nos logs, padrão `false`; também aceita `debug.time`. |

VLANs válidas são `1..4094`, sem repetição no mapeamento. Nomes de interfaces usam letras
ASCII, dígitos, sublinhados, pontos ou hífens e têm até 15 caracteres. O limite de seis
caracteres do prefixo reserva espaço para o slot do túnel e o sufixo VLAN de quatro
dígitos. Cada destino deve ser uma bridge Linux existente ou criada com
`bridge-force=true`. Várias VLANs podem usar a mesma bridge, unindo deliberadamente esses
domínios de broadcast. Entradas vazias, números malformados e VLANs repetidas invalidam a
configuração. Grupos e chaves opcionais podem ser omitidos.

Strings e booleanos são armazenados com seus tipos C corretos. Booleanos aceitam `true`,
`false`, `1` e `0`. Logs vão para stderr. Repetir `-d` seleciona níveis de diagnóstico até
quatro. A configuração é carregada na inicialização; reinicie o daemon para aplicar
alterações. Chaves desconhecidas são ignoradas.

## CLI e ciclo de vida

```text
softgred [-f] [-c FILE] [-p FILE] [-d...] [-t] [-h] [-v]
```

| Opção | Comportamento |
| --- | --- |
| `-f`, `--foreground` | Primeiro plano, recomendado para systemd ou containers. |
| `-c`, `--config-file` | Caminho da configuração. |
| `-p`, `--pid-file` | Substitui o caminho PID da configuração. |
| `-d`, `--debug` | Aumenta o nível de logs até quatro. |
| `-t`, `--debug-time` | Adiciona horário aos logs. |
| `-h`, `--help` | Ajuda sem exigir root ou carregar configuração. |
| `-v`, `--version` | Versão sem exigir root ou carregar configuração. |

O arquivo PID usa um bloqueio exclusivo mantido durante a vida do daemon e rejeita links
simbólicos. A inicialização em segundo plano só informa sucesso quando captura, API,
bridges e firewall estão prontos. Falhas de inicialização ou captura causam saída com
erro. `SIGINT` e `SIGTERM` encerram o daemon, removem túneis e chains próprios e apagam o
PID. `SIGUSR1` remove todos os túneis provisionados, permissões de MAC e índices; a
descoberta continua. `SIGUSR2` escreve contadores de túneis, MACs, pacotes, pacotes
ignorados e falhas em stderr.

Interfaces e regras de firewall sem relação com o daemon são preservadas. Bridges de
destino criadas dinamicamente permanecem ao encerrar. Slots são reutilizados após limpeza
bem-sucedida; nomes já presentes no namespace são pulados. Não há expiração automática por
inatividade nem mobilidade implícita de MAC. Um MAC pertence a um único remoto e sua
permissão cobre as VLANs configuradas naquele túnel. Reiniciar ou enviar `SIGUSR1` limpa o
vínculo antes de aprender o MAC em outro remoto.

## API TCP de consulta

A API escuta em loopback por padrão, sem autenticação nem criptografia. Mantenha-a em
loopback ou em uma interface de gerência confiável e controle o acesso pelo firewall.
`any` expõe a API em todas as interfaces IPv4. Compile com `--disable-service` para omitir
o listener TCP.

Cada conexão aceita um comando terminado por LF, CRLF ou fechamento da metade de escrita
TCP. O servidor responde e fecha a conexão. Envie uma nova linha: leituras parciais são
acumuladas. Os comandos diferenciam maiúsculas e minúsculas e exigem nome exato. Cada
cliente tem prazo total de cinco segundos, requisição de até 255 bytes e resposta de até 1
MiB. Conexões excedentes fecham.

```sh
printf 'STUN\n' | nc 127.0.0.1 8888
printf 'LMIP 192.0.2.2\n' | nc 127.0.0.1 8888
printf 'GTMC 02:00:00:00:00:42\n' | nc 127.0.0.1 8888
```

| Comando | Resposta |
| --- | --- |
| `HELP` | Lista legível dos comandos. |
| `VERS` | `RESULT: OK`, `BODY: versão`. |
| `QUIT` | Fecha sem corpo. |
| `STUN` | `RESULT: OK`, `BODY: remoto@interface;...`, ou `RESULT: NULL`. |
| `LMIP IPv4` | `RESULT: OK`, `BODY: mac;mac;...`, ou `RESULT: NOTFOUND`. |
| `GTMC MAC` | `RESULT: OK`, `BODY: remoto@interface`, ou `RESULT: NOTFOUND`. |
| `STAT` | `RESULT: OK`, `BODY: SO;versão;arquitetura;uptime;versão_daemon;início`. |

Respostas bem-sucedidas incluem `RESULT: OK` e uma linha `BODY:`. Comandos ou argumentos
inválidos retornam `RESULT: INVALID`. Consultas MAC aceitam seis octetos hexadecimais
separados por dois pontos, inclusive letras maiúsculas. LMIP ordena MACs
lexicograficamente; a ordem de STUN é indefinida. Uptime usa segundos; `início` é o
timestamp Unix do carregamento da configuração. A interface PHP opcional encaminha esses
comandos de consulta e oferece um dashboard simples sem dependências de frontend.

## Dashboard PHP opcional

Compile com `--enable-web` para instalar o dashboard de consulta em
`/usr/share/softgred/webinterface` com prefixo `/usr`. Sirva esse diretório com PHP 8.1 ou
mais recente em uma rede de gerência confiável. Os assets são estáticos e não exigem build
Node. O adaptador usa as variáveis `SOFTGRE_API_HOST` e `SOFTGRE_API_PORT`, com padrão
`127.0.0.1:8888`. O host deve ser um IPv4 literal e a porta deve estar em `1..65535`.

```sh
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --enable-web
make -j2
sudo make install
php -S 127.0.0.1:8080 -t /usr/share/softgred/webinterface
```

O servidor de desenvolvimento PHP serve para preview local. Em produção, configure PHP-FPM
com seu servidor web e restrinja o acesso. O adaptador aceita apenas GET, valida
argumentos IP/MAC e envia comandos terminados por nova linha. O timeout de conexão é dois
segundos, o prazo de resposta é cinco segundos e as respostas têm limite de 1 MiB. O JSON
contém os campos `status` e `body`.

| Endpoint | Comando TCP |
| --- | --- |
| `api.php?action=status` | `STAT` |
| `api.php?action=tunnels` | `STUN` |
| `api.php?action=ues&arg=192.0.2.2` | `LMIP 192.0.2.2` |
| `api.php?action=cpe&arg=02:00:00:00:00:42` | `GTMC 02:00:00:00:00:42` |
| `api.php?action=version` | `VERS` |
| `api.php?action=help` | `HELP` |

Requisições inválidas retornam HTTP 400, métodos não suportados 405, falhas do backend 502
e timeouts de resposta 504. O dashboard renderiza texto, nunca HTML. Dashboard e adaptador
não fornecem autenticação. Os builds padrão do daemon, pacote e container não instalam o
dashboard. `--enable-web` exige a API TCP habilitada. Execute `python3 tests/test_web.py`
com PHP instalado para testar o adaptador com backend simulado e conexões HTTP reais.

## Firewall e modelo de confiança

SoftGRE controla as chains ebtables `SOFTGRE_IN` e `SOFTGRE_OUT`. Insere saltos em FORWARD
limitados às interfaces VLAN de cada túnel próprio, permite MACs aprendidos de
origem/destino e descarta os demais pacotes correspondentes. Broadcast/multicast de saída
é permitido para ARP, DHCP e descoberta de vizinhos. Nunca esvazia FORWARD nem altera sua
política global. A inicialização rejeita chains próprias preexistentes para evitar assumir
outra instância ou estado abandonado. Cada MAC aprendido instala uma regra de entrada e
outra de saída. A limpeza revoga permissões e remove saltos antes de apagar as chains.
Valores de pacotes nunca entram em um shell.

GRE não fornece autenticação. Observar um MAC é descoberta, não verificação de identidade.
Use proteção contra falsificação de origem e restrinja o protocolo IP 47 a remotos
confiáveis ou a um transporte protegido. VLAN desconhecida, destino alheio, origem IPv4
zero/multicast/loopback, MAC inválido, IPv4 fragmentado, GRE com chave, campos opcionais
ou versão diferente de zero não provisionam. Os payloads aceitos são Ethernet com VLAN em
GRE de protocolo `0x6558`; o kernel encaminha ARP, IPv4 e IPv6 internos. IPv6 externo,
Ethernet interna sem VLAN, QinQ interno, GRE com chave e checksums GRE não são suportados.
O parser não autentica nem valida o payload IP interno. O parser controla a descoberta; o
tráfego estabelecido é encaminhado pelo kernel e controlado pelas regras do firewall de
bridge.

Os limites são 4096 túneis e 1024 MACs por túnel. A captura solicita buffer de 4 MiB,
sujeito ao limite do kernel. Falhas de criação suspendem globalmente trabalho novo por um
segundo para conter tentativas repetidas. Falhas de remoção no kernel mantêm os registros
para nova tentativa. Uma limpeza completa incompleta pausa a descoberta até um SIGUSR1
bem-sucedido. A limpeza esvazia as chains de MAC próprias em lote, sem executar um
processo para cada MAC removido. Após crash ou encerramento forçado, inspecione interfaces
e chains próprios antes de reiniciar; preserve o restante do firewall.

## Implantação Linux

A unit `share/softgred.service` executa em primeiro plano com `CAP_NET_ADMIN` e
`CAP_NET_RAW`, coleta logs no journal e inicia após a rede ficar online. Instale-a
manualmente com `make install`; o pacote Debian instala sua unit systemd automaticamente.

```sh
sudo install -m 644 share/softgred.service /etc/systemd/system/softgred.service
sudo systemctl daemon-reload
sudo systemctl enable --now softgred
sudo journalctl -u softgred -f
```

A unit espera `/usr/sbin/softgred` e `/etc/softgred/softgred.conf`. Configure uma
dependência do seu gerenciador de rede caso `network-online.target` não garanta que a
interface esteja pronta. Somente uma instância deve controlar um namespace de rede.

```sh
docker build -t softgre .
docker run --rm --network host --cap-add NET_ADMIN --cap-add NET_RAW \
  -v /absolute/path/softgred.conf:/etc/softgred/softgred.conf:ro softgre
```

O exemplo com rede do host configura a rede do host Linux. Para experimentar, use um
namespace descartável. O kernel do host precisa suportar GRE, VLANs, bridges e ebtables; o
container não fornece módulos ausentes. A imagem de produção contém daemon, GLib e
ebtables e executa em primeiro plano. Compile e execute Docker em Linux; um cliente macOS
pode usar um contexto Docker remoto.

## Releases e pacotes Debian/Ubuntu

A versão 2.0.1 está em `VERSION`. Autoconf lê esse arquivo; `bump.sh` também atualiza o
changelog Debian e as referências de versão nos guias bilíngues. Baixe os pacotes amd64
específicos da distribuição no GitHub Release:

[Baixar SoftGRE 2.0.1](https://github.com/jpereira/softgre/releases/tag/v2.0.1)

| Distribuição | Asset do release |
| --- | --- |
| Debian 12 | `softgred_2.0.1-1+debian12_amd64.deb` |
| Debian 13 | `softgred_2.0.1-1+debian13_amd64.deb` |
| Ubuntu 22.04 | `softgred_2.0.1-1+ubuntu22.04_amd64.deb` |
| Ubuntu 24.04 | `softgred_2.0.1-1+ubuntu24.04_amd64.deb` |

Baixe o pacote da sua distribuição e `SHA256SUMS`, confira o hash e instale com `sudo apt
install ./softgred_2.0.1-1+debian12_amd64.deb`, ajustando o nome ao seu sistema. O pacote
instala daemon, configuração, guias e unit systemd, sem iniciar o serviço automaticamente.
Configure o daemon e ative-o com `sudo systemctl enable --now softgred`.

```sh
./bump.sh --check
./bump.sh patch
```

O argumento aceita `major`, `minor`, `patch` (padrão) ou uma versão explícita. Cada bump
deve aumentar a versão e usar `MAJOR.MINOR.PATCH` sem zeros à esquerda. O bump atualiza
metadados, sem criar commit, tag ou push. Faça o commit e crie/envie uma tag `vX.Y.Z`
correspondente. O workflow valida o código da tag com a CI Linux completa, compila e testa
cada .deb na imagem da sua distribuição e publica pacotes e `SHA256SUMS` no release
somente após todos os jobs passarem. Uma execução manual recebe uma tag existente,
verifica a versão e substitui assets correspondentes. O website publica a documentação do
branch padrão.

```sh
release_version=$(cat VERSION)
git tag "v$release_version"
git push origin master "v$release_version"
# Build local dentro de um container Linux descartável da distribuição correta:
scripts/build-deb.sh debian12 /out
```

Os nomes dos pacotes incluem a distribuição para evitar colisões. A arquitetura do pacote
é a do host de build; o workflow fornecido compila amd64. Para outra arquitetura, compile
nativamente. O workflow não publica pacotes de símbolos de debug. Downloads e checksums
são assets públicos do GitHub e não exigem repositório de pacotes.

## Testes e desempenho

`make check` verifica limites do parser, todas as truncagens de fixtures válidas, opções
IPv4, VLANs externas, formatos GRE inválidos, entradas malformadas determinísticas,
configuração tipada, primeiro mapeamento de bridge, índices hash, capacidade de clientes,
rollback, bloqueios PID, comandos da API, fragmentação TCP e desconexões. Os testes
simulam operações do kernel para provocar falhas de estado e usam sockets loopback reais
para a API. `test-netlink` verifica atributos codificados, aninhamento e limites de
mensagens.

```sh
sudo apt-get install clang python3
scripts/check-linux.sh
```

O script compila fora da árvore com GCC e Clang usando `-Werror`, executa ASan/UBSan,
verifica `--disable-service`, códigos de saída da CLI e instalação em diretório
temporário, e roda `make distcheck`. Os testes de integração exigem namespace Linux
descartável e privilégios root:

```sh
docker build -f Dockerfile.test -t softgre-test .
docker run --rm --cap-add NET_ADMIN --cap-add NET_RAW \
  -e SOFTGRE_TEST_NAMESPACE=isolated softgre-test \
  python3 tests/integration.py bin/softgred
```

A integração verifica interfaces gretap e VLAN reais, primeira bridge, captura BPF, 10000
pacotes repetidos sem duplicar regras, VLANs externas, framing da API, limpeza por sinais,
preservação do firewall alheio, bloqueio PID e falha de inicialização em segundo plano. O
tempo de injeção dos pacotes é uma medição reproduzível de smoke test, não um benchmark de
encaminhamento. A CI executa verificações de compiladores, sanitizers e distribuição, além
da integração isolada.

A descoberta em estado estável usa consultas IPv4/MAC com custo médio O(1). A criação
exige mensagens netlink e dois processos de firewall por MAC. Essas operações bloqueiam
temporariamente o loop principal, cada uma por até seu timeout. Em cenários de alta
rotatividade ou milhões de MACs, meça latência de criação, percurso das regras e perdas de
captura antes de dimensionar. AF_PACKET ainda copia pacotes para o espaço de usuário; não
há PACKET_MMAP nem captura zero-copy.

## Diagnóstico e site de documentação

| Sintoma | Verificação |
| --- | --- |
| Erro ao compilar | Instale headers GLib, compilador e Autotools; compile em Linux. |
| Falha na captura | Confira root, interface, IPv4 e capability NET_RAW. |
| Falha na bridge | Confirme que o destino é uma bridge Linux ou ative `bridge-force`. |
| Túneis não aparecem | Confira destino GRE, protocolo 47, payload TEB sem chave e VLAN mapeada. |
| MAC não migra | Limpe o vínculo aprendido com SIGUSR1 ou reinício antes de outro remoto. |
| Chain já existe | Inspecione chains SOFTGRE abandonadas e saltos; encerre outras instâncias. |
| Tráfego interrompido | Confira MTU do transporte, overhead GRE, firewall de bridge e rota remota. |
| API expira | Termine comandos com nova linha; confira interface, porta e firewall. |
| PID não bloqueia | Confira outra instância e permissões do diretório; evite links simbólicos. |

GRE consome MTU do transporte. O kernel seleciona a MTU do túnel; inspecione com `ip -d
link show` e configure transporte e clientes de acordo. SoftGRE não modifica rotas do
host, política global de encaminhamento IP nem configuração de módulos do kernel.

O site é gerado a partir dos guias Markdown pt_BR e en_US usando a biblioteca padrão do
Python, sem Node, assets de terceiros, rastreamento, fontes remotas ou JavaScript externo.

```sh
python3 scripts/build-docs.py
python3 tests/test_docs.py
python3 -m http.server 8000 --directory _site
```

O workflow Pages gera `_site` e publica via GitHub Actions no branch padrão. A CI valida a
documentação em pushes e pull requests. Atualize os dois guias quando o comportamento
mudar. O gerador aceita títulos, parágrafos, listas, tabelas, blocos de código, código
inline e links Markdown; HTML bruto é escapado. O código usa LGPL-3.0-or-later.
