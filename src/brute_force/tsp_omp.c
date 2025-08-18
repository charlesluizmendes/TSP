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

double dist(City a, City b) {
    return round(sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y)));
}

void read_tsp(const char *filename) {
    FILE *f = fopen(filename, "r");
    char line[100];
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "NODE_COORD_SECTION", 18) == 0) break;
    while (fscanf(f, "%d %lf %lf", &cities[N].id, &cities[N].x, &cities[N].y) == 3)
        N++;
    fclose(f);
}

void tsp_seq(int level, double cost, int *path, int *vis) {
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[path[0]]);
        if (cost < best_cost_seq) {
            best_cost_seq = cost;
            memcpy(best_seq, path, N * sizeof(int));
        }
        return;
    }
    for (int i = 0; i < N; i++) {
        if (!vis[i]) {
            vis[i] = 1;
            path[level] = i;
            tsp_seq(level + 1, cost + dist(cities[path[level-1]], cities[i]), path, vis);
            vis[i] = 0;
        }
    }
}

void dfs(int level, double cost, int *path, int *vis, double *local_cost, int *local_best) {
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[0]);
        if (cost < *local_cost) {
            *local_cost = cost;
            memcpy(local_best, path, N * sizeof(int));
        }
        return;
    }
    for (int j = 0; j < N; j++) {
        if (!vis[j]) {
            vis[j] = 1;
            path[level] = j;
            dfs(level + 1, cost + dist(cities[path[level-1]], cities[j]), path, vis, local_cost, local_best);
            vis[j] = 0;
        }
    }
}

void tsp_omp() {
    #pragma omp parallel
    {
        int path[MAX], vis[MAX], local_best[MAX];
        double local_cost = 1e9;

        #pragma omp for
        for (int i = 1; i < N; i++) {
            memset(vis, 0, sizeof(vis));
            path[0] = 0;
            path[1] = i;
            vis[0] = vis[i] = 1;

            dfs(2, dist(cities[0], cities[i]), path, vis, &local_cost, local_best);
        }

        #pragma omp critical
        {
            if (local_cost < best_cost_omp) {
                best_cost_omp = local_cost;
                memcpy(best_omp, local_best, N * sizeof(int));
            }
        }
    }
}

void print_result(const char *label, int *path, double cost, double time) {
    printf("%s:\n", label);
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n\n", time);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s arquivo.tsp\n", argv[0]);
        return 1;
    }

    read_tsp(argv[1]);

    int vis[MAX] = {0}, path[MAX] = {0};
    path[0] = 0;
    vis[0] = 1;

    double t1 = omp_get_wtime();
    tsp_seq(1, 0, path, vis);
    double t2 = omp_get_wtime();

    double t3 = omp_get_wtime();
    tsp_omp();
    double t4 = omp_get_wtime();

    print_result("Sequencial", best_seq, best_cost_seq, t2 - t1);
    print_result("OpenMP", best_omp, best_cost_omp, t4 - t3);

    double tempo_seq = t2 - t1;
    double tempo_omp = t4 - t3;
    double speedup = tempo_seq / tempo_omp;
    double eficiencia = (speedup / omp_get_max_threads()) * 100.0;

    printf("Speedup: %.2fx\n", speedup);
    printf("Eficiencia: %.2f%%\n", eficiencia);

    return 0;
}