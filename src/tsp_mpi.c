#include <mpi.h>
#include "utils_mpi.h"

int main(int argc, char *argv[]) {
    int rank, size, n;
    Cidade *cidades = NULL;
    int *melhor_rota = NULL;
    int algoritmo;
    double t_inicio_global = 0.0, t_fim_global = 0.0;
    
    if (argc < 3) {
        printf("Uso: %s <arquivo.tsp> <algoritmo>\n", argv[0]);
        printf("Algoritmos: 0=forca bruta, 1=nearest neighbor, 2=2-opt\n");
        return 1;
    }
    
    algoritmo = atoi(argv[2]);
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Processo 0 lê o arquivo
    if (rank == 0) {
        // Primeira passada: descobrir quantas cidades há
        if (descobrir_tamanho_instancia(argv[1], &n) != 0) {
            printf("Erro ao ler dimensões do arquivo %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // Alocar dinamicamente baseado no tamanho real
        cidades = malloc(n * sizeof(Cidade));
        if (!cidades) {
            printf("Erro ao alocar memoria para %d cidades\n", n);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // Segunda passada: ler as coordenadas
        if (ler_instancia(argv[1], cidades, &n) != 0) {
            printf("Erro ao ler arquivo %s\n", argv[1]);
            free(cidades);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        printf("TSP com %d cidades, algoritmo %d, %d processos\n", n, algoritmo, size);
    }
    
    // Enviar dados para todos os processos
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&algoritmo, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    if (rank != 0) {
        cidades = malloc(n * sizeof(Cidade));
        if (!cidades) {
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    melhor_rota = malloc(n * sizeof(int));
    if (!melhor_rota) {
        free(cidades);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    MPI_Bcast(cidades, n * sizeof(Cidade), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    // Sincronizar todos os processos antes de iniciar
    MPI_Barrier(MPI_COMM_WORLD);
    t_inicio_global = MPI_Wtime();
    
    // Processo 0 anuncia qual algoritmo será executado
    if (rank == 0) {
        if (algoritmo == 0) {
            printf("Executando forca bruta com MPI...\n");
        } else if (algoritmo == 1) {
            printf("Executando nearest neighbor com MPI...\n");
        } else {
            printf("Executando 2-opt com MPI...\n");
        }
    }

    // Executar algoritmo escolhido
    double melhor_custo_local;
    double tempo_algoritmo_inicio = MPI_Wtime();
    
    if (algoritmo == 0) {
        melhor_custo_local = forca_bruta(cidades, n, rank, size, melhor_rota);
    } else if (algoritmo == 1) {
        melhor_custo_local = nearest_neighbor(cidades, n, rank, melhor_rota);
    } else {
        melhor_custo_local = two_opt(cidades, n, rank, size, melhor_rota);
    }
    
    double tempo_algoritmo = MPI_Wtime() - tempo_algoritmo_inicio;
    
    // Sincronizar antes da coleta de métricas
    MPI_Barrier(MPI_COMM_WORLD);
    t_fim_global = MPI_Wtime();
    double tempo_total = t_fim_global - t_inicio_global;
    
    // Encontrar melhor resultado e coletar métricas de tempo
    double melhor_custo_global = DBL_MAX;
    double tempo_max, tempo_min, tempo_soma;
    
    // Garantir que custos inválidos não afetem o resultado
    double custo_para_reduce = (melhor_custo_local == DBL_MAX) ? 1e20 : melhor_custo_local;
    
    MPI_Reduce(&custo_para_reduce, &melhor_custo_global, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tempo_algoritmo, &tempo_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tempo_algoritmo, &tempo_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tempo_algoritmo, &tempo_soma, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    
    // Broadcast do melhor custo para todos os processos
    MPI_Bcast(&melhor_custo_global, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    
    // Forçar flush de todos os buffers antes de imprimir rotas
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Identificar qual processo encontrou a melhor solução - VERSÃO CORRIGIDA
    int processo_vencedor = -1;
    
    // Cada processo verifica se tem o melhor custo (com tolerância)
    int eh_vencedor = 0;
    if (melhor_custo_local != DBL_MAX && fabs(melhor_custo_local - melhor_custo_global) < 1e-6) {
        eh_vencedor = 1;
    }    
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Método mais simples: usar MPI_Allreduce para encontrar o menor rank que é vencedor
    int rank_vencedor = eh_vencedor ? rank : INT_MAX;
    int rank_vencedor_global;
    
    MPI_Allreduce(&rank_vencedor, &rank_vencedor_global, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    
    if (rank_vencedor_global != INT_MAX) {
        processo_vencedor = rank_vencedor_global;
    }
    
    // Coletar detalhes de todos os processos para mostrar
    double *todos_custos = NULL;
    double *todos_tempos = NULL;
    
    if (rank == 0) {
        todos_custos = malloc(size * sizeof(double));
        todos_tempos = malloc(size * sizeof(double));
    }
    
    // Coletar custos e tempos de todos os processos
    double custo_para_gather = (melhor_custo_local == DBL_MAX) ? -1.0 : melhor_custo_local;
    MPI_Gather(&custo_para_gather, 1, MPI_DOUBLE, todos_custos, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(&tempo_algoritmo, 1, MPI_DOUBLE, todos_tempos, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    
    // Processo 0 mostra detalhes de todos os processos
    if (rank == 0) {
        printf("\n=== DETALHES POR PROCESSO ===\n");
        for (int i = 0; i < size; i++) {
            printf("Processo %d: custo=%.2f, tempo=%.6fs", i, todos_custos[i], todos_tempos[i], "\n");           
        }
        
        free(todos_custos);
        free(todos_tempos);
    }
    
    // Apenas o processo vencedor mostra sua rota
    if (rank == processo_vencedor) {
        printf("\n=== ROTA VENCEDORA (Processo %d) ===\n", rank);
        printf("Custo: %.2f\n", melhor_custo_local);
        printf("Rota: ");
        for (int i = 0; i < n; i++) {
            printf("%d", melhor_rota[i]);
            if (i < n - 1) printf(" -> ");
        }
        printf(" -> %d\n", melhor_rota[0]);
        fflush(stdout);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    // AGORA sim imprimir métricas (APENAS processo 0)
    if (rank == 0) {
        double tempo_medio = tempo_soma / size;
        
        printf("\n=== METRICAS DE PARALELIZACAO ===\n");
        printf("Melhor custo encontrado: %.2f (Processo %d)\n", melhor_custo_global, processo_vencedor);
        printf("Numero de processos: %d\n", size);
        printf("Tempo total (wall clock): %.6f segundos\n", tempo_total);
        printf("Tempo maximo: %.6f segundos\n", tempo_max);
        printf("Tempo medio:  %.6f segundos\n", tempo_medio);
        printf("Tempo minimo: %.6f segundos\n", tempo_min);
        
        // Só mostrar métricas de balanceamento se houver múltiplos processos
        if (size > 1) {
            double variacao_tempo = ((tempo_max - tempo_min) / tempo_max) * 100;
            double balanceamento = (tempo_min / tempo_max) * 100;
            double eficiencia_uso = (tempo_medio / tempo_max) * 100;

            // "Speedup estimado: tempo sequencial ~ tempo_medio"
            double speedup_real = (tempo_total > 0.0) ? (tempo_soma / tempo_total) : 0.0;
            double eficiencia_paralela = (size > 0) ? (speedup_real / size) * 100.0 : 0.0;
            
            printf("\n=== BALANCEAMENTO DE CARGA ===\n");
            printf("Variacao de tempo: %.1f%%\n", variacao_tempo);
            printf("Balanceamento:      %.1f%%\n", balanceamento);
            printf("Eficiencia de uso: %.1f%%\n", eficiencia_uso);

            printf("\n=== METRICAS DE PARALELIZACAO ===\n");
            printf("Speedup estimado:   %.2fx\n", speedup_real);
            printf("Eficiencia paralela: %.1f%%\n", eficiencia_paralela);
            
        } else {
            printf("\n=== EXECUCAO SEQUENCIAL ===\n");
            printf("Executando com 1 processo (sem paralelizacao)\n");
        }
        
        fflush(stdout); // Garantir que métricas sejam impressas
    }
    
    free(cidades);
    free(melhor_rota);
    MPI_Finalize();
    return 0;
}