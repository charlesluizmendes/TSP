#!/bin/bash

echo "=== SUBMISSÃO DE JOBS TSP - LoboC ==="
echo ""

# Verificar se está no diretório correto
if [[ ! -f "src/tsp_mpi.c" || ! -f "src/tsp_omp.c" ]]; then
    echo "ERRO: Execute no diretório raiz do projeto"
    echo "Estrutura esperada:"
    echo "  tsp_project/"
    echo "  ├── src/tsp_{mpi,omp}.c"
    echo "  ├── scripts/*.pbs"
    echo "  └── data/*.tsp"
    exit 1
fi

echo "Escolha o tipo de execução:"
echo ""
echo "1. Teste Rápido (1 hora, 8 cores)"
echo "   - Validação inicial"
echo "   - Configurações: 1,2,4,8"
echo ""
echo "2. Benchmark Completo (6 horas, 24 cores)"
echo "   - Todos os arquivos .tsp"
echo "   - Configurações: 1,2,4,8,12,16,20,24"
echo ""
echo "3. Instância Específica (6 horas, 16 cores)"
echo "   - Um arquivo específico"
echo "   - Configurações: 1,2,4,6,8,10,12,14,16"
echo ""

read -p "Digite sua escolha (1, 2 ou 3): " choice

case $choice in
    1)
        echo ""
        echo "Submetendo teste rápido..."
        job_id=$(qsub scripts/test_quick.pbs)
        echo "Job ID: $job_id"
        echo ""
        echo "Comandos para monitorar:"
        echo "  qstat           - Ver status"
        echo "  tail -f logs/test_quick_output.log  - Acompanhar"
        ;;
        
    2)
        echo ""
        tsp_count=$(ls data/*.tsp 2>/dev/null | wc -l)
        if [[ $tsp_count -eq 0 ]]; then
            echo "AVISO: Nenhum arquivo .tsp em data/"
            echo "Execute: scripts/download_data.sh"
            echo ""
            read -p "Continuar? (y/N): " continue_choice
            if [[ ! $continue_choice =~ ^[Yy]$ ]]; then
                exit 0
            fi
        else
            echo "Arquivos encontrados:"
            ls data/*.tsp
        fi
        
        echo ""
        echo "Submetendo benchmark completo..."
        job_id=$(qsub scripts/benchmark_full.pbs)
        echo "Job ID: $job_id"
        echo ""
        echo "ATENÇÃO: Pode demorar até 6 horas!"
        echo "Comandos para monitorar:"
        echo "  qstat           - Ver status"
        echo "  tail -f logs/benchmark_full_output.log  - Acompanhar"
        ;;
        
    3)
        echo ""
        echo "Arquivos disponíveis:"
        if ls data/*.tsp >/dev/null 2>&1; then
            ls data/*.tsp | sed 's|data/||'
        else
            echo "Nenhum arquivo .tsp encontrado"
        fi
        echo ""
        
        read -p "Digite o nome do arquivo (ex: berlin52.tsp): " filename
        
        if [[ ! -f "data/$filename" ]]; then
            echo "ERRO: Arquivo data/$filename não encontrado!"
            exit 1
        fi
        
        # Editar o script
        sed -i "s/INSTANCE=.*/INSTANCE=\"$filename\"/" scripts/run_specific.pbs
        
        echo "Submetendo teste específico para $filename..."
        job_id=$(qsub scripts/run_specific.pbs)
        echo "Job ID: $job_id"
        echo ""
        echo "Comandos para monitorar:"
        echo "  qstat           - Ver status"
        echo "  tail -f logs/specific_output.log  - Acompanhar"
        ;;
        
    *)
        echo "Opção inválida!"
        exit 1
        ;;
esac

echo ""
echo "=== JOB SUBMETIDO! ==="
echo ""
echo "Comandos úteis:"
echo "  qstat -u \$USER  - Ver seus jobs"
echo "  qdel $job_id     - Cancelar job"
echo "  scripts/analyze.sh - Analisar resultados"