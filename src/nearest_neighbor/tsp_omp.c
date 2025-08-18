#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define MAX 10000

typedef struct { int id; double x, y; } City;

City cities[MAX];
int N;
int best_path[MAX], best_path_par[MAX];
double best_cost = 1e9, best_cost_par = 1e9;

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

double nearest_neighbor(int start, int *path) {
    int visited[MAX] = {0};
    path[0] = start;
    visited[start] = 1;
    double cost = 0.0;

    for (int i = 1; i < N; i++) {
        int last = path[i - 1], next = -1;
        double min_d = 1e9;
        for (int j = 0; j < N; j++) {
            if (!visited[j]) {
                double d = dist(cities[last], cities[j]);
                if (d < min_d) {
                    min_d = d;
                    next = j;
                }
            }
        }
        path[i] = next;
        visited[next] = 1;
        cost += min_d;
    }
    cost += dist(cities[path[N - 1]], cities[start]);
    return cost;
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

    double t1 = omp_get_wtime();
    for (int i = 0; i < N; i++) {
        int path[MAX];
        double cost = nearest_neighbor(i, path);
        if (cost < best_cost) {
            best_cost = cost;
            memcpy(best_path, path, sizeof(int) * N);
        }
    }
    double t2 = omp_get_wtime();

    double t3 = omp_get_wtime();
    #pragma omp parallel
    {
        int local_path[MAX];
        double local_best = 1e9;

        #pragma omp for
        for (int i = 0; i < N; i++) {
            int path[MAX];
            double cost = nearest_neighbor(i, path);
            if (cost < local_best) {
                local_best = cost;
                memcpy(local_path, path, sizeof(int) * N);
            }
        }

        #pragma omp critical
        {
            if (local_best < best_cost_par) {
                best_cost_par = local_best;
                memcpy(best_path_par, local_path, sizeof(int) * N);
            }
        }
    }
    double t4 = omp_get_wtime();
    

    print_result("Sequencial", best_path, best_cost, t2 - t1);
    print_result("OpenMP", best_path_par, best_cost_par, t4 - t3);

    double speedup = (t2 - t1) / (t4 - t3);
    double eff = 100.0 * speedup / omp_get_max_threads();
    printf("Speedup: %.2fx\n", speedup);
    printf("Eficiencia: %.2f%%\n", eff);

    return 0;
}
