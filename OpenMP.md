# Resultados do OpenMP (Nearest Neighbor) (pcb442.tsp)

## **📊 Análise dos Resultados:**

### **1. Consistência da Qualidade da Solução:**

| Threads | Melhor Custo | Thread Vencedor | Melhoria vs 1 thread |
|---------|--------------|-----------------|----------------------|
| 1       | 58952.97     | 0               | Baseline            |
| 2       | 58952.97     | 1               | **0.0% (igual)**    |
| 4       | 58952.97     | 2               | **0.0% (igual)**    |
| 8       | 58952.97     | 0               | **0.0% (igual)**    |
| 16      | 58952.97     | 1               | **0.0% (igual)**    |
| 32      | 58952.97     | 3               | **0.0% (igual)**    |
| 64      | 58952.97     | 20              | **0.0% (igual)**    |
| 128     | 58952.97     | 5               | **0.0% (igual)** 🎯 |

### **2. Padrão de Escalabilidade Temporal:**

#### **⚡ Speedup Quase Linear (1-8 threads):**
- **1 thread**: 0.102s
- **2 threads**: 0.052s → **Speedup 1.96x**
- **4 threads**: 0.034s → **Speedup 3.00x**
- **8 threads**: 0.014s → **Speedup 7.29x** ⭐

#### **📉 Platô de Performance (16+ threads):**
- **16-128 threads**: Tempo estável entre 0.014-0.019s
- **Overhead de sincronização** começa a dominar

### **3. Análise do Balanceamento de Carga:**

| Threads | Variação Tempo | Balanceamento | Eficiência | Speedup | Efic. Paralela |
|---------|----------------|---------------|------------|---------|----------------|
| 2       | 0.0%          | 100.0%        | 100.0%     | 0.98x   | **49.0%**      |
| 4       | 0.0%          | 100.0%        | 100.0%     | 1.00x   | **25.0%**      |
| 8       | 0.0%          | 100.0%        | 100.0%     | 1.00x   | **12.5%**      |
| 16      | 0.0%          | 100.0%        | 100.0%     | 0.93x   | **5.8%**       |
| 32      | **46.2%**     | **53.8%**     | 90.6%      | 0.84x   | **2.6%**       |
| 64      | **61.5%**     | **38.5%**     | 81.1%      | 0.66x   | **1.0%**       |
| 128     | **92.3%**     | **7.7%**      | 48.9%      | 0.33x   | **0.3%**       |

### **4. Insights Importantes:**

#### **🎯 Determinismo vs Aleatoriedade:**
- **OpenMP mantém qualidade consistente** (58952.97 em todas as execuções)
- **Diferente do MPI**: Não há "lottery effect" - mesmo algoritmo, mesma rota ótima
- **Thread vencedora varia**, mas **custo sempre idêntico**

#### **⚡ Eficiência de Paralelização:**
- **1-8 threads**: Balanceamento perfeito (100.0%)
- **Sweet spot: 4-8 threads** para melhor speedup real
- **16+ threads**: Overhead de sincronização cresce drasticamente

#### **🚫 Problemas com Muitas Threads:**
- **32+ threads**: Desbalanceamento severo (>46% variação)
- **128 threads**: 39 threads falharam completamente ("não encontrou solução válida")
- **Contenção de recursos** e overhead de criação de threads

### **5. Comparação OpenMP vs MPI:**

#### **Qualidade da Solução:**
- **OpenMP**: Consistente (58952.97) ✅
- **MPI**: Melhora com mais processos (61984.05 → 59685.52) ⭐

#### **Speedup:**
- **OpenMP**: Linear até 8 threads, depois platô
- **MPI**: Foco na qualidade, não no tempo

#### **Balanceamento:**
- **OpenMP**: Perfeito até 16 threads
- **MPI**: Problemas desde 8 processos

### **6. Padrão de Falhas (128 threads):**
- **39 threads falharam** ("custo=-1.00, tempo=0.001000s")
- **Possível causa**: Race conditions ou esgotamento de recursos
- **Threads válidas**: Ainda encontram a solução ótima

### **7. Recomendações:**

