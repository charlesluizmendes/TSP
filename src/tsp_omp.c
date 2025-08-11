// tsp_omp.c
// TSP heurístico (Nearest Neighbor + 2-Opt) com leitura TSPLIB95 (EUC_2D)
// Paralelismo por "starts" via OpenMP (for schedule(dynamic))
// Instrumentado com métricas de tempo por fase e speedup vs sequencial.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <omp.h>

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

    int id; double x,y; int count=0;
    char *ok;
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
    int improved = 0;
    int old = tour_length(coord, tour, n);
    for (int i=1; i<n-1; ++i) {
        for (int k=i+1; k<n; ++k) {
            int len = k - i + 1;
            for (int a=0; a<len/2; ++a) {
                int tmp = tour[i+a];
                tour[i+a] = tour[k-a];
                tour[k-a] = tmp;
            }
            int newlen = tour_length(coord, tour, n);
            if (newlen < old) { improved = 1; old = newlen; }
            else {
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
    double t0 = omp_get_wtime();
    int *tour = (int*)malloc(n*sizeof(int));
    int best_len = INT_MAX;
    for (int s=1; s<=n; ++s) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len) {
            best_len = len;
            memcpy(best_tour_seq, tour, n*sizeof(int));
        }
    }
    double t = omp_get_wtime() - t0;
    *best_len_seq = best_len;
    free(tour);
    return t;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: OMP_NUM_THREADS=k %s <arquivo.tsp>\n", argv[0]);
        return 1;
    }

    // Métricas
    double t_total0 = omp_get_wtime(), t_total;
    double t_io0=0, t_io=0, t_par0=0, t_par=0;
    double max_thread_work = 0.0;

    // Leitura TSPLIB
    Coord *coord = NULL; int n=0;
    t_io0 = omp_get_wtime();
    if (!read_tsplib_euc2d(argv[1], &coord, &n)) return 1;
    t_io = omp_get_wtime() - t_io0;

    // Paralelismo por starts
    int *best_tour = (int*)malloc(n*sizeof(int));
    int best_len = INT_MAX;

    t_par0 = omp_get_wtime();

    #pragma omp parallel
    {
        int *tour = (int*)malloc(n*sizeof(int));
        int *local_best_tour = (int*)malloc(n*sizeof(int));
        int local_best = INT_MAX;

        double t0 = omp_get_wtime();

        #pragma omp for schedule(dynamic)
        for (int s=1; s<=n; ++s) {
            int len = solve_from_start(coord, n, s, tour);
            if (len < local_best) {
                local_best = len;
                memcpy(local_best_tour, tour, n*sizeof(int));
            }
        }

        double t_thread = omp_get_wtime() - t0;

        #pragma omp critical
        {
            if (local_best < best_len) {
                best_len = local_best;
                memcpy(best_tour, local_best_tour, n*sizeof(int));
            }
            if (t_thread > max_thread_work) max_thread_work = t_thread;
        }

        free(local_best_tour);
        free(tour);
    }

    t_par = omp_get_wtime() - t_par0;
    t_total = omp_get_wtime() - t_total0;

    // Saída
    printf("\n=== TSP OpenMP (NN + 2-Opt) ===\n");
    printf("Instancia: %s  (n=%d)\n", argv[1], n);
    printf("Melhor comprimento: %d\n", best_len);
    printf("Tour: ");
    for (int i=0;i<n;i++) printf("%d ", best_tour[i]);
    printf("\n");

    // Speedup vs sequencial
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
    printf("Leitura (I/O):            %.6f s (%.1f%%)\n", t_io,  (t_io  / t_total)*100.0);
    printf("Regiao paralela:          %.6f s (%.1f%%)\n", t_par, (t_par / t_total)*100.0);
    printf("Pior thread (NN+2-Opt):   %.6f s (dentro da regiao)\n", max_thread_work);

    printf("\n--- CONCLUSOES ---\n");
    if (t_par > t_io)
        printf("Paralelismo eficaz: trabalho util supera overhead de I/O\n");
    else
        printf("Paralelismo limitado: I/O domina o tempo total\n");

    free(best_tour_seq);
    free(best_tour);
    free(coord);
    return 0;
}
