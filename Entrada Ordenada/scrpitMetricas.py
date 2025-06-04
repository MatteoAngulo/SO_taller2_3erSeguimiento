import subprocess 
import time
import os
import resource
import psutil
import threading
import glob
import pandas as pd
from datetime import datetime
from collections import defaultdict

def escribir_configuracion(nombre_archivo, carros, estaciones, carros_por_estacion):
    with open(nombre_archivo, "w") as archivo:
        archivo.write(f"{carros}\n{estaciones}\n{carros_por_estacion}\n")

def monitorear_recursos(pid, metricas, stop_event):
    """Monitorea CPU, memoria y otros recursos del proceso"""
    try:
        proceso = psutil.Process(pid)
        while not stop_event.is_set():
            try:
                cpu_percent = proceso.cpu_percent()
                memoria_info = proceso.memory_info()
                memoria_mb = memoria_info.rss / 1024 / 1024
                
                metricas['cpu_samples'].append(cpu_percent)
                metricas['memoria_samples'].append(memoria_mb)
                metricas['num_threads'].append(proceso.num_threads())
                
                time.sleep(0.1)  # Muestreo cada 100ms
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                break
    except psutil.NoSuchProcess:
        pass

def ejecutar_programa(codigo_c, archivo_config, nombre_programa):
    """Ejecuta un programa C específico y retorna sus métricas"""
    ejecutable = f"./ejecutable_{nombre_programa}"
    compilacion = subprocess.run(["gcc", "-pthread", codigo_c, "-o", ejecutable])
    if compilacion.returncode != 0:
        raise RuntimeError(f"Fallo al compilar el programa {codigo_c}")

    # Preparar métricas de monitoreo
    metricas = {
        'cpu_samples': [],
        'memoria_samples': [],
        'num_threads': []
    }
    
    inicio = time.time()
    uso_inicio = resource.getrusage(resource.RUSAGE_CHILDREN)

    # Ejecutar proceso
    proceso = subprocess.Popen([ejecutable, archivo_config], 
                              stdout=subprocess.PIPE, 
                              stderr=subprocess.PIPE)
    
    # Iniciar monitoreo de recursos
    stop_event = threading.Event()
    monitor_thread = threading.Thread(
        target=monitorear_recursos, 
        args=(proceso.pid, metricas, stop_event)
    )
    monitor_thread.start()
    
    # Esperar a que termine el proceso
    salida, error = proceso.communicate()
    
    # Detener monitoreo
    stop_event.set()
    monitor_thread.join()
    
    fin = time.time()
    uso_fin = resource.getrusage(resource.RUSAGE_CHILDREN)

    # Limpiar ejecutable temporal
    if os.path.exists(ejecutable):
        os.remove(ejecutable)

    # Calcular métricas básicas
    tiempo_total = fin - inicio  # Latencia (wall-clock time)
    tiempo_usuario = uso_fin.ru_utime - uso_inicio.ru_utime
    tiempo_sistema = uso_fin.ru_stime - uso_inicio.ru_stime
    tiempo_cpu_total = tiempo_usuario + tiempo_sistema

    # Procesar salida para throughput
    salida_decodificada = salida.decode()
    lineas_procesadas = len(salida_decodificada.splitlines())
    
    # Throughput: operaciones por segundo
    throughput = lineas_procesadas / tiempo_total if tiempo_total > 0 else 0
    
    # Utilización de CPU
    porcentaje_cpu = (tiempo_cpu_total / tiempo_total) * 100
    
    # Métricas adicionales del monitoreo
    cpu_promedio = sum(metricas['cpu_samples']) / len(metricas['cpu_samples']) if metricas['cpu_samples'] else 0
    memoria_promedio = sum(metricas['memoria_samples']) / len(metricas['memoria_samples']) if metricas['memoria_samples'] else 0
    memoria_maxima = max(metricas['memoria_samples']) if metricas['memoria_samples'] else 0
    threads_promedio = sum(metricas['num_threads']) / len(metricas['num_threads']) if metricas['num_threads'] else 0
    
    return {
        'latencia': tiempo_total,
        'throughput': throughput,
        'cpu_utilizacion': porcentaje_cpu,
        'cpu_promedio_monitor': cpu_promedio,
        'memoria_promedio_mb': memoria_promedio,
        'memoria_maxima_mb': memoria_maxima,
        'threads_promedio': threads_promedio,
        'tiempo_usuario': tiempo_usuario,
        'tiempo_sistema': tiempo_sistema,
        'lineas_procesadas': lineas_procesadas
    }

