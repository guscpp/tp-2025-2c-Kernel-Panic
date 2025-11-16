#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

typedef enum
{
    // HANDSHAKES
    QC_HANDSHAKE,           // Query Control se presenta
    WORKER_HANDSHAKE,       // Worker se presenta  
    WORKER_INTERRUPTION_HANDSHAKE,
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
    QUERY_RESPONSE_ERROR_ERROR_EN_INSTRUCCION,
    QUERY_RESPONSE_ERROR_MODIFICAR_COMMIT,
    QUERY_RESPONSE_ERROR_TAMANIO_ESCRITURA_EXCEDIDO,
    QUERY_RESPONSE_ERROR_STORAGE_DESCONECTADO,
    
    // MASTER → WORKER
    WORKER_ASSIGN_QUERY,    // Asignar query a worker
    WORKER_DESALOJO,        // Pedir desalojo de query
    RETENER_WORKER,
    
    // WORKER → MASTER

    WORKER_ID,              // Enviar ID
    WORKER_PC_UPDATE,       //Envio nuevo PC, el queryID del que pertenece y el nombreDelQuery (informo el nuevo pc tras una interrupcion)
    WORKER_READ_RESULT,     // Resultado de lectura
    WORKER_QUERY_END,       // Query terminada en worker
    WORKER_ID_INTERRUPT,    //Handshake del segundo connect
    WORKER_ERROR_ARCHIVO,   //archivo no encontrado en storage
    WORKER_ERROR_INSTRUCCION_MALFORMADA,    
    WORKER_ERROR_MODIFICAR_COMMIT,
    WORKER_ERROR_TAMANIO_ESCRITURA_EXCEDIDO,
    WORKER_ERROR_TAMANIO_LECTURA_EXCEDIDO, 
    WORKER_ERROR_STORAGE_DESCONECTADO,  
    WORKER_ERROR_DIRECCION_INVALIDA, //para cuando el file quiere acceder a un bloque logico que no le corresponde
    WORKER_ERROR_QUERY_NO_ENCONTRADA, //el path que me pasa master no puede ejecutarse
    
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