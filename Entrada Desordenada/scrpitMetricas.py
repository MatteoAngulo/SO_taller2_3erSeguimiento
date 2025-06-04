import subprocess
import time
import os
import resource
import csv
import matplotlib.pyplot as plt

def ejecutar_programa(codigo_c, entrada, num_repeticiones=50):
    ejecutable = "./programa"
    compilacion = subprocess.run(["gcc", "-pthread", "-o", ejecutable, codigo_c])
    if compilacion.returncode != 0:
        raise RuntimeError("Fallo al compilar el programa en C")

    resultados = []

    for i in range(num_repeticiones):
        inicio = time.time()
        uso_inicio = resource.getrusage(resource.RUSAGE_CHILDREN)

        with open(entrada, 'r') as f:
            proceso = subprocess.run([ejecutable], stdin=f, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        fin = time.time()
        uso_fin = resource.getrusage(resource.RUSAGE_CHILDREN)

        tiempo_total = fin - inicio
        tiempo_usuario = uso_fin.ru_utime - uso_inicio.ru_utime
        tiempo_sistema = uso_fin.ru_stime - uso_inicio.ru_stime

        salida = proceso.stdout.decode()
        lineas_procesadas = len(salida.splitlines())
        throughput = lineas_procesadas / tiempo_total if tiempo_total > 0 else 0

        porcentaje_hilos = (tiempo_usuario + tiempo_sistema) / tiempo_total / os.cpu_count() * 100

        resultados.append({
            "iteración": i + 1,
            "latencia": tiempo_total,
            "throughput": throughput,
            "usuario": tiempo_usuario,
            "sistema": tiempo_sistema,
            "porcentaje_hilos": porcentaje_hilos
        })

    # Guardar en CSV
    with open("resultados.csv", mode="w", newline='') as archivo_csv:
        campos = ["iteración", "latencia", "throughput", "usuario", "sistema", "porcentaje_hilos"]
        writer = csv.DictWriter(archivo_csv, fieldnames=campos)
        writer.writeheader()
        for r in resultados:
            writer.writerow(r)

    # Ordenar por menor latencia
    mejores = sorted(resultados, key=lambda x: x["latencia"])

    # Mostrar resumen
    print("\n📊 Resultados globales (50 iteraciones):")
    latencias = [r["latencia"] for r in resultados]
    print(f"Latencia promedio: {sum(latencias)/len(latencias):.6f} s")

    throughputs = [r["throughput"] for r in resultados]
    print(f"Throughput promedio: {sum(throughputs)/len(throughputs):.2f} líneas/s")

    usuario_prom = sum(r["usuario"] for r in resultados) / len(resultados)
    sistema_prom = sum(r["sistema"] for r in resultados) / len(resultados)
    print(f"Tiempo de usuario promedio: {usuario_prom:.6f} s")
    print(f"Tiempo de sistema promedio: {sistema_prom:.6f} s")

    print(f"\n✅ Mejor ejecución: Iteración {mejores[0]['iteración']} con latencia {mejores[0]['latencia']:.6f} s")

    print("\n🏆 Top 5 mejores ejecuciones (por menor latencia):")
    for r in mejores[:5]:
        print(f"- Iteración {r['iteración']}: {r['latencia']:.6f}s | Throughput: {r['throughput']:.2f} líneas/s | % Hilos: {r['porcentaje_hilos']:.2f}%")

    # Gráficas
    iteraciones = [r["iteración"] for r in resultados]
    latencias = [r["latencia"] for r in resultados]
    throughputs = [r["throughput"] for r in resultados]
    hilos = [r["porcentaje_hilos"] for r in resultados]

    plt.figure(figsize=(12, 6))
    plt.subplot(1, 3, 1)
    plt.plot(iteraciones, latencias, marker='o')
    plt.title("Latencia por iteración")
    plt.xlabel("Iteración")
    plt.ylabel("Segundos")

    plt.subplot(1, 3, 2)
    plt.plot(iteraciones, throughputs, marker='s', color='green')
    plt.title("Throughput por iteración")
    plt.xlabel("Iteración")
    plt.ylabel("Líneas/seg")

    plt.subplot(1, 3, 3)
    plt.plot(iteraciones, hilos, marker='^', color='red')
    plt.title("% Uso de hilos por iteración")
    plt.xlabel("Iteración")
    plt.ylabel("Porcentaje")

    plt.tight_layout()
    plt.savefig("graficas_resultados.png")
    plt.show()

# Uso
if __name__ == "__main__":
    ejecutar_programa("programa.c", "entrada.txt", num_repeticiones=50)
