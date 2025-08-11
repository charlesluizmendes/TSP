# Problema do Caixeiro Viajante (TSP - Traveling Salesman Problem)

Este repositório contém duas implementações em C que resolvem o Problema do Caixeiro Viajante (TSP) usando programação paralela: uma com **MPI** e outra com **OpenMP**. Ambas implementam soluções heurísticas para encontrar rotas aproximadamente ótimas.

## **Objetivo do Programa**
O código implementa uma solução heurística para o TSP que encontra uma rota aproximadamente ótima visitando todas as cidades exatamente uma vez e retornando à origem. Ambas as versões usam paralelização para acelerar o processo testando diferentes pontos de partida simultaneamente.

## **Estrutura Principal**

### **1. Leitura de Dados (TSPLIB95)**
```c
int read_tsplib_euc2d(const char *path, Coord **out, int *n)
```
- Lê arquivos no formato TSPLIB95 (padrão para problemas TSP)
- Suporta apenas coordenadas euclidianas 2D (EUC_2D)
- Extrai dimensão, coordenadas x,y de cada cidade

### **2. Algoritmos Heurísticos**

**Nearest Neighbor (Vizinho Mais Próximo):**
```c
void nearest_neighbor(const Coord *coord, int n, int start, int *tour)
```
- Começa em uma cidade inicial
- Sempre vai para a cidade não visitada mais próxima
- Simples mas pode gerar soluções ruins

**2-Opt (Otimização Local):**
```c
void two_opt(const Coord *coord, int *tour, int n)
```
- Melhora uma rota existente
- Tenta trocar segmentos do caminho para reduzir a distância total
- Repete até não conseguir mais melhorias

### **3. Paralelização - Duas Abordagens**

## **Versão MPI (tsp_mpi.c)**

**Características:**
- Múltiplos processos independentes
- Comunicação explícita entre processos
- Ideal para clusters e computação distribuída

**Como funciona:**
- **Processo 0**: Lê o arquivo de entrada
- **Broadcast**: Distribui os dados para todos os processos
- **Divisão do trabalho**: Cada processo testa diferentes cidades como ponto de partida
  - Processo 0 testa cidades 1, 1+P, 1+2P, ...
  - Processo 1 testa cidades 2, 2+P, 2+2P, ...
  - (onde P = número de processos)
- **Redução**: Encontra a melhor solução entre todos os processos
- **Coleta**: O processo 0 recebe a melhor rota completa

## **Versão OpenMP (tsp_omp.c)**

**Características:**
- Múltiplas threads compartilhando memória
- Paralelização com diretivas simples
- Ideal para máquinas multicore

**Como funciona:**
```c
#pragma omp parallel
{
    #pragma omp for schedule(dynamic)
    for (int s=1; s<=n; ++s) {
        // Cada thread testa diferentes pontos de partida
        int len = solve_from_start(coord, n, s, tour);
        // Atualiza melhor resultado local
    }

    #pragma omp critical
    {
        // Atualiza resultado global de forma thread-safe
        if (local_best < best_len) {
            best_len = local_best;
            memcpy(best_tour, local_best_tour, n*sizeof(int));
        }
    }
}
```

### **4. Análise de Performance**

**Versão MPI (análise detalhada):**
- **Tempo de I/O**: Leitura do arquivo
- **Tempo de Broadcast**: Distribuição dos dados
- **Tempo de Trabalho**: Execução dos algoritmos NN+2-Opt
- **Tempo de Redução**: Encontrar o melhor resultado
- **Tempo de Coleta**: Transferir a melhor rota

**Versão OpenMP (análise simplificada):**
- **Tempo de I/O**: Leitura do arquivo
- **Tempo da região paralela**: Todo o trabalho paralelo
- **Tempo da thread mais lenta**: Para detectar desbalanceamento

### **5. Comparação com Versão Sequencial**
```c
double solve_tsp_sequencial(const Coord *coord, int n, int *best_tour_seq, int *best_len_seq)
```
- Executa o mesmo algoritmo sequencialmente
- Calcula o speedup (aceleração) da versão paralela
- Determina se a paralelização foi eficaz

## **Compilação**

### **Versão OpenMP:**
```bash
gcc -O3 -fopenmp -lm "./src/tsp_omp.c" -o "./bin/tsp_omp.exe"
```

### **Versão MPI:**
```bash
gcc -O3 -I"$MSMPI_INC" "./src/tsp_mpi.c" -L"$MSMPI_LIB64" -lmsmpi -lAdvapi32 -o "./bin/tsp_mpi.exe"
```

## **Como Usar**

### **Versão OpenMP:**
```bash
OMP_NUM_THREADS=4 ./bin/tsp_omp.exe arquivo.tsp
```

