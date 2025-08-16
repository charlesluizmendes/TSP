#include <omp.h>
#include "utils_omp.h"

// Estrutura para armazenar resultado de cada thread
typedef struct {
    double custo;
    int *rota;
    int thread_id;
} ResultadoThread;

int main(int argc, char *argv[]) {
    int n, algoritmo;
    Cidade *cidades = NULL;
    int num_threads = omp_get_max_threads(); // Usar todas as threads disponíveis
    
    if (argc < 3) {
        printf("Uso: %s <arquivo.tsp> <algoritmo> [num_threads]\n", argv[0]);
        printf("Algoritmos: 0=forca bruta, 1=nearest neighbor, 2=2-opt\n");
        return 1;
    }
    
    algoritmo = atoi(argv[2]);
    
    // Permitir especificar número de threads
    if (argc >= 4) {
        num_threads = atoi(argv[3]);
        if (num_threads <= 0 || num_threads > omp_get_max_threads()) {
            printf("Número de threads inválido. Usando %d threads.\n", omp_get_max_threads());
            num_threads = omp_get_max_threads();
        }
    }
    
    omp_set_num_threads(num_threads);
    
    // Ler arquivo TSP
    if (descobrir_tamanho_instancia(argv[1], &n) != 0) {
        printf("Erro ao ler dimensões do arquivo %s\n", argv[1]);
        return 1;
    }
    
    cidades = malloc(n * sizeof(Cidade));
    if (!cidades) {
        printf("Erro ao alocar memoria para %d cidades\n", n);
        return 1;
    }
    
    if (ler_instancia(argv[1], cidades, &n) != 0) {
        printf("Erro ao ler arquivo %s\n", argv[1]);
        free(cidades);
        return 1;
    }
    
    printf("TSP com %d cidades, algoritmo %d, %d threads OpenMP\n", n, algoritmo, num_threads);
    
    // Preparar estruturas para resultados
    ResultadoThread *resultados = malloc(num_threads * sizeof(ResultadoThread));
    for (int i = 0; i < num_threads; i++) {
        resultados[i].rota = malloc(n * sizeof(int));
        resultados[i].custo = DBL_MAX;
        resultados[i].thread_id = i;
    }
    
    double tempo_inicio = omp_get_wtime();
    double *tempos_threads = malloc(num_threads * sizeof(double));
    
    // Executar algoritmo escolhido com paralelização OpenMP
    if (algoritmo == 0) {
        // Força bruta com OpenMP
        printf("Executando forca bruta com OpenMP...\n");
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double tempo_thread_inicio = omp_get_wtime();
            
            double melhor_custo = forca_bruta_openmp(cidades, n, tid, num_threads, resultados[tid].rota);
            resultados[tid].custo = melhor_custo;
            tempos_threads[tid] = omp_get_wtime() - tempo_thread_inicio;
        }
        
    } else if (algoritmo == 1) {
        // Nearest neighbor com múltiplas cidades iniciais
        printf("Executando nearest neighbor com OpenMP...\n");
        
        #pragma omp parallel for schedule(dynamic)
        for (int start = 0; start < n; start++) {
            int tid = omp_get_thread_num();
            if (tid == 0 && start == 0) {
                tempos_threads[tid] = omp_get_wtime();
            }
            
            int *rota_temp = malloc(n * sizeof(int));
            double custo = nearest_neighbor(cidades, n, start, rota_temp);
            
            #pragma omp critical
            {
                if (custo < resultados[tid].custo) {
                    resultados[tid].custo = custo;
                    for (int i = 0; i < n; i++) {
                        resultados[tid].rota[i] = rota_temp[i];
                    }
                }
            }
            
            free(rota_temp);
            
            if (tid == 0 && start == n-1) {
                tempos_threads[tid] = omp_get_wtime() - tempos_threads[tid];
            }
        }
        
        // Ajustar tempos para outras threads
        for (int i = 1; i < num_threads; i++) {
            tempos_threads[i] = tempos_threads[0];
        }
        
    } else {
        // 2-opt com OpenMP
        printf("Executando 2-opt com OpenMP...\n");
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double tempo_thread_inicio = omp_get_wtime();
            
            double melhor_custo = two_opt_openmp(cidades, n, tid, num_threads, resultados[tid].rota);
            resultados[tid].custo = melhor_custo;
            tempos_threads[tid] = omp_get_wtime() - tempo_thread_inicio;
        }
    }
    
    double tempo_total = omp_get_wtime() - tempo_inicio;
    
    // Encontrar melhor resultado global e mostrar detalhes por thread
    double melhor_custo_global = DBL_MAX;
    int thread_vencedora = -1;
    
    printf("\n=== DETALHES POR THREAD ===\n");
    for (int i = 0; i < num_threads; i++) {
        double custo_display = (resultados[i].custo == DBL_MAX) ? -1.0 : resultados[i].custo;
        printf("Thread %d: custo=%.2f, tempo=%.6fs", i, custo_display, tempos_threads[i]);
        
        if (resultados[i].custo < melhor_custo_global) {
            melhor_custo_global = resultados[i].custo;
            thread_vencedora = i;
        }
        printf("\n");
    }
    
    // Mostrar resultados
    if (thread_vencedora != -1) {
        printf("\n=== ROTA VENCEDORA (Thread %d) ===\n", thread_vencedora);
        printf("Custo: %.2f\n", melhor_custo_global);
        printf("Rota: ");
        for (int i = 0; i < n; i++) {
            printf("%d", resultados[thread_vencedora].rota[i]);
            if (i < n - 1) printf(" -> ");
        }
        printf(" -> %d\n", resultados[thread_vencedora].rota[0]);
    }
    
    // Calcular métricas de tempo
    double tempo_max = tempos_threads[0];
    double tempo_min = tempos_threads[0];
    double tempo_soma = 0;
    
    for (int i = 0; i < num_threads; i++) {
        if (tempos_threads[i] > tempo_max) tempo_max = tempos_threads[i];
        if (tempos_threads[i] < tempo_min) tempo_min = tempos_threads[i];
        tempo_soma += tempos_threads[i];
    }
    
    double tempo_medio = tempo_soma / num_threads;
    
    printf("\n=== METRICAS DE PARALELIZACAO ===\n");
    printf("Melhor custo encontrado: %.2f (Thread %d)\n", melhor_custo_global, thread_vencedora);
    printf("Numero de threads: %d\n", num_threads);
    printf("Tempo total (wall clock): %.6f segundos\n", tempo_total);
    printf("Tempo maximo (thread): %.6f segundos\n", tempo_max);
    printf("Tempo medio (thread):  %.6f segundos\n", tempo_medio);
    printf("Tempo minimo (thread): %.6f segundos\n", tempo_min);
    
    double variacao_tempo = ((tempo_max - tempo_min) / tempo_max) * 100;
    double balanceamento = (tempo_min / tempo_max) * 100;
    double eficiencia_uso = (tempo_medio / tempo_max) * 100;
    double speedup = (tempo_soma) / (num_threads * tempo_total);
    double eficiencia_paralela = speedup / num_threads * 100;
    
    printf("\n=== BALANCEAMENTO DE CARGA ===\n");
    printf("Variacao de tempo: %.1f%%\n", variacao_tempo);
    printf("Balanceamento:     %.1f%%\n", balanceamento);
    printf("Eficiencia de uso: %.1f%%\n", eficiencia_uso);
    printf("Speedup estimado:  %.2fx\n", speedup);
    printf("Eficiencia paralela: %.1f%%\n", eficiencia_paralela);
    
    // Limpeza
    for (int i = 0; i < num_threads; i++) {
        free(resultados[i].rota);
    }
    free(resultados);
    free(tempos_threads);
    free(cidades);
    
    return 0;
}