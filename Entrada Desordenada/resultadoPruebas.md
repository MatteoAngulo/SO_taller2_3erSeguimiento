- **Programas analizados**: 4
- **Configuraciones por programa**: 3

## Tabla 

| Programa | Config | Latencia (s) | Throughput (ops/s) | CPU (%) | Memoria (MB) |
|----------|---------|--------------|-------------------|---------|---------------|
| mantenimientoDeTeslas.c | 10C-5E-3CPE | 0.1122 | 900.5 | 4.8 | 0.0 |
| mantenimientoDeTeslas.c | 100C-5E-3CPE | 0.1126 | 8895.2 | 15.3 | 0.0 |
| mantenimientoDeTeslas.c | 500C-5E-3CPE | 0.1334 | 40222.9 | 68.2 | 0.2 |
| mantenimientoDeTeslasBarrera.c | 10C-5E-3CPE | 4.0334 | 25.0 | 0.2 | 3.1 |
| mantenimientoDeTeslasBarrera.c | 100C-5E-3CPE | 30.7972 | 402.7 | 1.2 | 29.7 |
| mantenimientoDeTeslasBarrera.c | 500C-5E-3CPE | 148.5519 | 2210.8 | 6.7 | 33.6 |
| mantenimientoDeTeslasCondicion.c | 10C-5E-3CPE | 0.1115 | 906.6 | 4.4 | 0.0 |
| mantenimientoDeTeslasCondicion.c | 100C-5E-3CPE | 0.1107 | 9041.5 | 15.0 | 0.0 |
| mantenimientoDeTeslasCondicion.c | 500C-5E-3CPE | 0.1130 | 44376.1 | 53.9 | 0.0 |
| mantenimientoDeTeslasEspera.c | 10C-5E-3CPE | 4.6182 | 22.9 | 0.1 | 3.4 |
| mantenimientoDeTeslasEspera.c | 100C-5E-3CPE | 30.4078 | 33950.9 | 74.1 | 18.5 |
| mantenimientoDeTeslasEspera.c | 500C-5E-3CPE | 150.0584 | 174720.4 | 347.7 | 19.9 |

## Mejores Rendimientos

### Mejor Latencia (menor tiempo)
- **Programa**: mantenimientoDeTeslasCondicion.c
- **Configuración**: 100C-5E-3CPE
- **Latencia**: 0.110722 segundos

### Mejor Throughput (mayor ops/s)
- **Programa**: mantenimientoDeTeslasEspera.c
- **Configuración**: 500C-5E-3CPE
- **Throughput**: 174720.43 ops/s

