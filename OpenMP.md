# Resultados do **OpenMP** (Nearest Neighbor) — *pcb442.tsp*

## **Análise dos Resultados**

### **1. Qualidade da solução vs número de threads**

> Baseline (1 thread): **58952.97**

| Threads | Melhor Custo  | Thread vencedora  | Melhoria vs 1 thread |
|--------:|--------------:|------------------:|---------------------:|
| 1       | **58952.97**  | 0                 | 0.0%                  |
| 2       | **58952.97**  | 1                 | 0.0%                  |
| 4       | **58952.97**  | 2                 | 0.0%                  |
| 8       | **58952.97**  | 3                 | 0.0%                  |
| 16      | **58952.97**  | 15                | 0.0%                  |
| 32      | **58952.97**  | 15                | 0.0%                  |
| 64      | **58952.97**  | 38                | 0.0%                  |
| 128     | **58952.97**  | 33                | 0.0%                  |

---

### **2. Padrão de Escalabilidade**

| Threads | Tempo (s) | Speedup **real** (vs 1T) | Eficiência paralela **real** |
|--------:|----------:|--------------------------:|------------------------------:|
| 1       | 0.105      | 1.00×                      | 100.0%                         |
| 2       | 0.051      | 2.06×                      | 102.9%                         |
| 4       | 0.030      | 3.50×                      | 87.5%                          |
| 8       | 0.016      | 6.56×                      | 82.0%                          |
| 16      | **0.014**  | **7.50×**                  | **46.9%**                      |
| 32      | 0.015      | 7.00×                      | 21.9%                          |
| 64      | 0.016      | 6.56×                      | 10.3%                          |
| 128     | 0.018      | 5.83×                      | 4.6%                           |

> Observação: a ferramenta também reporta um “Speedup estimado” interno, mas ele **não coincide** com o **speedup real** calculado a partir do *wall clock* (0.105 s ÷ tempo).

**Sweet spot de desempenho:**  
- **16 threads** entregam o **melhor tempo absoluto** (≈ **0.014 s**).  
- **8–32 threads** ficam muito próximos do mínimo, com 8T oferecendo boa velocidade e eficiência.

---

### **3. Análise do Balanceamento**

| Threads | Variação de tempo | Balanceamento | Eficiência de uso | Speedup estimado |
|--------:|-------------------:|--------------:|------------------:|-----------------:|
| 2       | 0.0%               | 100.0%        | 100.0%            |         2.00× |
| 4       | 0.0%               | 100.0%        | 100.0%            |         4.00× |
| 8       | 0.0%               | 100.0%        | 100.0%            |         7.50× |
| 16      | 0.0%               | 100.0%        | 100.0%            |        13.71× |
| 32      | 30.8%               | 69.2%        | 91.3%            |        25.33× |
| 64      | 61.5%               | 38.5%        | 88.3%            |        45.94× |
| 128     | 92.3%               | 7.7%        | 46.2%            |        42.67× |

**Leituras-chave:**
- Até **16 threads**, o programa reporta *balanceamento perfeito* (variação 0%).  
- A partir de **32 threads**, cresce a **variação de tempo** entre threads e cai o *balanceamento*, sinalizando **sobrecustos/ociosidade**.  
- Em **128 threads**, além do forte desequilíbrio, várias threads retornam **custo = −1.00** (não encontrou solução válida).

---

### **4. Insights Importantes**

- **Qualidade constante:** com *Nearest Neighbor* e o mesmo conjunto de cidades, o melhor custo permaneceu **58952.97** em todas as configurações — paralelizar reduziu **tempo**, não **custo**.
- **Escalonamento prático:** ganhos **quase lineares até 4T** e muito bons em **8–16T** (speedup real de **6.56×** e **7.50×**).  
- **Sinais de overhead/limitações:** acima de **32T**, aumentam a desigualdade de tempos e o overhead de criação/coordenação de threads. Em **128T**, surgem **threads sem solução** (custo −1.00), típico de:
  - variáveis não privadas por thread (ex.: vetores `visited`, buffers de rota),
  - divisão de trabalho que deixa threads **sem tarefas** (e você imprime mesmo assim),
  - atualizações competitivas do “melhor” sem proteção adequada.

---

### **5. Recomendações**

**Para tempo de execução (desempenho):**
- Adote **16 threads** como padrão (menor *wall time*).  
- **8 threads** é ótima alternativa (quase tão rápido, com eficiência maior).  
- Acima de **32 threads**, os ganhos saturam e o custo de coordenação domina.

**Para estabilidade/robustez:**
- Evite **128 threads** neste problema: há **custos −1.00**.  
  - Use `default(none)` e marque corretamente `private/firstprivate/shared`.  
  - Garanta **estruturas por thread** (ex.: vetor de visitados e rota local).  
  - Atualize o melhor resultado com `reduction(min:best_cost)` (e guarde a semente/indice do melhor em estrutura separada ou com `critical` mínimo).  
  - Se alguma thread ficar sem trabalho, **não registre** custo/tempo para ela ou inicialize explicitamente um caminho válido (nunca deixe `-1`).

**Para melhor paralelismo:**
- Paralelize o **loop de cidades iniciais** (`for (s=0; s<n; ++s)`) com `#pragma omp parallel for schedule(static)` (ou `dynamic` se custos variarem) — isso evita threads ociosas quando `threads` > trabalho útil.
- **Cap threads** a `min(nCidades, max_threads_físicos)` para reduzir overhead em problemas pequenos.
- Se quiser melhorar **qualidade**, aplique 2-opt/3-opt local após NN; aí sim mais threads podem ajudar a “explorar” melhor.

---

### **6. Conclusão**

No *pcb442.tsp*, com OpenMP + Nearest Neighbor, a paralelização **reduz fortemente o tempo** (melhor *wall time* ≈ **0.014 s** em **16 threads**), mantendo a **mesma qualidade** (melhor custo **58952.97**). O **ponto ótimo prático** fica em **16 threads** (ou **8 threads** como opção muito próxima). Acima de 32 threads, crescem **overheads** e **desequilíbrios**; em 128 threads aparecem **soluções inválidas**, indicando necessidade de revisar o escopo das variáveis e a estratégia de divisão de trabalho.
