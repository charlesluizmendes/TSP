#!/bin/bash

echo "=== ANÁLISE DE RESULTADOS TSP ==="
echo ""

# Encontrar arquivo CSV mais recente
LATEST_CSV=$(ls -t results/*.csv 2>/dev/null | head -1)

if [[ -z "$LATEST_CSV" ]]; then
    echo "❌ Nenhum resultado encontrado em results/"
    echo ""
    echo "Execute primeiro: scripts/run_tests.sh"
    exit 1
fi

echo "📊 Analisando: $LATEST_CSV"
echo "📅 Última modificação: $(date -r "$LATEST_CSV")"
echo ""

# Verificar se arquivo tem dados
total_lines=$(wc -l < "$LATEST_CSV")
if [[ $total_lines -le 1 ]]; then
    echo "❌ Arquivo CSV vazio"
    exit 1
fi

data_lines=$((total_lines - 1))
echo "📈 Total de experimentos: $data_lines"

echo ""
echo "=== ESTATÍSTICAS BÁSICAS ==="

algorithms=$(tail -n +2 "$LATEST_CSV" | cut -d',' -f1 | sort -u | tr '\n' ' ')
echo "Algoritmos: $algorithms"

if grep -q "instance" "$LATEST_CSV"; then
    instances=$(tail -n +2 "$LATEST_CSV" | cut -d',' -f2 | sort -u | tr '\n' ' ')
    echo "Instâncias: $instances"
fi

threads_col=$(head -1 "$LATEST_CSV" | tr ',' '\n' | grep -n "threads" | cut -d':' -f1)
if [[ -n "$threads_col" ]]; then
    configurations=$(tail -n +2 "$LATEST_CSV" | cut -d',' -f$threads_col | sort -nu | tr '\n' ' ')
    echo "Configurações: $configurations"
fi

echo ""
echo "=== TOP 10 SPEEDUPS ==="
echo "Speedup | Algoritmo | Threads"
echo "--------|-----------|--------"

speedup_col=$(head -1 "$LATEST_CSV" | tr ',' '\n' | grep -n "speedup" | cut -d':' -f1)
if [[ -n "$speedup_col" ]]; then
    tail -n +2 "$LATEST_CSV" | \
    awk -F',' -v sc="$speedup_col" -v tc="$threads_col" \
    '$sc > 0 {printf "%.2f    | %-8s | %s\n", $sc, $1, $tc}' | \
    sort -nr | head -10
fi

echo ""
echo "=== COMPARAÇÃO MPI vs OpenMP ==="
echo "Threads | MPI Avg | OpenMP Avg | Vencedor"
echo "--------|---------|------------|----------"

for threads in 2 4 8 16; do
    mpi_avg=$(tail -n +2 "$LATEST_CSV" | \
        awk -F',' -v t="$threads" -v sc="$speedup_col" -v tc="$threads_col" \
        '$1=="MPI" && $tc==t && $sc>0 {sum+=$sc; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}')
    
    omp_avg=$(tail -n +2 "$LATEST_CSV" | \
        awk -F',' -v t="$threads" -v sc="$speedup_col" -v tc="$threads_col" \
        '$1=="OpenMP" && $tc==t && $sc>0 {sum+=$sc; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}')
    
    if [[ "$mpi_avg" != "N/A" && "$omp_avg" != "N/A" ]]; then
        winner=$(echo "$mpi_avg $omp_avg" | awk '{
            if ($1 > $2) print "MPI"
            else if ($2 > $1) print "OpenMP" 
            else print "Empate"
        }')
        printf "%-7s | %-7s | %-10s | %s\n" "$threads" "$mpi_avg" "$omp_avg" "$winner"
    fi
done

echo ""
echo "=== EFICIÊNCIA POR CONFIGURAÇÃO ==="
echo "Threads | MPI Efic.% | OpenMP Efic.%"
echo "--------|------------|---------------"

for threads in 2 4 8 16; do
    # Calcular eficiência = (speedup/threads) * 100
    mpi_eff=$(tail -n +2 "$LATEST_CSV" | \
        awk -F',' -v t="$threads" -v sc="$speedup_col" -v tc="$threads_col" \
        '$1=="MPI" && $tc==t && $sc>0 {eff=($sc/t)*100; sum+=eff; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')
    
    omp_eff=$(tail -n +2 "$LATEST_CSV" | \
        awk -F',' -v t="$threads" -v sc="$speedup_col" -v tc="$threads_col" \
        '$1=="OpenMP" && $tc==t && $sc>0 {eff=($sc/t)*100; sum+=eff; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')
    
    printf "%-7s | %-10s | %s\n" "$threads" "$mpi_eff" "$omp_eff"
done

echo ""
echo "=== ANÁLISE CONCLUÍDA ==="
echo "Arquivo: $LATEST_CSV"