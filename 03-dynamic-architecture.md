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

**Diagrama visual:**

![Diagrama de secuencia 1: reenvío exitoso de un paquete](diagrams/03-secuencia-1-reenvio-exitoso.png)

<details>
<summary>Ver código Mermaid editable</summary>

```mermaid
sequenceDiagram
    hide footbox
    autonumber
    actor U as Cliente
    participant RAPI as Router API
    participant FWD as Forwarding Service
    participant HTTP as HTTP Client
    participant CAPI as Controller API
    participant ROUTE as Routing Service
    participant TDAO as Topology DAO
    participant BF as Bellman-Ford
    participant LOG as Forwarding Log DAO

    U->>RAPI: POST /packets
    RAPI->>FWD: forward(packet)
    FWD->>HTTP: get_route(src,dst,metric)
    HTTP->>CAPI: GET /route
    CAPI->>ROUTE: calculate_route(src,dst,metric)
    ROUTE->>TDAO: get_snapshot()
    TDAO-->>ROUTE: Topology y version
    ROUTE->>BF: calculate(graph,src,dst,metric)
    Note over BF: Calcula camino minimo y costo
    BF-->>ROUTE: Ruta y costo
    ROUTE-->>CAPI: Route
    CAPI-->>HTTP: HTTP 200 RouteResponse
    HTTP-->>FWD: Ruta seleccionada
    Note over FWD: Simula los saltos recibidos; no recalcula
    FWD->>LOG: append_result(result)
    LOG-->>FWD: Registro confirmado
    FWD-->>RAPI: Resultado y traza
    RAPI-->>U: HTTP 200 PacketResponse
```

</details>

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

**Diagrama visual:**

![Diagrama de secuencia 2: fallo y recuperación de un enlace](diagrams/03-secuencia-2-fallo-restauracion.png)

<details>
<summary>Ver código Mermaid editable</summary>

```mermaid
sequenceDiagram
    hide footbox
    autonumber
    actor ADM as Administrador
    actor U as Cliente
    participant LAPI as Link API
    participant LMS as Link Service
    participant TDAO as Topology DAO
    participant RAPI as Route API
    participant ROUTE as Routing Service
    participant BF as Bellman-Ford

    ADM->>LAPI: POST /links/fail
    LAPI->>LMS: fail_link(link_id)
    LMS->>TDAO: get_snapshot()
    TDAO-->>LMS: Topologia actual
    Note over LMS: Deshabilita el enlace y aumenta version
    LMS->>TDAO: save_topology(topology)
    TDAO-->>LMS: Persistencia confirmada
    LMS-->>LAPI: LinkStateResponse
    LAPI-->>ADM: HTTP 200

    U->>RAPI: GET /route
    RAPI->>ROUTE: calculate_route(src,dst,metric)
    ROUTE->>TDAO: get_snapshot()
    TDAO-->>ROUTE: Topologia con enlace deshabilitado
    ROUTE->>BF: calculate(graph,src,dst,metric)
    BF-->>ROUTE: Ruta alternativa o sin camino
    alt Existe ruta alternativa
        ROUTE-->>RAPI: Route alternativa
        RAPI-->>U: HTTP 200 RouteResponse
    else No existe ruta
        ROUTE-->>RAPI: PATH_NOT_FOUND
        RAPI-->>U: HTTP 404 PATH_NOT_FOUND
    end

    ADM->>LAPI: POST /links/restore
    LAPI->>LMS: restore_link(link_id)
    LMS->>TDAO: get_snapshot()
    TDAO-->>LMS: Topologia actual
    Note over LMS: Restaura el enlace y aumenta version
    LMS->>TDAO: save_topology(topology)
    TDAO-->>LMS: Persistencia confirmada
    LMS-->>LAPI: LinkStateResponse
    LAPI-->>ADM: HTTP 200
    U->>RAPI: GET /route nuevamente
    RAPI->>ROUTE: calculate_route(src,dst,metric)
    ROUTE->>TDAO: get_snapshot()
    TDAO-->>ROUTE: Topologia con enlace restaurado
    ROUTE->>BF: calculate(graph,src,dst,metric)
    BF-->>ROUTE: Ruta actualizada
    ROUTE-->>RAPI: Route
    RAPI-->>U: HTTP 200 RouteResponse
```

</details>

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

**Diagrama visual:**

![Diagrama de secuencia 3: Controller no disponible](diagrams/03-secuencia-3-controller-no-disponible.png)

<details>
<summary>Ver código Mermaid editable</summary>

```mermaid
sequenceDiagram
    hide footbox
    autonumber
    actor U as Cliente
    participant RAPI as Router API
    participant FWD as Forwarding Service
    participant HTTP as HTTP Client
    participant LOG as Forwarding Log DAO

    U->>RAPI: POST /packets
    RAPI->>FWD: forward(packet)
    FWD->>HTTP: GET /route
    Note over HTTP: Controller no responde
    alt Error de conexion
        HTTP-->>FWD: Connection error
    else Tiempo de espera agotado
        HTTP-->>FWD: Timeout 2 segundos
    end
    Note over FWD: No calcula la ruta localmente
    FWD->>LOG: append_result(FAILED)
    LOG-->>FWD: Registro confirmado
    FWD-->>RAPI: ControllerUnavailable
    RAPI-->>U: HTTP 503 CONTROLLER_UNAVAILABLE
```

</details>

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
