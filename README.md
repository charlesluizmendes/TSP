# TSP (Problema do Caxeiro Viajante)

O Problema do Caixeiro Viajante (TSP) é um problema clássico de otimização combinatória que busca o caminho mais curto para visitar um conjunto de cidades (ou pontos) e retornar à cidade de origem. É um problema NP-difícil, o que significa que a complexidade para encontrar a solução ótima cresce exponencialmente com o número de cidades. 

O TSP é conhecido por ser um problema NP-difícil, o que significa que não há algoritmos conhecidos que possam resolver o problema de forma eficiente (em tempo polinomial) para um grande número de cidades. Para conjuntos de dados maiores, a busca de soluções ótimas torna-se computacionalmente impraticável. 

A seguinte solução propõe a utilização desse algoritmo utilizando bibliotecas do MPI e OpenMP com os datasets de http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/, com as seguintes caracteristicas:

- TYPE: TSP
- EDGE_WEIGHT_TYPE : EUC_2D
- NODE_COORD_SECTION

## Compilação

Para compilar o programa, basta executar os seguintes comandos abaixo:

### MPI

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -I"$Env:MSMPI_INC" ".\src\tsp_mpi.c" -L"$Env:MSMPI_LIB64" -lmsmpi -lAdvapi32 -o ".\bin\tsp_mpi.exe"
```

### OpenMP

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -fopenmp -lm ".\src\tsp_omp.c" -o ".\bin\tsp_omp.exe"
```

## Execução

Para executar o programa e visualizar os resultados basta executar os comandos abaixo:

### MPI

```bash
mpiexec -np 8 .\bin\tsp_mpi.exe .\data\ulysses16.tsp 0
mpiexec -np 4 .\bin\tsp_mpi.exe .\data\kroA100.tsp 1
mpiexec -np 2 .\bin\tsp_mpi.exe .\data\pcb442.tsp 2
```

- `mpiexec -np 8`: Executa o programa com **8 processos MPI**.
- `.\bin\tsp_mpi.exe`: Caminho para o executável compilado com suporte a **MPI**.
- `.\data\pcb442.tsp`: Caminho para o arquivo de entrada no formato **TSPLIB**.
- **Argumento final:**
  - `0`: Algoritmo **Força Bruta** (todas as permutações possíveis)
  - `1`: Algoritmo **Nearest Neighbor** (Vizinho Mais Próximo)
  - `2`: Algoritmo **2-opt** (otimização local)

### OpenM

```bash
$env:OMP_NUM_THREADS=8; .\bin\tsp_omp.exe .\data\ulysses16.tsp 0
$env:OMP_NUM_THREADS=4; .\bin\tsp_omp.exe .\data\kroA100.tsp 1
$env:OMP_NUM_THREADS=2; .\bin\tsp_omp.exe .\data\pcb442.tsp 2
```

- `$env:OMP_NUM_THREADS=8`: Define o uso de **8 threads paralelas** (para PowerShell).
- `.\bin\tsp_omp.exe`: Caminho para o executável compilado com suporte a **OpenMP**.
- `.\data\pcb442.tsp`: Caminho para o arquivo de entrada no formato **TSPLIB**.
- **Argumento final:**
  - `0`: Algoritmo **Força Bruta** (todas as permutações possíveis)
  - `1`: Algoritmo **Nearest Neighbor** (Vizinho Mais Próximo)
  - `2`: Algoritmo **2-opt** (otimização local)