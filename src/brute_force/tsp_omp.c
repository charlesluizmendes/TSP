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

typedef struct {
    double cost;
    int path[MAX];
    int thread_id;
    double individual_time;
} ThreadInfo;

City cities[MAX];
int N;
int best_omp[MAX];
double best_cost_omp = 1e9;
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

void tsp_omp_run(ThreadInfo *all_threads) {
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int path[MAX], local_best[MAX];
        unsigned char vis[MAX];
        double local_cost = 1e9;
        unsigned long long local_leaves = 0ULL;
        double thread_start = omp_get_wtime();

        #pragma omp for schedule(static)
        for (int i = 1; i < N; i++) {
            memset(vis, 0, N * sizeof(unsigned char));
            path[0] = 0; path[1] = i;
            vis[0] = 1; vis[i] = 1;

            dfs_omp(2, dist(cities[0], cities[i]), path, vis,
                    &local_cost, local_best, &local_leaves);
        }

        double thread_end = omp_get_wtime();

        all_threads[thread_id].cost = local_cost;
        all_threads[thread_id].thread_id = thread_id;
        all_threads[thread_id].individual_time = thread_end - thread_start;
        memcpy(all_threads[thread_id].path, local_best, N * sizeof(int));

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

static void print_result(const char *label, int *path, double cost, double time_s) 
{
    printf("%s:\n", label);
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n", time_s);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int num_threads = omp_get_max_threads();
    printf("Numero de threads OpenMP: %d\n\n", num_threads);

    read_tsp(argv[1]);

    ThreadInfo *all_threads = malloc(num_threads * sizeof(ThreadInfo));

    double t1 = omp_get_wtime();
    tsp_omp_run(all_threads);
    double t2 = omp_get_wtime();

    for (int i = 0; i < num_threads; i++) {
        if (all_threads[i].cost < 1e9) {
            char label[20];
            sprintf(label, "Thread %d", i);
            print_result(label, all_threads[i].path, all_threads[i].cost, 
                        all_threads[i].individual_time);
        }
    }

    int winner_thread = -1;
    double best_cost = 1e9;
    double best_time = 1e9;

    for (int i = 0; i < num_threads; i++) {
        if (all_threads[i].cost < 1e9) {
            if (all_threads[i].cost < best_cost || 
               (all_threads[i].cost == best_cost && all_threads[i].individual_time < best_time)) {
                best_cost = all_threads[i].cost;
                best_time = all_threads[i].individual_time;
                winner_thread = i;
            }
        }
    }

    printf("===== RESULTADO FINAL =====\n");
    printf("Thread vencedora: %d\n", winner_thread);
    
    for (int i = 0; i < N; i++) printf("%d ", cities[all_threads[winner_thread].path[i]].id);
    printf("%d\n", cities[all_threads[winner_thread].path[0]].id);
    printf("Custo: %.0f\n", all_threads[winner_thread].cost);
    printf("Tempo: %.6f s\n", all_threads[winner_thread].individual_time);
    printf("Tempo total da execucao paralela: %.6f s\n", t2 - t1);

    free(all_threads);
    return 0;
}