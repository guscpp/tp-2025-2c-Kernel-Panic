#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

typedef enum
{
    // HANDSHAKES
    QC_HANDSHAKE,           // Query Control se presenta
    WORKER_HANDSHAKE,       // Worker se presenta  
    STORAGE_HANDSHAKE,      // Storage se presenta
    
    // QUERY CONTROL → MASTER
    QUERY_REQUEST,          // Envío de query (path + prioridad)
    
    // MASTER → QUERY CONTROL  
    QUERY_RESPONSE_READ,    // Envío de contenido leido
    QUERY_RESPONSE_END,     // Query finalizada exitosamente
    QUERY_RESPONSE_ERROR,   // Query finalizada con error
    
    // MASTER → WORKER
    WORKER_ASSIGN_QUERY,    // Asignar query a worker
    WORKER_DESALOJO,        // Pedir desalojo de query
    
    // WORKER → MASTER
    WORKER_READ_RESULT,     // Resultado de lectura
    WORKER_QUERY_END,       // Query terminada en worker
    
    // WORKER ↔ STORAGE
    STORAGE_GET_BLOCK_SIZE, // Solicitar tamaño de bloque
    STORAGE_CREATE_FILE,
    STORAGE_READ_BLOCK,
    STORAGE_WRITE_BLOCK,
    STORAGE_RESPONSE,
    STORAGE_SEND_BLOCK_SIZE        // Respuesta generica de Storage
} op_code;

#endif /* PROTOCOLO_H_ */