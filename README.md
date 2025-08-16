#

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -I"$Env:MSMPI_INC" ".\src\tsp_mpi.c" -L"$Env:MSMPI_LIB64" -lmsmpi -lAdvapi32 -o ".\bin\tsp_mpi.exe"
```

```bash
& "C:\msys64\ucrt64\bin\gcc.exe" -O3 -fopenmp -lm ".\src\tsp_omp.c" -o ".\bin\tsp_omp.exe"
```

```bash
mpiexec -n 1 .\bin\tsp_mpi.exe .\data\berlin52.tsp
```

```bash
$env:OMP_NUM_THREADS=8;  .\bin\tsp_omp.exe .\data\berlin52.tsp
```