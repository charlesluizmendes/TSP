// tsp_mpi.c
// TSP heurístico (Nearest Neighbor + 2-Opt) com leitura TSPLIB95 (EUC_2D)
// Paralelismo por "starts" (cada processo avalia vários nós iniciais)
// Instrumentado com métricas de tempo por fase (I/O, broadcast, trabalho, redução, coleta) e speedup vs sequencial.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <mpi.h> // Biblioteca para programação paralela com MPI

typedef struct { double x, y; } Coord;

// ---------- Utilidades ----------
static inline int euc_2d(const Coord *c, int i, int j) {
    double dx = c[i].x - c[j].x;
    double dy = c[i].y - c[j].y;
    return (int) lround(sqrt(dx*dx + dy*dy));
}

int tour_length(const Coord *coord, const int *tour, int n) {
    long sum = 0;
    for (int i = 0; i < n; ++i) {
        int a = tour[i];
        int b = tour[(i+1)%n];
        sum += euc_2d(coord, a, b);
    }
    return (int)sum;
}

// ---------- TSPLIB (EUC_2D) ----------
int read_tsplib_euc2d(const char *path, Coord **out, int *n) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Erro ao abrir %s\n", path); return 0; }

    char line[512], type[64] = {0};
    int dim = -1;
    int in_coords = 0;

    // Primeiro passe: ler cabeçalho até NODE_COORD_SECTION
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "DIMENSION : %d", &dim) == 1) continue;
        if (sscanf(line, "DIMENSION: %d", &dim) == 1) continue;
        if (sscanf(line, "EDGE_WEIGHT_TYPE : %63s", type) == 1) continue;
        if (sscanf(line, "EDGE_WEIGHT_TYPE: %63s", type) == 1) continue;
        if (strstr(line, "NODE_COORD_SECTION")) { in_coords = 1; break; }
    }
    if (dim <= 0) { fprintf(stderr, "DIMENSION invalida\n"); fclose(f); return 0; }
    if (strcmp(type, "EUC_2D") != 0) {
        fprintf(stderr, "EDGE_WEIGHT_TYPE=%s nao suportado (use EUC_2D)\n", type);
        fclose(f); return 0;
    }
    if (!in_coords) { fprintf(stderr, "NODE_COORD_SECTION nao encontrado\n"); fclose(f); return 0; }

    Coord *coord = (Coord*)calloc(dim+1, sizeof(Coord)); // 1-based
    if (!coord) { fclose(f); return 0; }

    // Ler coordenadas
    int id; double x,y; int count=0;
    while (count < dim && fgets(line, sizeof(line), f)) {
        if (strstr(line, "EOF")) break;
        if (sscanf(line, "%d %lf %lf", &id, &x, &y) == 3) {
            if (id >=1 && id <= dim) { coord[id].x = x; coord[id].y = y; count++; }
        }
    }
    fclose(f);
    if (count != dim) { fprintf(stderr, "Coord insuficientes (%d/%d)\n", count, dim); free(coord); return 0; }
    *out = coord; *n = dim; return 1;
}

// ---------- Heurísticas (NN + 2-Opt) ----------
void nearest_neighbor(const Coord *coord, int n, int start, int *tour) {
    char *used = (char*)calloc(n+1, 1);
    int cur = start;
    for (int i=0;i<n;i++) tour[i]=0;
    for (int i=0;i<n;i++) {
        tour[i] = cur;
        used[cur] = 1;
        int best=-1, bestd=INT_MAX;
        for (int j=1;j<=n;j++) if(!used[j]) {
            int d = euc_2d(coord, cur, j);
            if (d < bestd) { bestd=d; best=j; }
        }
        cur = (best==-1)? start : best;
    }
    free(used);
}

int try_2opt_once(const Coord *coord, int *tour, int n) {
    // retorna 1 se melhorou
    int improved = 0;
    int old = tour_length(coord, tour, n);
    for (int i=1; i<n-1; ++i) {
        for (int k=i+1; k<n; ++k) {
            // swap segmento [i..k]
            int len = k - i + 1;
            for (int a=0; a<len/2; ++a) {
                int tmp = tour[i+a];
                tour[i+a] = tour[k-a];
                tour[k-a] = tmp;
            }
            int newlen = tour_length(coord, tour, n);
            if (newlen < old) { improved = 1; old = newlen; }
            else {
                // desfaz
                for (int a=0; a<len/2; ++a) {
                    int tmp = tour[i+a];
                    tour[i+a] = tour[k-a];
                    tour[k-a] = tmp;
                }
            }
        }
    }
    return improved;
}

void two_opt(const Coord *coord, int *tour, int n) {
    // loop até não haver melhora
    for (;;) {
        if (!try_2opt_once(coord, tour, n)) break;
    }
}

int solve_from_start(const Coord *coord, int n, int start, int *tour_out) {
    nearest_neighbor(coord, n, start, tour_out);
    two_opt(coord, tour_out, n);
    return tour_length(coord, tour_out, n);
}

