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
    QUERY_RESPONSE_ERROR_ARCHIVO_NO_ENCONTRADO,
    QUERY_RESPONSE_ERROR_LECTURA_INVALIDA,
    QUERY_RESPONSE_ERROR_WORKER_DESCONECTADO,
    QUERY_RESPONSE_ERROR_QUERY_DESCONECTADO,
    QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION,
    
    // MASTER → WORKER
    WORKER_ASSIGN_QUERY,    // Asignar query a worker
    WORKER_DESALOJO,        // Pedir desalojo de query
    
    // WORKER → MASTER

    WORKER_ID,              // Enviar ID
    WORKER_PC_UPDATE,       //Envio nuevo PC, el queryID del que pertenece y el nombreDelQuery (informo el nuevo pc tras una interrupcion)

    WORKER_READ_RESULT,     // Resultado de lectura
    WORKER_QUERY_END,       // Query terminada en worker
    WORKER_ID_INTERRUPT,
    
    // WORKER ↔ STORAGE
    STORAGE_GET_BLOCK_SIZE, // Solicitar tamaño de bloque
    STORAGE_CREATE_FILE,
    STORAGE_READ_BLOCK,
    STORAGE_WRITE_BLOCK,
    STORAGE_RESPONSE,
    STORAGE_SEND_BLOCK_SIZE,      // Respuesta generica de Storage //Preguntar
    STORAGE_TRUNCATE, 
    STORAGE_TAG,
    STORAGE_COMMIT, 
    STORAGE_FLUSH,
    STORAGE_DELETE,

    STORAGE_SEND_OK,
    STORAGE_SEND_ERROR

} op_code;

#endif /* PROTOCOLO_H_ */