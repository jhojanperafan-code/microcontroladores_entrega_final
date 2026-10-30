# 03 - Arquitectura Dinámica
## Sistema SDN: Controller y Router — Grupo 2, Bellman-Ford

**Versión:** 1.0  
**Estado:** Propuesta para revisión  
**Diagramas obligatorios:** 3, en notación Mermaid.

---

## 1. Objetivo

Describir las interacciones temporales entre cliente, Router, Controller, servicios y DAO en los tres escenarios exigidos: reenvío exitoso, fallo/recuperación de enlace e indisponibilidad del Controller.

Los participantes internos utilizados aquí corresponden a clases/componentes definidos en `02-static-architecture.md`. Los endpoints y respuestas corresponden a `04-api-contract.md`.

## 2. Secuencia 1 — Reenvío exitoso de un paquete

### 2.1 Objetivo y participantes

Representar el flujo desde `POST /packets` hasta la respuesta con la ruta y traza. Participan: Cliente, `PacketAPIController`, `PacketView`, `ForwardingService`, `ControllerHTTPClient`, `RouteAPIController`, `RouteView`, `RoutingService`, `TopologyDAO`, `BellmanFordEngine` y `ForwardingLogDAO`.

### 2.2 Diagrama

```mermaid
sequenceDiagram
    autonumber
    actor Client as Cliente
    participant PAC as PacketAPIController
    participant PV as PacketView
    participant FS as ForwardingService
    participant HC as ControllerHTTPClient
    participant RAC as RouteAPIController
    participant RV as RouteView
    participant RS as RoutingService
    participant TD as TopologyDAO
    participant BF as BellmanFordEngine
    participant LD as ForwardingLogDAO

    Client->>PAC: POST /packets (PacketRequest)
    PAC->>PV: validate_request(body)
    PV-->>PAC: Packet validado
    PAC->>FS: forward(packet)
    FS->>HC: get_route(src,dst,metric)
    HC->>RAC: GET /route?src=A&dst=D&metric=delay
    RAC->>RV: validate_query(params)
    RV-->>RAC: Consulta válida
    RAC->>RS: calculate_route(A,D,delay)
    RS->>TD: get_snapshot()
    TD-->>RS: Topology + version
    RS->>BF: calculate(graph,A,D,delay)
    Note over BF: Bellman-Ford relaja aristas y detecta<br/>ciclos negativos alcanzables
    BF-->>RS: Camino mínimo y costo
    RS-->>RAC: Route
    RAC-->>HC: HTTP 200 + RouteResponse
    HC-->>FS: Ruta del Controller
    Note over FS: Simula los saltos de la ruta recibida;<br/>no calcula ni modifica el camino
    FS->>LD: append_result(ForwardingResult)
    LD-->>FS: Registro confirmado
    FS-->>PAC: Resultado y traza
    PAC-->>Client: HTTP 200 + PacketResponse
```

### 2.3 Descripción del flujo

1. Cliente envía `POST /packets` con `src`, `dst`, `metric`, `size_bytes` y opcionalmente `packet_id`.
2. `PacketAPIController` valida mediante `PacketView`.
3. `ForwardingService` solicita ruta a `ControllerHTTPClient`.
4. El cliente HTTP llama `GET /route` al Controller.
5. `RouteAPIController` valida parámetros mediante `RouteView` y delega en `RoutingService`.
6. `RoutingService` obtiene una instantánea coherente desde `TopologyDAO`.
7. `BellmanFordEngine` calcula el camino mínimo para la métrica seleccionada y comprueba ciclos negativos alcanzables.
8. Controller devuelve `RouteResponse` con camino, costo, unidad y versión.
9. `ForwardingService` simula cada salto de la ruta recibida sin recalcularla.
10. El servicio registra el resultado mediante `ForwardingLogDAO`.
11. Router devuelve `PacketResponse` con estado `FORWARDED` y traza.

### 2.4 Resultado esperado

Para una ruta `A -> B -> D`, la traza contiene los saltos `A -> B` y `B -> D`, en ese orden, ambos con estado `FORWARDED`.

## 3. Secuencia 2 — Fallo y recuperación de un enlace

### 3.1 Objetivo y participantes