### **Versão MPI:**
```bash
mpirun -np 4 ./bin/tsp_mpi.exe arquivo.tsp
```

## **Comparação das Abordagens**

| Aspecto | OpenMP | MPI |
|---------|--------|-----|
| **Modelo** | Threads (memória compartilhada) | Processos (memória distribuída) |
| **Complexidade** | Mais simples | Mais complexo |
| **Escalabilidade** | Limitada ao número de cores | Escalável para clusters |
| **Overhead** | Menor | Maior (comunicação) |
| **Balanceamento** | Automático (`schedule(dynamic)`) | Manual (round-robin) |
| **Uso ideal** | Máquinas multicore | Clusters/supercomputadores |

## **Saída do Programa**

Ambas as versões produzem:
- Melhor rota encontrada e seu comprimento
- Análise temporal detalhada
- Speedup comparado à versão sequencial
- Identificação de gargalos
- Conclusões sobre eficácia da paralelização

## **Limitações**
- Usa heurística (não garante solução ótima)
- Suporta apenas coordenadas euclidianas 2D
- Eficiência depende do número de cidades vs. número de threads/processos

## **Quando Usar Cada Versão**

### **Use OpenMP quando:**
- ✅ Executando em uma única máquina multicore
- ✅ Desenvolvimento mais simples é prioridade
- ✅ Overhead de comunicação deve ser mínimo
- ✅ Balanceamento automático de carga é desejado

### **Use MPI quando:**
- ✅ Executando em cluster ou múltiplas máquinas
- ✅ Problema requer memória distribuída
- ✅ Máxima escalabilidade é necessária
- ✅ Tolerância a falhas é importante

Ambas as implementações são excelentes exemplos de como paralelizar algoritmos heurísticos mantendo instrumentação detalhada para análise de performance!



# Compilação e Execução

## **Comando de Compilação OpenMP**

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -fopenmp -lm ".\src\tsp_omp.c" -o ".\bin\tsp_omp.exe"
```

### **O que cada parte faz:**

- **`&`**: Operador do PowerShell para executar comandos
- **`gcc.exe`**: Compilador C do MSYS2 (ambiente Unix no Windows)
- **`-O3`**: Otimização máxima (código mais rápido)
- **`-fopenmp`**: Habilita paralelismo OpenMP (threads)
- **`-lm`**: Liga biblioteca matemática (para `sqrt`, etc.)
- **`src\tsp_omp.c`**: Arquivo fonte a ser compilado
- **`-o bin\tsp_omp.exe`**: Nome do executável gerado

## **Comando de Execução OpenMP:**

```bash
$env:OMP_NUM_THREADS=1;  .\bin\tsp_omp.exe .\data\berlin52.tsp
$env:OMP_NUM_THREADS=2;  .\bin\tsp_omp.exe .\data\berlin52.tsp
$env:OMP_NUM_THREADS=4;  .\bin\tsp_omp.exe .\data\berlin52.tsp
$env:OMP_NUM_THREADS=8;  .\bin\tsp_omp.exe .\data\kroA100.tsp
$env:OMP_NUM_THREADS=16; .\bin\tsp_omp.exe .\data\pcb442.tsp
```

## **Comando de Compilação MPI**

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -I"$Env:MSMPI_INC" ".\src\tsp_mpi.c" -L"$Env:MSMPI_LIB64" -lmsmpi -lAdvapi32 -o ".\bin\tsp_mpi.exe"
```

### **O que cada parte faz:**

- **`&`**: Operador do PowerShell para executar comandos
- **`gcc.exe`**: Compilador C do MSYS2
- **`-O3`**: Otimização máxima (código mais rápido)
- **`-I"$Env:MSMPI_INC"`**: Pasta dos headers MPI (arquivos .h)
- **`src\tsp_mpi.c`**: Arquivo fonte a ser compilado
- **`-L"$Env:MSMPI_LIB64"`**: Pasta das bibliotecas MPI (.lib)
- **`-lmsmpi`**: Liga biblioteca principal do Microsoft MPI
- **`-lAdvapi32`**: Liga biblioteca do Windows (para funções do sistema)
- **`-o bin\tsp_mpi.exe`**: Nome do executável gerado

## **Comando de Execução MPI:**

```bash
mpiexec -n 1 .\bin\tsp_mpi.exe .\data\berlin52.tsp
mpiexec -n 2 .\bin\tsp_mpi.exe .\data\berlin52.tsp
mpiexec -n 4 .\bin\tsp_mpi.exe .\data\kroA100.tsp
mpiexec -n 8 .\bin\tsp_mpi.exe .\data\pcb442.tsp
mpiexec -n 16 .\bin\tsp_mpi.exe .\data\pcb442.tsp
```




