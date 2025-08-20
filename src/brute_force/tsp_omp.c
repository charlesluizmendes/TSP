#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <string.h>

#define MAX 20

typedef struct {
    int id;
    double x, y;
} City;

City cities[MAX];
int N;
int best_seq[MAX], best_omp[MAX];
double best_cost_seq = 1e9, best_cost_omp = 1e9;

unsigned long long seq_leaves = 0ULL;
unsigned long long omp_leaves = 0ULL;

static inline int dist(City a, City b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return (int)llround(sqrt(dx*dx + dy*dy));
}

void read_tsp(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { perror("fopen"); exit(1); }
    char line[100];
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "NODE_COORD_SECTION", 18) == 0) break;
    while (N < MAX && fscanf(f, "%d %lf %lf", &cities[N].id, &cities[N].x, &cities[N].y) == 3)
        N++;
    fclose(f);
}

void tsp_seq_dfs(int level, int cost, int *path, unsigned char *vis,
                 double *best_cost, int *best_path,
                 unsigned long long *leaf_count)
{
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[path[0]]);
        if (cost < *best_cost) {
            *best_cost = cost;
            memcpy(best_path, path, N * sizeof(int));
        }
        if (leaf_count) (*leaf_count)++;
        return;
    }
    int prev = path[level-1];
    for (int i = 0; i < N; i++) {
        if (!vis[i]) {
            vis[i] = 1;
            path[level] = i;
            tsp_seq_dfs(level + 1, cost + dist(cities[prev], cities[i]),
                        path, vis, best_cost, best_path, leaf_count);
            vis[i] = 0;
        }
    }
}

void tsp_seq_run() {
    unsigned char vis[MAX] = {0};
    int path[MAX] = {0};
    path[0] = 0; vis[0] = 1;
    tsp_seq_dfs(1, 0, path, vis, &best_cost_seq, best_seq, &seq_leaves);
}

static void dfs_omp(int level, int cost, int *path, unsigned char *vis,
                    double *local_cost, int *local_best,
                    unsigned long long *leaf_count)
{
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[path[0]]);
        if (cost < *local_cost) {
            *local_cost = cost;
            memcpy(local_best, path, N * sizeof(int));
        }
        if (leaf_count) (*leaf_count)++;
        return;
    }
    int prev = path[level-1];
    for (int j = 0; j < N; j++) {
        if (!vis[j]) {
            vis[j] = 1;
            path[level] = j;
            dfs_omp(level + 1, cost + dist(cities[prev], cities[j]),
                    path, vis, local_cost, local_best, leaf_count);
            vis[j] = 0;
        }
    }
}

void tsp_omp_run() {
    #pragma omp parallel
    {
        int path[MAX], local_best[MAX];
        unsigned char vis[MAX];
        double local_cost = 1e9;
        unsigned long long local_leaves = 0ULL;

        #pragma omp for schedule(static)
        for (int i = 1; i < N; i++) {
            memset(vis, 0, N * sizeof(unsigned char));
            path[0] = 0; path[1] = i;
            vis[0] = 1; vis[i] = 1;

            dfs_omp(2, dist(cities[0], cities[i]), path, vis,
                    &local_cost, local_best, &local_leaves);
        }

        #pragma omp critical
        {
            if (local_cost < best_cost_omp) {
                best_cost_omp = local_cost;
                memcpy(best_omp, local_best, N * sizeof(int));
            }
        }

        #pragma omp atomic
        omp_leaves += local_leaves;
    }
}

static void print_result(const char *label, int *path, double cost, 
                    double time_s, unsigned long long tours) 
{
    printf("%s:\n", label);
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n", time_s);
    if (tours > 0) {
        double tps = time_s > 0 ? (double)tours / time_s : 0.0;
        printf("Tours: %llu (%.3e tours/s)\n", tours, tps);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("Número de threads OpenMP: %d\n\n", omp_get_max_threads());

    if (argc < 2) {
        printf("Uso: %s arquivo.tsp\n", argv[0]);
        return 1;
    }

    read_tsp(argv[1]);

    double t1 = omp_get_wtime();
    tsp_seq_run();
    double t2 = omp_get_wtime();

    double t3 = omp_get_wtime();
    tsp_omp_run();
    double t4 = omp_get_wtime();

    print_result("Sequencial", best_seq, best_cost_seq, t2 - t1, seq_leaves);
    print_result("OpenMP",    best_omp, best_cost_omp, t4 - t3, omp_leaves);

    double tempo_seq = t2 - t1;
    double tempo_omp = t4 - t3;
    double speedup = tempo_seq / tempo_omp;
    double eficiencia = (speedup / omp_get_max_threads()) * 100.0;

    printf("Speedup: %.2fx\n", speedup);
    printf("Eficiencia: %.2f%%\n", eficiencia);

    unsigned long long expected = 1ULL;
    for (int k = 2; k <= N-1; ++k) expected *= (unsigned long long)k;
    printf("Folhas esperadas: %llu\n", expected);

    return 0;
}