Mostrar que un enlace deshabilitado queda excluido de consultas posteriores y que su restauración vuelve a habilitarlo. Participan Administrador, `LinkAPIController`, `LinkManagementService`, `TopologyDAO`, Cliente, `PacketAPIController`, `ForwardingService`, `ControllerHTTPClient`, `RouteAPIController`, `RoutingService`, `BellmanFordEngine` y `ForwardingLogDAO`.

### 3.2 Diagrama

```mermaid
sequenceDiagram
    autonumber
    actor Admin as Administrador
    actor Client as Cliente
    participant LAC as LinkAPIController
    participant LMS as LinkManagementService
    participant TD as TopologyDAO
    participant PAC as PacketAPIController
    participant FS as ForwardingService
    participant HC as ControllerHTTPClient
    participant RAC as RouteAPIController
    participant RS as RoutingService
    participant BF as BellmanFordEngine
    participant LD as ForwardingLogDAO

    Admin->>LAC: POST /links/fail {link_id: L2}
    LAC->>LMS: fail_link(L2)
    LMS->>TD: get_snapshot()
    TD-->>LMS: Topología actual + versión
    Note over LMS: enabled=false; incrementar versión<br/>solo si el estado cambió
    LMS->>TD: save_topology(topology)
    TD-->>LMS: Persistencia confirmada
    LMS-->>LAC: LinkStateResponse
    LAC-->>Admin: HTTP 200

    Client->>PAC: POST /packets
    PAC->>FS: forward(packet)
    FS->>HC: get_route(src,dst,metric)
    HC->>RAC: GET /route
    RAC->>RS: calculate_route(src,dst,metric)
    RS->>TD: get_snapshot()
    TD-->>RS: Topología con L2 deshabilitado
    RS->>BF: calculate(graph,src,dst,metric)
    BF-->>RS: Ruta alternativa o PathNotFound
    alt Existe ruta alternativa
        RS-->>RAC: Route alternativa
        RAC-->>HC: HTTP 200 + RouteResponse
        HC-->>FS: Ruta alternativa
        FS->>LD: append_result(FORWARDED)
        LD-->>FS: Registro confirmado
        FS-->>PAC: Resultado con ruta alternativa
        PAC-->>Client: HTTP 200 + FORWARDED
    else No existe ruta alternativa
        RS-->>RAC: PathNotFound
        RAC-->>HC: HTTP 404 + PATH_NOT_FOUND
        HC-->>FS: Error de camino inexistente
        FS->>LD: append_result(DROPPED)
        LD-->>FS: Registro confirmado
        FS-->>PAC: Resultado sin ruta
        PAC-->>Client: HTTP 404 + PATH_NOT_FOUND
    end

    Admin->>LAC: POST /links/restore {link_id: L2}
    LAC->>LMS: restore_link(L2)
    LMS->>TD: get_snapshot()
    TD-->>LMS: Topología actual + versión
    Note over LMS: enabled=true; incrementar versión<br/>solo si el estado cambió
    LMS->>TD: save_topology(topology)
    TD-->>LMS: Persistencia confirmada
    LMS-->>LAC: LinkStateResponse
    LAC-->>Admin: HTTP 200
    Client->>RAC: GET /route?src=A&dst=D&metric=delay
    RAC->>RS: calculate_route(A,D,delay)
    RS->>TD: get_snapshot()
    TD-->>RS: Topología con L2 restaurado
    RS->>BF: calculate(graph,A,D,delay)
    BF-->>RS: Ruta actualizada
    RS-->>RAC: Route
    RAC-->>Client: HTTP 200 + RouteResponse
```

### 3.3 Descripción del flujo

1. El administrador solicita deshabilitar `L2` mediante `POST /links/fail`.
2. `LinkManagementService` obtiene la topología, verifica el enlace y cambia su estado administrativo.
3. Si el estado cambia, incrementa la versión y persiste mediante `TopologyDAO`.
4. Una solicitud posterior de paquete provoca una nueva consulta de ruta al Controller.
5. `RoutingService` obtiene una nueva instantánea; Bellman-Ford excluye el enlace deshabilitado.
6. Si hay camino alternativo, Router lo simula y registra `FORWARDED`; si no, registra `DROPPED` y devuelve `PATH_NOT_FOUND`.
7. El administrador restaura el enlace mediante `POST /links/restore`.
8. El Controller persiste el nuevo estado y aumenta versión solo si cambió.
9. Las consultas posteriores pueden considerar nuevamente el enlace, siempre que sea utilizable por la métrica.