// ---------- Sequencial (para speedup) ----------
double solve_tsp_sequencial(const Coord *coord, int n, int *best_tour_seq, int *best_len_seq) {
    double t0 = MPI_Wtime();
    int *tour = (int*)malloc((n)*sizeof(int));
    int best_len = INT_MAX; int best_start = 1;
    for (int s=1; s<=n; ++s) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len) {
            best_len = len;
            memcpy(best_tour_seq, tour, n*sizeof(int));
            best_start = s;
        }
    }
    double t = MPI_Wtime() - t0;
    *best_len_seq = best_len;
    free(tour);
    (void)best_start;
    return t;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        if (0==0) fprintf(stderr, "Uso: mpirun -np P %s <arquivo.tsp>\n", argv[0]);
        return 1;
    }

    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    // Métricas
    double t_total0 = MPI_Wtime(), t_total;
    double t_io0=0, t_io=0, t_bcast0=0, t_bcast=0, t_work0=0, t_work=0, t_reduce0=0, t_reduce=0, t_collect0=0, t_collect=0;

    // Leitura TSPLIB (rank 0) + broadcast
    Coord *coord = NULL; int n = 0;

    if (rank == 0) {
        t_io0 = MPI_Wtime();
        if (!read_tsplib_euc2d(argv[1], &coord, &n)) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        t_io = MPI_Wtime() - t_io0;
    }

    t_bcast0 = MPI_Wtime();
    // Broadcast dimensão
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) coord = (Coord*)calloc(n+1, sizeof(Coord));
    // Broadcast coordenadas (2 doubles por ponto, 1-based -> n+1)
    MPI_Bcast(&coord[0], (n+1)*sizeof(Coord), MPI_BYTE, 0, MPI_COMM_WORLD);
    t_bcast = MPI_Wtime() - t_bcast0;

    // Trabalho: cada rank varre starts em round-robin
    int *tour = (int*)malloc(n*sizeof(int));
    int *best_tour_local = (int*)malloc(n*sizeof(int));
    int best_len_local = INT_MAX;

    MPI_Barrier(MPI_COMM_WORLD);
    t_work0 = MPI_Wtime();
    for (int s = 1 + rank; s <= n; s += size) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len_local) {
            best_len_local = len;
            memcpy(best_tour_local, tour, n*sizeof(int));
        }
    }
    t_work = MPI_Wtime() - t_work0;

    // Redução MINLOC para descobrir melhor global
    struct { int len; int rank; } in, out;
    in.len = best_len_local; in.rank = rank;

    t_reduce0 = MPI_Wtime();
    MPI_Allreduce(&in, &out, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);
    t_reduce = MPI_Wtime() - t_reduce0;

    int owner = out.rank;
    int best_len_global = out.len;
    int *best_tour_global = NULL;

    if (rank == 0) {
        best_tour_global = (int*)malloc(n*sizeof(int));
        t_collect0 = MPI_Wtime();
        if (owner == 0) {
            memcpy(best_tour_global, best_tour_local, n*sizeof(int));
        } else {
            MPI_Recv(best_tour_global, n, MPI_INT, owner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        t_collect = MPI_Wtime() - t_collect0;
    } else if (rank == owner) {
        MPI_Send(best_tour_local, n, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    t_total = MPI_Wtime() - t_total0;

    // Reduções de máximos (pior caso) por fase
    double max_io=0, max_bcast=0, max_work=0, max_reduce=0, max_collect=0;
    MPI_Reduce(&t_io,      &max_io,      1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_bcast,   &max_bcast,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_work,    &max_work,    1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_reduce,  &max_reduce,  1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_collect, &max_collect, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        // imprime melhor solução encontrada
        printf("\n=== TSP MPI (NN + 2-Opt) ===\n");
        printf("Instancia: %s  (n=%d)\n", argv[1], n);
        printf("Melhor comprimento: %d\n", best_len_global);
        printf("Tour: ");
        for (int i=0;i<n;i++) printf("%d ", best_tour_global[i]);
        printf("\n");

        // Speedup vs sequencial (mesmo algoritmo)
        int *best_tour_seq = (int*)malloc(n*sizeof(int));
        int best_len_seq=0;
        printf("\n--- SPEEDUP ---\n");
        double t_seq = solve_tsp_sequencial(coord, n, best_tour_seq, &best_len_seq);
        double speedup = t_seq / t_total;
        printf("Tempo Sequencial: %.6f s\n", t_seq);
        printf("Tempo Paralelo:   %.6f s\n", t_total);
        printf("Speedup:          %.2fx\n", speedup);
        printf("Resultado:        %s\n", (speedup > 1.0) ? "Paralelo e MAIS RAPIDO!" : "Sequencial e mais rapido");

        // Gargalos
        printf("\n--- GARGALOS ---\n");
        printf("Leitura (I/O):            %.6f s (%.1f%%)\n", max_io,     (max_io     / t_total)*100.0);
        printf("Broadcast:                %.6f s (%.1f%%)\n", max_bcast,  (max_bcast  / t_total)*100.0);
        printf("Trabalho (NN+2-Opt):      %.6f s (%.1f%%)\n", max_work,   (max_work   / t_total)*100.0);
        printf("Reducao (MINLOC):         %.6f s (%.1f%%)\n", max_reduce, (max_reduce / t_total)*100.0);
        printf("Coleta tour vencedor:     %.6f s (%.1f%%)\n", max_collect,(max_collect/ t_total)*100.0);

        // Conclusões
        printf("\n--- CONCLUSOES ---\n");
        if (max_work > (max_reduce + max_collect))
            printf("Paralelismo eficaz: trabalho util supera overhead de comunicacao\n");
        else
            printf("Paralelismo limitado: comunicacao/organizacao domina\n");

        free(best_tour_seq);
        free(best_tour_global);
    }

    free(best_tour_local);
    free(tour);
    free(coord);

    MPI_Finalize();
    return 0;
}