def calcular_estadisticas(valores):
    """Calcula estadísticas básicas de una lista de valores"""
    if not valores:
        return {'promedio': 0, 'min': 0, 'max': 0, 'desviacion': 0}
    
    promedio = sum(valores) / len(valores)
    minimo = min(valores)
    maximo = max(valores)
    
    # Desviación estándar
    varianza = sum((x - promedio) ** 2 for x in valores) / len(valores)
    desviacion = varianza ** 0.5
    
    return {
        'promedio': promedio,
        'min': minimo,
        'max': maximo,
        'desviacion': desviacion
    }

def obtener_programas_c(directorio="."):
    """Obtiene todos los archivos .c del directorio especificado"""
    patron = os.path.join(directorio, "*.c")
    archivos_c = glob.glob(patron)
    return [os.path.basename(archivo) for archivo in archivos_c]

def mostrar_tabla_detallada_programa(resultados, nombre_programa):
    print(f"\n" + "="*120)
    print(f"RESULTADOS DETALLADOS DEL BENCHMARK - PROGRAMA: {nombre_programa}")
    print("="*120)
    
    for r in resultados:
        print(f"\nConfiguración: {r['carros']} carros, {r['estaciones']} estaciones, {r['carros_por_estacion']} carros/estación")
        print("-" * 80)
        
        # Latencia
        lat_stats = r['latencia_stats']
        print(f"LATENCIA (segundos):")
        print(f"  Promedio: {lat_stats['promedio']:.6f}s")
        print(f"  Mínimo:   {lat_stats['min']:.6f}s")
        print(f"  Máximo:   {lat_stats['max']:.6f}s")
        print(f"  Desv.Est: {lat_stats['desviacion']:.6f}s")
        
        # Throughput
        thr_stats = r['throughput_stats']
        print(f"\nTHROUGHPUT (operaciones/segundo):")
        print(f"  Promedio: {thr_stats['promedio']:.2f} ops/s")
        print(f"  Mínimo:   {thr_stats['min']:.2f} ops/s")
        print(f"  Máximo:   {thr_stats['max']:.2f} ops/s")
        print(f"  Desv.Est: {thr_stats['desviacion']:.2f} ops/s")
        
        # CPU
        cpu_stats = r['cpu_stats']
        print(f"\nUTILIZACIÓN CPU (%):")
        print(f"  Promedio: {cpu_stats['promedio']:.2f}%")
        print(f"  Mínimo:   {cpu_stats['min']:.2f}%")
        print(f"  Máximo:   {cpu_stats['max']:.2f}%")
        
        # Memoria
        mem_stats = r['memoria_stats']
        print(f"\nMEMORIA (MB):")
        print(f"  Promedio: {mem_stats['promedio']:.2f} MB")
        print(f"  Máximo:   {mem_stats['max']:.2f} MB")
        
        # Threads
        thread_stats = r['threads_stats']
        print(f"\nTHREADS:")
        print(f"  Promedio: {thread_stats['promedio']:.1f}")

def mostrar_tabla_resumen_programa(resultados, nombre_programa):
    print(f"\n" + "="*100)
    print(f"TABLA RESUMEN - PROGRAMA: {nombre_programa}")
    print("="*100)
    print(f"{'Config':<15}{'Latencia (s)':<15}{'Throughput':<15}{'CPU %':<10}{'Memoria (MB)':<15}{'Threads':<10}")
    print("-" * 100)
    
    for r in resultados:
        config = f"{r['carros']}C-{r['estaciones']}E-{r['carros_por_estacion']}CPE"
        latencia = f"{r['latencia_stats']['promedio']:.4f}"
        throughput = f"{r['throughput_stats']['promedio']:.1f}"
        cpu = f"{r['cpu_stats']['promedio']:.1f}"
        memoria = f"{r['memoria_stats']['promedio']:.1f}"
        threads = f"{r['threads_stats']['promedio']:.1f}"
        
        print(f"{config:<15}{latencia:<15}{throughput:<15}{cpu:<10}{memoria:<15}{threads:<10}")