### 3.4 Resultados esperados

- Con ruta alternativa: `HTTP 200`, estado `FORWARDED` y camino que no contiene el enlace fallido.
- Sin ruta alternativa: `HTTP 404`, código `PATH_NOT_FOUND`, resultado `DROPPED` registrado.
- Después de restaurar: las siguientes consultas usan la topología actualizada.

## 4. Secuencia 3 — Controller no disponible

### 4.1 Objetivo y participantes

Mostrar el comportamiento del Router ante error de conexión o timeout. Participan Cliente, `PacketAPIController`, `ForwardingService`, `ControllerHTTPClient` y `ForwardingLogDAO`.

### 4.2 Diagrama

```mermaid
sequenceDiagram
    autonumber
    actor Client as Cliente
    participant PAC as PacketAPIController
    participant FS as ForwardingService
    participant HC as ControllerHTTPClient
    participant LD as ForwardingLogDAO

    Client->>PAC: POST /packets
    PAC->>FS: forward(packet)
    FS->>HC: get_route(src,dst,metric)
    HC->>HC: Solicitud HTTP con timeout de 2 s
    alt Error de conexión
        HC-->>FS: ControllerUnavailable
    else Timeout
        HC-->>FS: ControllerTimeout
    end
    Note over FS: No calcula ruta localmente;<br/>construye resultado FAILED
    FS->>LD: append_result(ForwardingResult FAILED)
    LD-->>FS: Registro confirmado
    FS-->>PAC: ControllerUnavailable
    PAC-->>Client: HTTP 503 + CONTROLLER_UNAVAILABLE
```

### 4.3 Descripción del flujo

1. El cliente envía `POST /packets`.
2. `PacketAPIController` valida la solicitud y llama a `ForwardingService`.
3. `ForwardingService` usa `ControllerHTTPClient` para pedir la ruta.
4. Se produce un error de conexión o se supera el timeout configurado de 2 segundos.
5. El servicio no intenta calcular una ruta localmente; construye un resultado `FAILED`.
6. Se intenta registrar el resultado mediante `ForwardingLogDAO`.
7. Router responde `HTTP 503` con código `CONTROLLER_UNAVAILABLE` y permanece activo.

### 4.4 Resultado esperado

La indisponibilidad del Controller no se confunde con una ruta inexistente. Respuesta: HTTP 503, código `CONTROLLER_UNAVAILABLE`, estado `FAILED`, sin ruta y sin reintentos automáticos en esta versión.

## 5. Correspondencia con arquitectura estática y API

| Participante | Clase definida en `02-static-architecture.md` |
|---|---|
| `PacketAPIController` | Sí |
| `PacketView` | Sí |
| `ForwardingService` | Sí |
| `ControllerHTTPClient` | Sí |
| `RouteAPIController` | Sí |
| `RouteView` | Sí |
| `RoutingService` | Sí |
| `TopologyDAO` | Sí |
| `BellmanFordEngine` | Sí |
| `LinkAPIController` | Sí |
| `LinkManagementService` | Sí |
| `ForwardingLogDAO` | Sí |

Los actores Cliente y Administrador son externos. Endpoints usados: `POST /packets`, `GET /route`, `POST /links/fail` y `POST /links/restore`.

## 6. Reglas dinámicas

1. Cada consulta utiliza una instantánea coherente de topología.
2. La versión en `RouteResponse` corresponde a la instantánea usada en el cálculo.
3. No se usa caché de rutas en esta versión; así se evita invalidación tras fallos/restauraciones.
4. La modificación del estado de enlaces se serializa para evitar actualizaciones concurrentes incoherentes.
5. Router simula exactamente la ruta recibida y no la modifica.
6. Los resultados fallidos se registran mediante DAO.
7. Los errores de comunicación no terminan el proceso Router.
8. Las respuestas coinciden con el contrato REST.

## 7. Revisión y aprobación

- [ ] Todas las clases participantes aparecen en el diagrama de clases.
- [ ] Los métodos y endpoints corresponden al contrato API.
- [ ] Los servicios contienen la lógica de negocio.
- [ ] El almacenamiento se realiza mediante DAO.
- [ ] Router no calcula rutas localmente.

**Revisión Estudiante A:** pendiente.  
**Revisión Estudiante B:** pendiente.  
**Congelamiento:** pendiente de aprobación conjunta.
