#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

#define MAX 20

typedef struct { 
    int id; 
    double x, y; 
} City;

City cities[MAX];
int N;

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

void tsp(int level, double cost, int *path, int *vis, 
        double *best_cost, int *best_path, 
        unsigned long long *leaf_count) {
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[path[0]]);
        if (cost < *best_cost) {
            *best_cost = cost;
            memcpy(best_path, path, N * sizeof(int));
        }
        if (leaf_count) (*leaf_count)++;
        return;
    }
    for (int i = 0; i < N; i++) {
        if (!vis[i]) {
            vis[i] = 1;
            path[level] = i;
            tsp(level + 1, cost + dist(cities[path[level-1]], cities[i]),
                path, vis, best_cost, best_path, leaf_count);
            vis[i] = 0;
        }
    }
}

void print_result(const char *label, int *path, double cost, 
        double time, unsigned long long tours) {
    printf("%s:\n", label);
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n", time);
    if (tours > 0)
    {
        double tps = time > 0 ? (double)tours / time : 0.0;
        printf("Tours: %llu (%.3e tours/s)\n", tours, tps);
    }
       
    printf("\n");
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) printf("Uso: %s arquivo.tsp\n", argv[0]);
        MPI_Finalize(); return 1;
    }

    if (rank == 0) read_tsp(argv[1]);
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(cities, sizeof(City)*MAX, MPI_BYTE, 0, MPI_COMM_WORLD);

    double best_cost_seq = 1e9, best_cost_par = 1e9;
    int best_seq[MAX], best_par[MAX];
    double t1, t2, t3, t4;
    unsigned long long seq_leaves = 0ULL;
    unsigned long long local_leaves = 0ULL, global_leaves = 0ULL;

    if (rank == 0) {
        int path[MAX] = {0}, vis[MAX] = {0};
        vis[0] = 1;
        t1 = MPI_Wtime();
        tsp(1, 0, path, vis, &best_cost_seq, best_seq, &seq_leaves);
        t2 = MPI_Wtime();
    }

    double local_best = 1e9;
    int local_path[MAX];
    int path[MAX], vis[MAX];
    MPI_Barrier(MPI_COMM_WORLD);
    t3 = MPI_Wtime();

    for (int i = 1 + rank; i < N; i += size) {
        memset(vis, 0, sizeof(vis));
        path[0] = 0; path[1] = i;
        vis[0] = vis[i] = 1;
        tsp(2, dist(cities[0], cities[i]), path, vis,
            &local_best, local_path, &local_leaves);
   }

    struct { double cost; int rank; } local_min = {local_best, rank};
    struct { double cost; int rank; } global_min;
    MPI_Allreduce(&local_min, &global_min, 1, MPI_DOUBLE_INT, MPI_MINLOC, MPI_COMM_WORLD);
    MPI_Bcast(local_path, MAX, MPI_INT, global_min.rank, MPI_COMM_WORLD);

    MPI_Reduce(&local_leaves, &global_leaves, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    t4 = MPI_Wtime();

    if (rank == 0) {
        print_result("Sequencial", best_seq, best_cost_seq, t2 - t1, seq_leaves);
        print_result("MPI", local_path, global_min.cost, t4 - t3, global_leaves);
        double tempo_seq = t2 - t1;
        double tempo_par = t4 - t3;
        double speedup = tempo_seq / tempo_par;
        double eficiencia = (speedup / size) * 100.0;
        printf("Speedup: %.2fx\n", speedup);
        printf("Eficiencia: %.2f%%\n", eficiencia);

        unsigned long long expected = 1ULL;
        for (int k = 2; k <= N-1; ++k) expected *= k;
        printf("Folhas esperadas: %llu\n", expected);
    }

    MPI_Finalize();
    return 0;
}