def mostrar_comparacion_programas(todos_los_resultados):
    """Muestra una tabla comparativa entre todos los programas"""
    print(f"\n" + "="*150)
    print("COMPARACIÓN ENTRE PROGRAMAS")
    print("="*150)
    print(f"{'Programa':<20}{'Config':<15}{'Latencia (s)':<15}{'Throughput':<15}{'CPU %':<10}{'Memoria (MB)':<15}{'Threads':<10}")
    print("-" * 150)
    
    for nombre_programa, resultados in todos_los_resultados.items():
        for i, r in enumerate(resultados):
            config = f"{r['carros']}C-{r['estaciones']}E-{r['carros_por_estacion']}CPE"
            latencia = f"{r['latencia_stats']['promedio']:.4f}"
            throughput = f"{r['throughput_stats']['promedio']:.1f}"
            cpu = f"{r['cpu_stats']['promedio']:.1f}"
            memoria = f"{r['memoria_stats']['promedio']:.1f}"
            threads = f"{r['threads_stats']['promedio']:.1f}"
            
            # Solo mostrar el nombre del programa en la primera fila
            programa_display = nombre_programa if i == 0 else ""
            print(f"{programa_display:<20}{config:<15}{latencia:<15}{throughput:<15}{cpu:<10}{memoria:<15}{threads:<10}")
        
        if resultados:  # Agregar separador entre programas
            print("-" * 150)

if __name__ == "__main__":
    # Configuraciones a probar (nhilos = carros)
    configuraciones = [
        (10, 3, 6),      
        (100, 6, 3),     
        (1000, 10, 8),   
    ]
    
    repeticiones = 5
    directorio_programas = "."  # Cambia esto por la ruta de tu carpeta si es diferente
    
    # Obtener todos los programas C
    programas_c = obtener_programas_c(directorio_programas)
    
    if not programas_c:
        print(f"No se encontraron archivos .c en el directorio: {directorio_programas}")
        exit(1)
    
    print(f"Programas C encontrados: {programas_c}")
    print(f"Iniciando benchmark con {repeticiones} repeticiones por configuración")
    print(f"CPU cores disponibles: {os.cpu_count()}")
    
    # Diccionario para almacenar resultados de todos los programas
    todos_los_resultados = {}

    # Ejecutar benchmark para cada programa
    for programa_c in programas_c:
        nombre_programa = os.path.splitext(programa_c)[0]  # Quitar extensión .c
        print(f"\n" + "="*80)
        print(f"INICIANDO BENCHMARK PARA: {programa_c}")
        print("="*80)
        
        resultados_finales = []
        
        for carros, estaciones, carros_por_estacion in configuraciones:
            print(f"\nEjecutando configuración: {carros} carros (hilos), {estaciones} estaciones, {carros_por_estacion} carros/estación")
            escribir_configuracion("mantenimientoConfig.txt", carros, estaciones, carros_por_estacion)

            # Listas para almacenar resultados de cada repetición
            resultados_repeticiones = []

            for i in range(repeticiones):
                print(f"  Repetición {i+1}/{repeticiones}...", end=' ')
                try:
                    resultado = ejecutar_programa(programa_c, "mantenimientoConfig.txt", nombre_programa)
                    resultados_repeticiones.append(resultado)
                    print(f"Latencia: {resultado['latencia']:.4f}s, Throughput: {resultado['throughput']:.1f} ops/s")
                except Exception as e:
                    print(f"Error: {e}")
                    continue

            if not resultados_repeticiones:
                print(f"  ERROR: No se pudieron obtener resultados para esta configuración")
                continue

            # Calcular estadísticas
            latencias = [r['latencia'] for r in resultados_repeticiones]
            throughputs = [r['throughput'] for r in resultados_repeticiones]
            cpus = [r['cpu_utilizacion'] for r in resultados_repeticiones]
            memorias = [r['memoria_promedio_mb'] for r in resultados_repeticiones]
            threads = [r['threads_promedio'] for r in resultados_repeticiones]

            resultado_config = {
                'carros': carros,
                'estaciones': estaciones,
                'carros_por_estacion': carros_por_estacion,
                'latencia_stats': calcular_estadisticas(latencias),
                'throughput_stats': calcular_estadisticas(throughputs),
                'cpu_stats': calcular_estadisticas(cpus),
                'memoria_stats': calcular_estadisticas(memorias),
                'threads_stats': calcular_estadisticas(threads)
            }
            
            resultados_finales.append(resultado_config)

        # Guardar resultados del programa actual
        todos_los_resultados[programa_c] = resultados_finales
        
        # Mostrar resultados individuales del programa
        if resultados_finales:
            mostrar_tabla_detallada_programa(resultados_finales, programa_c)
            mostrar_tabla_resumen_programa(resultados_finales, programa_c)
            print(f"\nBenchmark de {programa_c} completado. Configuraciones probadas: {len(resultados_finales)}")
        else:
            print(f"\nNo se pudieron obtener resultados para {programa_c}")

    # Mostrar comparación final entre todos los programas
    if todos_los_resultados:
        mostrar_comparacion_programas(todos_los_resultados)
        print(f"\n" + "="*80)
        print(f"BENCHMARK COMPLETADO PARA TODOS LOS PROGRAMAS")
        print(f"Total de programas analizados: {len(todos_los_resultados)}")
        print("="*80)
    