#### **Para Performance Máxima:**
- **Use 4-8 threads** para melhor speedup e eficiência
- **Evite 32+ threads** devido ao overhead excessivo

#### **Para Estabilidade:**
- **Até 16 threads** mantém balanceamento perfeito
- **128 threads** pode causar falhas de execução

#### **Trade-off Ideal:**
- **8 threads**: Speedup 7.29x com balanceamento perfeito
- **Tempo: 0.014s vs 0.102s sequencial** (86% mais rápido)

### **8. Conclusão Científica:**

O OpenMP com Nearest Neighbor demonstra **excelente paralelização de speedup** até 8 threads, mantendo **qualidade de solução consistente**. Diferentemente do MPI, o OpenMP não melhora a qualidade da solução (mesmo algoritmo determinístico), mas oferece **aceleração significativa** com **balanceamento perfeito** em configurações otimizadas. O limite prático é **8-16 threads** antes que o overhead domine a performance. 🚀⚡

---

## **📈 Gráficos de Performance:**

### **Speedup vs Threads:**
```
Speedup = Tempo_1_thread / Tempo_N_threads

1 thread:   1.00x (baseline)
2 threads:  1.96x ████████████████████
4 threads:  3.00x ██████████████████████████████
8 threads:  7.29x █████████████████████████████████████████████████████████████████████████
16 threads: 6.80x ████████████████████████████████████████████████████████████████████
32 threads: 7.29x █████████████████████████████████████████████████████████████████████████
64 threads: 6.38x ████████████████████████████████████████████████████████████████
128 threads: 5.37x █████████████████████████████████████████████████████████
```

### **Eficiência Paralela vs Threads:**
```
Eficiência = Speedup / Número_de_Threads * 100%

2 threads:  49.0% ████████████████████████████████████████████████████
4 threads:  25.0% █████████████████████████
8 threads:  12.5% █████████████
16 threads:  5.8% ██████
32 threads:  2.6% ███
64 threads:  1.0% █
128 threads: 0.3% ▌
```

---

## **🔍 Análise Detalhada por Thread Count:**

### **2 Threads:**
- **Melhor configuração para eficiência** (49.0%)
- **Speedup próximo ao ideal** (1.96x de 2.00x possível)
- **Balanceamento perfeito** sem overhead significativo

### **4 Threads:**
- **Speedup excelente** (3.00x de 4.00x possível)
- **Eficiência ainda alta** (25.0%)
- **Ponto ideal para CPUs quad-core**

### **8 Threads:**
- **Melhor speedup absoluto** (7.29x)
- **Sweet spot para performance**
- **Último ponto com balanceamento perfeito**

### **16+ Threads:**
- **Plateau de performance** - speedup não melhora significativamente
- **Eficiência diminui drasticamente**
- **Overhead de sincronização domina**

### **128 Threads:**
- **Degradação severa** (39 threads falham)
- **Contenção de recursos** crítica
- **Não recomendado para produção**

---

## **⚖️ Comparativo Detalhado: OpenMP vs MPI**

| Aspecto | OpenMP | MPI |
|---------|--------|-----|
| **Qualidade da Solução** | Consistente (58952.97) | Melhora com paralelização |
| **Melhor Speedup** | 7.29x (8 threads) | Foco na qualidade |
| **Balanceamento** | Perfeito até 16 threads | Problemas desde 8 processos |
| **Determinismo** | Totalmente determinístico | "Lottery effect" benéfico |
| **Overhead** | Baixo até 16 threads | Alto desde início |
| **Falhas** | Apenas com 128 threads | Raras |
| **Uso Prático** | 4-8 threads ideais | 8-16 processos para qualidade |

---

## **🎯 Conclusões Finais:**

1. **OpenMP é superior para speedup puro** quando a qualidade da solução já é conhecida
2. **MPI é superior para exploração** e descoberta de soluções melhores
3. **8 threads é o sweet spot** para OpenMP com este algoritmo
4. **Overhead de sincronização** é o principal limitador em configurações maiores
5. **Determinismo do OpenMP** é vantagem e desvantagem dependendo do objetivo