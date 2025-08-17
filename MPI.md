# Resultados do **MPI** (Nearest Neighbor) — *pcb442.tsp*

## **Análise dos Resultados**

### **1. Qualidade da solução vs número de processos**

> Baseline (1 processo): **61984.05**

| Processos | Melhor Custo  | Processo vencedor | Melhoria vs 1 processo |
|----------:|--------------:|------------------:|-----------------------:|
| 1          | **61984.05**  | 0                 | Baseline                |
| 2          | 61749.42      | 1                 | 0.4%                    |
| 4          | 61738.92      | 3                 | 0.4%                    |
| 8          | 59737.76      | 6                 | 3.6%                    |
| 16         | 59737.76      | 6                 | 3.6%                    |
| 32         | 59737.76      | 6                 | 3.6%                    |
| 64         | 59737.76      | 6                 | 3.6%                    |
| 128        | **59685.52**  | 107               | 3.7%                    |

---

### **2. Padrão de Escalabilidade**

| Processos | Tempo (s)  | Speedup **real** (vs 1P) | Eficiência paralela **real** |
|----------:|-----------:|--------------------------:|------------------------------:|
| 1          | 0.000290   | 1.00×                      | 100.0%                         |
| 2          | **0.000233** | **1.24×**                  | **62.2%**                      |
| 4          | 0.000298   | 0.97×                      | 24.3%                          |
| 8          | 0.000482   | 0.60×                      | 7.5%                           |
| 16         | 0.000798   | 0.36×                      | 2.3%                           |
| 32         | 0.001901   | 0.15×                      | 0.5%                           |
| 64         | 0.004907   | 0.06×                      | 0.1%                           |
| 128        | 0.013478   | 0.02×                      | 0.0%                           |

> Observação: a ferramenta também reporta um “Speedup estimado” interno (abaixo), que **não coincide** com o **speedup real** calculado a partir do *wall clock* (0.000290 s ÷ tempo). Como o trabalho por processo é muito pequeno, a sobrecarga de inicialização/coordenação do MPI domina.

**Sweet spot de desempenho:**  
- **2 processos** entregam o **menor tempo absoluto** (≈ **0.000233 s**).  
- A partir de **8 processos**, o tempo **aumenta continuamente** — forte indício de overhead dominar o custo computacional do NN com 442 cidades.

---

### **3. Análise do Balanceamento**

| Processos | Variação de tempo | Balanceamento | Eficiência de uso | Speedup estimado |
|----------:|-------------------:|--------------:|------------------:|-----------------:|
| 2          | 1.2%               | 98.8%        | 99.4%            |         1.98× |
| 4          | 2.8%               | 97.2%        | 98.1%            |         3.14× |
| 8          | 38.3%               | 61.7%        | 76.3%            |         5.25× |
| 16         | 52.6%               | 47.4%        | 70.9%            |         8.57× |
| 32         | 45.8%               | 54.2%        | 68.8%            |         7.68× |
| 64         | 65.1%               | 34.9%        | 45.8%            |         5.74× |
| 128        | 82.0%               | 18.0%        | 28.0%            |         4.09× |

**Leituras-chave:**
- Até **4 processos**, o balanceamento é excelente (>97%), mas o ganho real de tempo já é baixo.  
- De **8** a **32 processos**, crescem variação e overhead; em **64–128**, o **desequilíbrio** e o custo de coordenação/IO aumentam bastante.

---

### **4. Insights Importantes**

- **Qualidade melhora com mais processos:** o melhor custo cai de **61984.05** (1P) para **59685.52** (128P), **≈ 3.7%** melhor. Isso sugere que cada processo explora **inícios/seeds diferentes**.  
- **Tempo piora com P alto:** o *wall time* mínimo ocorre em **2P**. A partir daí, a **latência de criação de processos, comunicação e IO** supera o ganho de paralelismo (o kernel NN é muito curto).  
- **Diferença vs OpenMP:** no seu experimento OpenMP, o melhor custo foi **58952.97**, **menor** que qualquer custo obtido com MPI. Vale **alinhar a heurística** (mesmas cidades iniciais, mesma leitura de instância e cálculo de distâncias) para comparações justas.

---

### **5. Recomendações**

**Se a meta é tempo (latência mínima):**
- Use **2 processos** (melhor *wall time*). **1 processo** também é muito bom e mais simples.
- Em uma única máquina, prefira **OpenMP** para este problema pequeno; o overhead do MPI tende a dominar.

**Se a meta é qualidade (menor custo):**
- Rode com **vários processos (32–128)**, cada um com **seed/partição de cidades inicial diferente** (ex.: `start = rank, rank+P, …`).  
- Estabeleça um **orçamento de tempo** e faça **várias rodadas curtas**, guardando o **melhor de N**.

**Robustez/engenharia:**
- Dê **seeds únicas por rank** e assegure **determinismo** opcional para reprodutibilidade.  
- Reduza IO: **agregue no rank 0** via `MPI_Reduce`/**`MPI_MINLOC`** (min custo + rank) e imprima **apenas a rota vencedora**.  
- Considere **híbrido MPI+OpenMP** (menos processos MPI, alguns threads por processo) para reduzir overhead e manter exploração.  
- Faça *pinning* e mapeamento sensato (`--bind-to core`, `--map-by ppr:N:socket`) se estiver numa máquina multi-socket.

---

### **6. Conclusão**

No *pcb442.tsp*, a abordagem MPI com Nearest Neighbor **melhora a qualidade** quando aumentamos o número de processos (até **≈3.7%** melhor custo), mas o **tempo total cresce** após **2 processos** devido ao overhead de paralelização. Para **latência**, **2 processos** (ou **1**) são ideais; para **qualidade**, usar muitos processos faz sentido, preferencialmente com **seeds/partições distintas** e **agregação eficiente** do melhor resultado. Alinhe a heurística com a versão OpenMP para garantir comparações equivalentes de custo.
