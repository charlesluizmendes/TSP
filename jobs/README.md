# Execução em HPC

## Criar pasta "bin"
```
mkdir bin
```

## Compila MPI
```
module purge
module load gcc/11.4.0
module load openmpi-gnu/4.0.1
mpicc -O3 -o bin/tsp_mpi src/brute_force/tsp_mpi.c -lm
```

## Compila OpenMP
```
module purge
module load gcc/11.4.0
gcc -O3 -fopenmp -o bin/tsp_omp src/brute_force/tsp_omp.c -lm
```

## Submete os jobs
```
qsub jobs/tsp_mpi_2.job
qsub jobs/tsp_omp_2.job
```
```
qsub jobs/tsp_mpi_4.job
qsub jobs/tsp_omp_4.job
```
```
qsub jobs/tsp_mpi_8.job
qsub jobs/tsp_omp_8.job
```
```
qsub jobs/tsp_mpi_16.job
qsub jobs/tsp_omp_16.job
```

## Verifica status dos jobs
```
qstat -u $USER
```

## Vê os resultados (arquivos .o<jobid> serão criados)
```
ls -la *.o*
cat tsp_mpi.o<jobid>
cat tsp_omp.o<jobid>
```