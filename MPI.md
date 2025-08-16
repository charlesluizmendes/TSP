# Resultados do MPI (Nearest Neighbor) (pcb442.tsp)

## **📊 Análise dos Resultados:**

### **1. Melhoria da Qualidade da Solução com Paralelização:**

| Processos | Melhor Custo | Processo Vencedor | Melhoria vs 1 proc |
|-----------|--------------|-------------------|---------------------|
| 1         | 61984.05     | 0                 | Baseline           |
| 2         | 61749.42     | 1                 | **0.4% melhor**    |
| 4         | 61738.92     | 3                 | **0.4% melhor**    |
| 8         | **59737.76** | 6                 | **3.6% melhor** ⭐ |
| 16        | **59737.76** | 6                 | **3.6% melhor**    |
| 32        | **59737.76** | 6                 | **3.6% melhor**    |
| 64        | **59737.76** | 6                 | **3.6% melhor**    |
| 128       | **59685.52** | 107               | **3.7% melhor** 🎯 |

### **2. Padrão de Escalabilidade:**

#### **🎯 Sweet Spot: 8 processos**
- **Maior salto de qualidade**: De 61738.92 → 59737.76 (diferença de ~2000!)
- **Processo 6 consistentemente vencedor** de 8 até 64 processos
- **128 processos**: Processo 107 encontra solução ainda melhor

#### **📈 Tendência de Melhoria:**
- **1-4 processos**: Melhorias incrementais pequenas (~0.4%)
- **8+ processos**: Salto significativo (~3.6%)
- **128 processos**: Pico de qualidade (~3.7%)

### **3. Análise do Balanceamento:**

| Processos | Variação Tempo | Balanceamento | Eficiência |
|-----------|----------------|---------------|------------|
| 2         | 0.5%          | 99.5%         | 99.8%      |
| 4         | 5.0%          | 95.0%         | 98.5%      |
| 8         | **46.6%**     | **53.4%**     | **70.9%**  |
| 16        | 27.8%         | 72.2%         | 83.7%      |
| 32        | 35.2%         | 64.8%         | 83.2%      |
| 64        | **58.5%**     | **41.5%**     | **64.3%**  |
| 128       | **60.1%**     | **39.9%**     | **51.0%**  |

### **4. Insights Importantes:**

#### **🚀 Vantagem da Paralelização no Nearest Neighbor:**
- **Exploração de múltiplas cidades iniciais** simultaneamente
- **Processo 6** (cidade inicial 6) encontra rota superior consistentemente
- **Processo 107** (cidade inicial 107) descobre a melhor rota absoluta

#### **⚠️ Problemas de Balanceamento:**
- **8+ processos**: Variação de tempo cresce significativamente
- **Overhead de comunicação**: MPI_Gather, MPI_Reduce consomem tempo
- **Diminuição da eficiência**: Com mais processos, menos trabalho útil por processo

#### **🎯 Padrão "Lottery Effect":**
- **Nearest Neighbor é determinístico** por cidade inicial
- **Mais processos = mais "bilhetes de loteria"** (cidades iniciais testadas)
- **Algumas cidades iniciais produzem rotas muito superiores**

### **5. Recomendações:**

#### **Para Qualidade da Solução:**
- **Use 8-16 processos** para o melhor custo-benefício
- **128 processos** se qualidade máxima for prioridade

#### **Para Eficiência:**
- **2-4 processos** para melhor balanceamento
- **Evite 64+ processos** devido ao overhead excessivo

#### **Trade-off Ideal:**
- **8 processos**: Bom equilíbrio entre qualidade (+3.6%) e eficiência (70.9%)

### **6. Conclusão Científica:**

O MPI com Nearest Neighbor demonstra **super-linear speedup em qualidade** - não apenas executa mais rápido, mas encontra **soluções significativamente melhores** devido à exploração paralela de múltiplas cidades iniciais. O "processo sortudo" (6 ou 107) encontra caminhos superiores que o processo sequencial nunca descobriria! 🎯✨