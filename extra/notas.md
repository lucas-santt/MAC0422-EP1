## Mutex para cada processo

Não é necessário de um mutex para cada processo pois o fluxo do
código do escalonador não permite que duas threads estejam responsáveis
pelo mesmo processo ao mesmo tempo.

> A primeira vista, as operações que envolvem ps não são seções críticas 
            pois cada processo é gerenciado por uma thread.
        Porém, como a thread possui cancelation type deferred, pode demorar um
            tempo para a thread ser cancelada e, no caso específico onde a fila está vazia,
            outra thread ficar responsável pelo processo e alterar qualquer valor dentro de ps.
        Portanto, é necessário proteger com o mutex do próprio ps.

Este comentário está **errado**

Como o processo só volta da fila após sofrer uma preempção quando
pthreads_tryjoin_np retorna 0, ele só retorna após a thread estiver
completamente encerrada (contando a função de cleanup: onThreadPreempted).

Portanto, não é necessário um mutex para cada proceso

## Processadores dos PC's

### Computador A
11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz, 8CPUS

#### Computador B
Intel(R) Xeon(R) CPU E5-2670 0 @ 2.60GHz, 32 CPUS