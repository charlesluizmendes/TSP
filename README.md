# TSP

## Compilacao

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -I"$Env:MSMPI_INC" ".\src\tsp_mpi.c" -L"$Env:MSMPI_LIB64" -lmsmpi -lAdvapi32 -o ".\bin\tsp_mpi.exe"
```

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -fopenmp -lm ".\src\tsp_omp.c" -o ".\bin\tsp_omp.exe"
```

## Execucao

```bash
mpiexec -np 8 .\bin\tsp_mpi.exe .\data\pcb442.tsp 1
mpiexec -np 8 .\bin\tsp_mpi.exe .\data\pcb442.tsp 2
```

```bash
$env:OMP_NUM_THREADS=8;  .\bin\tsp_omp.exe .\data\pcb442.tsp 1
$env:OMP_NUM_THREADS=8;  .\bin\tsp_omp.exe .\data\pcb442.tsp 2
```