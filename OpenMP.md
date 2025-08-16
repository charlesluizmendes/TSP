# Resultados do **OpenMP** (Nearest Neighbor) — *pcb442.tsp*

## ** Análise dos Resultados**

### **1) Qualidade da solução vs número de threads**

> Baseline (1 thread): **58952.97**

| Threads | Melhor Custo  | Thread vencedora  | Melhoria vs 1 thread |
|--------:|--------------:|------------------:|---------------------:|
| 1       | **58952.97**  | 0                 | Baseline             |
| 2       | **58952.97**  | 1                 | 0.0%                 |
| 4       | **58952.97**  | 2                 | 0.0%                 |
| 8       | **58952.97**  | 0                 | 0.0%                 |
| 16      | **58952.97**  | 1                 | 0.0%                 |
| 32      | **58952.97**  | 3                 | 0.0%                 |
| 64      | **58952.97**  | 20                | 0.0%                 |
| 128     | **58952.97**  | 5                 | 0.0%                 |

---

### **2. Padrão de Escalabilidade**

| Threads | Tempo (s) | Speedup **real** (vs 1T) | Eficiência paralela **real** |
|--------:|----------:|--------------------------:|------------------------------:|
| 1       | 0.102     | 1.00×                    | 100.0%                        |
| 2       | 0.052     | 1.96×                    | 98.1%                         |
| 4       | 0.034     | 3.00×                    | 75.0%                         |
| 8       | 0.014     | **7.29×**                | **91.1%**                     |
| 16      | 0.015     | 6.80×                    | 42.5%                         |
| 32      | 0.014     | **7.29×**                | 22.8%                         |
| 64      | 0.016     | 6.38×                    | 10.0%                         |
| 128     | 0.019     | 5.37×                    | 4.2%                          |

> Observação: a ferramenta também reporta um “Speedup estimado” interno (0.98×, 1.00×, …), mas ele não coincide com o **speedup real** calculado a partir do *wall clock*. Acima, apresento o **speedup real** (0.102 s ÷ tempo).

**Sweet spot de desempenho:**  
- **8–32 threads** entregam os **melhores tempos absolutos** (~0.014 s), com **8 threads** atingindo o melhor compromisso entre velocidade e eficiência.

---

### **3. Análise do Balanceamento**

| Threads | Variação de tempo | Balanceamento | Eficiência de uso | Speedup estimado |
|--------:|-------------------:|--------------:|------------------:|-----------------:|
| 2       | 0.0%               | 100.0%        | 100.0%            | 0.98×            |
| 4       | 0.0%               | 100.0%        | 100.0%            | 1.00×            |
| 8       | 0.0%               | 100.0%        | 100.0%            | 1.00×            |
| 16      | 0.0%               | 100.0%        | 100.0%            | 0.93×            |
| 32      | **46.2%**          | **53.8%**     | **90.6%**         | 0.84×            |
| 64      | **61.5%**          | **38.5%**     | **81.1%**         | 0.66×            |
| 128     | **92.3%**          | **7.7%**      | **48.9%**         | 0.33×            |

**Leituras-chave:**
- Até **16 threads**, o programa reporta *balanceamento perfeito* (variação 0%).  
- A partir de **32 threads**, cresce a **variação de tempo** entre threads e cai o *balanceamento*, indicando **sobrecustos e ociosidade**.
- Em **128 threads**, além de forte desequilíbrio, várias threads retornam **“não encontrou solução válida”** — provável efeito colateral de saturação/concorrência (ver recomendações).

---

### **4. Insights Importantes**

- **Qualidade constante:** como a política de busca é *Nearest Neighbor* e o conjunto de pontos é fixo, o melhor custo aparece com qualquer número de threads; a paralelização não “descobre” custos menores neste setup.
- **Escalonamento prático:** ganhos **quase lineares até 4T** e **excelentes em 8T** (7.29×). Em **32T**, o tempo volta a 0.014 s (ainda ótimo), mas a **eficiência por thread** cai.
- **Sinais de contenção/overhead:** acima de 32T,
  - aumentam discrepâncias de tempo entre threads,
  - surgem threads sem solução válida (128T), sugerindo **problemas de gestão de memória/seed/estado** por thread ou **zonas críticas** mal protegidas.

---

### **5. Recomendações**

**Para tempo de execução (desempenho):**
- Use **8 threads** como padrão (melhor tempo e alta eficiência).
- **32 threads** também entrega o melhor tempo, mas com **eficiência bem menor** — útil se a máquina estiver ociosa e você quiser latência mínima.

**Para estabilidade/robustez:**
- Evite **128 threads**: há muitas threads sem solução válida.  
  - Garanta que **todas as estruturas** usadas no NN sejam **privadas por thread** (`private`, `firstprivate`) ou **reduções adequadas**.
  - Verifique **seeds** e buffers por thread; minimize seções críticas (`critical`) e acessos compartilhados.

**Para análises futuras:**
- Logue **cidade inicial por thread** e **custo por thread** em CSV para avaliar distribuição de custos.
- Teste com **outras heurísticas** (2-opt/3-opt locais após NN) para verificar se múltiplas threads trazem **melhorias de qualidade** (não só tempo).

---

### **6. Conclusão**

No *pcb442.tsp*, com OpenMP+Nearest Neighbor, a paralelização **reduz fortemente o tempo** (até ~**7.3×** em 8–32 threads), **sem alterar a qualidade** (melhor custo **58952.97** em todos os casos). O **ponto ótimo prático** é **8 threads**, combinando **tempo mínimo** e **alta eficiência**. Acima de 32 threads, os **overheads e desequilíbrios** crescem, e em 128 threads surgem **falhas de solução** — sinal para revisar o desenho das regiões paralelas e o isolamento de dados por thread.