def exportar_a_excel_csv(todos_los_resultados):
    """Exporta todos los resultados a archivos Excel y CSV organizados"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Crear DataFrames para diferentes tipos de datos
    
    # 1. Resumen General - Una fila por programa y configuración
    datos_resumen = []
    for programa, resultados in todos_los_resultados.items():
        for resultado in resultados:
            fila = {
                'Programa': programa,
                'Configuracion': f"{resultado['carros']}C-{resultado['estaciones']}E-{resultado['carros_por_estacion']}CPE",
                'Carros': resultado['carros'],
                'Estaciones': resultado['estaciones'],
                'Carros_por_Estacion': resultado['carros_por_estacion'],
                'Latencia_Promedio_s': resultado['latencia_stats']['promedio'],
                'Latencia_Min_s': resultado['latencia_stats']['min'],
                'Latencia_Max_s': resultado['latencia_stats']['max'],
                'Latencia_DesvEst_s': resultado['latencia_stats']['desviacion'],
                'Throughput_Promedio_ops_s': resultado['throughput_stats']['promedio'],
                'Throughput_Min_ops_s': resultado['throughput_stats']['min'],
                'Throughput_Max_ops_s': resultado['throughput_stats']['max'],
                'Throughput_DesvEst_ops_s': resultado['throughput_stats']['desviacion'],
                'CPU_Promedio_pct': resultado['cpu_stats']['promedio'],
                'CPU_Min_pct': resultado['cpu_stats']['min'],
                'CPU_Max_pct': resultado['cpu_stats']['max'],
                'CPU_DesvEst_pct': resultado['cpu_stats']['desviacion'],
                'Memoria_Promedio_MB': resultado['memoria_stats']['promedio'],
                'Memoria_Max_MB': resultado['memoria_stats']['max'],
                'Threads_Promedio': resultado['threads_stats']['promedio']
            }
            datos_resumen.append(fila)
    
    df_resumen = pd.DataFrame(datos_resumen)
    
    # 2. Comparación por Métrica - Tablas pivote
    # Latencia
    df_latencia = df_resumen.pivot(index='Configuracion', columns='Programa', values='Latencia_Promedio_s')
    
    # Throughput
    df_throughput = df_resumen.pivot(index='Configuracion', columns='Programa', values='Throughput_Promedio_ops_s')
    
    # CPU
    df_cpu = df_resumen.pivot(index='Configuracion', columns='Programa', values='CPU_Promedio_pct')
    
    # Memoria
    df_memoria = df_resumen.pivot(index='Configuracion', columns='Programa', values='Memoria_Promedio_MB')
    
    # 3. Ranking de Rendimiento
    datos_ranking = []
    for config in df_resumen['Configuracion'].unique():
        datos_config = df_resumen[df_resumen['Configuracion'] == config].copy()
        
        # Ranking por latencia (menor es mejor)
        datos_config_lat = datos_config.sort_values('Latencia_Promedio_s')
        for i, (_, row) in enumerate(datos_config_lat.iterrows()):
            datos_ranking.append({
                'Configuracion': config,
                'Programa': row['Programa'],
                'Metrica': 'Latencia',
                'Valor': row['Latencia_Promedio_s'],
                'Ranking': i + 1,
                'Unidad': 'segundos'
            })
        
        # Ranking por throughput (mayor es mejor)
        datos_config_thr = datos_config.sort_values('Throughput_Promedio_ops_s', ascending=False)
        for i, (_, row) in enumerate(datos_config_thr.iterrows()):
            datos_ranking.append({
                'Configuracion': config,
                'Programa': row['Programa'],
                'Metrica': 'Throughput',
                'Valor': row['Throughput_Promedio_ops_s'],
                'Ranking': i + 1,
                'Unidad': 'ops/s'
            })
        
        # Ranking por CPU (menor es mejor para eficiencia)
        datos_config_cpu = datos_config.sort_values('CPU_Promedio_pct')
        for i, (_, row) in enumerate(datos_config_cpu.iterrows()):
            datos_ranking.append({
                'Configuracion': config,
                'Programa': row['Programa'],
                'Metrica': 'CPU_Eficiencia',
                'Valor': row['CPU_Promedio_pct'],
                'Ranking': i + 1,
                'Unidad': '%'
            })
    
    df_ranking = pd.DataFrame(datos_ranking)
    
    # 4. Estadísticas por Programa
    datos_stats_programa = []
    for programa, resultados in todos_los_resultados.items():
        latencias = [r['latencia_stats']['promedio'] for r in resultados]
        throughputs = [r['throughput_stats']['promedio'] for r in resultados]
        cpus = [r['cpu_stats']['promedio'] for r in resultados]
        memorias = [r['memoria_stats']['promedio'] for r in resultados]
        
        datos_stats_programa.append({
            'Programa': programa,
            'Configuraciones_Probadas': len(resultados),
            'Latencia_Media_General': sum(latencias) / len(latencias) if latencias else 0,
            'Latencia_Min_General': min(latencias) if latencias else 0,
            'Latencia_Max_General': max(latencias) if latencias else 0,
            'Throughput_Media_General': sum(throughputs) / len(throughputs) if throughputs else 0,
            'Throughput_Min_General': min(throughputs) if throughputs else 0,
            'Throughput_Max_General': max(throughputs) if throughputs else 0,
            'CPU_Media_General': sum(cpus) / len(cpus) if cpus else 0,
            'Memoria_Media_General': sum(memorias) / len(memorias) if memorias else 0
        })
    
    df_stats_programa = pd.DataFrame(datos_stats_programa)
    
    # Exportar a Excel con múltiples hojas
    nombre_excel = f"benchmark_resultados_{timestamp}.xlsx"
    with pd.ExcelWriter(nombre_excel, engine='openpyxl') as writer:
        df_resumen.to_excel(writer, sheet_name='Resumen_Completo', index=False)
        df_latencia.to_excel(writer, sheet_name='Comparacion_Latencia')
        df_throughput.to_excel(writer, sheet_name='Comparacion_Throughput')
        df_cpu.to_excel(writer, sheet_name='Comparacion_CPU')
        df_memoria.to_excel(writer, sheet_name='Comparacion_Memoria')
        df_ranking.to_excel(writer, sheet_name='Rankings', index=False)
        df_stats_programa.to_excel(writer, sheet_name='Stats_por_Programa', index=False)
    
    # Exportar CSVs individuales
    df_resumen.to_csv(f"benchmark_resumen_{timestamp}.csv", index=False)
    df_latencia.to_csv(f"benchmark_latencia_{timestamp}.csv")
    df_throughput.to_csv(f"benchmark_throughput_{timestamp}.csv")
    df_cpu.to_csv(f"benchmark_cpu_{timestamp}.csv")
    df_memoria.to_csv(f"benchmark_memoria_{timestamp}.csv")
    df_ranking.to_csv(f"benchmark_rankings_{timestamp}.csv", index=False)
    df_stats_programa.to_csv(f"benchmark_stats_programa_{timestamp}.csv", index=False)
    
    print(f"\n" + "="*80)
    print("ARCHIVOS EXPORTADOS EXITOSAMENTE")
    print("="*80)
    print(f"📊 Excel principal: {nombre_excel}")
    print("📊 Archivos CSV generados:")
    print(f"   - benchmark_resumen_{timestamp}.csv")
    print(f"   - benchmark_latencia_{timestamp}.csv")
    print(f"   - benchmark_throughput_{timestamp}.csv")
    print(f"   - benchmark_cpu_{timestamp}.csv")
    print(f"   - benchmark_memoria_{timestamp}.csv")
    print(f"   - benchmark_rankings_{timestamp}.csv")
    print(f"   - benchmark_stats_programa_{timestamp}.csv")
    print("="*80)
    
    return nombre_excel

def generar_reporte_markdown(todos_los_resultados):
    """Genera un reporte en Markdown con los resultados"""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    nombre_archivo = f"benchmark_reporte_{datetime.now().strftime('%Y%m%d_%H%M%S')}.md"
    
    with open(nombre_archivo, 'w', encoding='utf-8') as f:
        f.write(f"# Reporte de Benchmark - {timestamp}\n\n")
        f.write(f"## Resumen Ejecutivo\n\n")
        f.write(f"- **Programas analizados**: {len(todos_los_resultados)}\n")
        f.write(f"- **Configuraciones por programa**: {len(list(todos_los_resultados.values())[0]) if todos_los_resultados else 0}\n")
        f.write(f"- **Fecha de ejecución**: {timestamp}\n\n")
        
        # Tabla resumen por programa
        f.write("## Tabla Resumen por Programa\n\n")
        f.write("| Programa | Config | Latencia (s) | Throughput (ops/s) | CPU (%) | Memoria (MB) |\n")
        f.write("|----------|---------|--------------|-------------------|---------|---------------|\n")
        
        for programa, resultados in todos_los_resultados.items():
            for resultado in resultados:
                config = f"{resultado['carros']}C-{resultado['estaciones']}E-{resultado['carros_por_estacion']}CPE"
                lat = f"{resultado['latencia_stats']['promedio']:.4f}"
                thr = f"{resultado['throughput_stats']['promedio']:.1f}"
                cpu = f"{resultado['cpu_stats']['promedio']:.1f}"
                mem = f"{resultado['memoria_stats']['promedio']:.1f}"
                f.write(f"| {programa} | {config} | {lat} | {thr} | {cpu} | {mem} |\n")
        
        # Mejores rendimientos
        f.write("\n## Mejores Rendimientos\n\n")
        
        # Encontrar mejores por cada métrica
        mejor_latencia = None
        mejor_throughput = None
        menor_latencia = float('inf')
        mayor_throughput = 0
        
        for programa, resultados in todos_los_resultados.items():
            for resultado in resultados:
                lat = resultado['latencia_stats']['promedio']
                thr = resultado['throughput_stats']['promedio']
                config = f"{resultado['carros']}C-{resultado['estaciones']}E-{resultado['carros_por_estacion']}CPE"
                
                if lat < menor_latencia:
                    menor_latencia = lat
                    mejor_latencia = (programa, config, lat)
                
                if thr > mayor_throughput:
                    mayor_throughput = thr
                    mejor_throughput = (programa, config, thr)
        
        if mejor_latencia:
            f.write(f"### Mejor Latencia (menor tiempo)\n")
            f.write(f"- **Programa**: {mejor_latencia[0]}\n")
            f.write(f"- **Configuración**: {mejor_latencia[1]}\n")
            f.write(f"- **Latencia**: {mejor_latencia[2]:.6f} segundos\n\n")
        
        if mejor_throughput:
            f.write(f"### Mejor Throughput (mayor ops/s)\n")
            f.write(f"- **Programa**: {mejor_throughput[0]}\n")
            f.write(f"- **Configuración**: {mejor_throughput[1]}\n")
            f.write(f"- **Throughput**: {mejor_throughput[2]:.2f} ops/s\n\n")
    
    print(f"📝 Reporte Markdown: {nombre_archivo}")
    return nombre_archivo