\ingroup GroupModules Modules
\defgroup GroupI2C I2C HAL

Module I2C Architecture
=======================

# Overview                                   {#module_i2c_architecture_overview}

This module implements a Hardware Abstraction Layer (HAL) API for I2C
transactions.

# Architecture                           {#module_i2c_architecture_architecture}

The I2C module provides an interface for modules to request transmission of data
through an I2C bus, reception from an I2C bus and to request a transmission
followed by a reception on an I2C bus.

The I2C module defines a driver interface on which it relies to transfer/receive
data to/from the bus.

A response event notifies the caller of the transaction completion.

The I2C module provides support for concurrent accesses to an I2C bus. If a
I2C transaction is requested on an I2C bus while the bus is busy processing
another transaction, the transaction request is queued. The queuing and
processing of I2C transaction requests follow a FIFO logic. When a transaction
is completed, the processing of the transaction request at the head of the
queue, if any, is initiated.

The optional `transfer_as_controller()` API instead executes one synchronous
transaction and returns its final status without generating a response event.
It accepts a `mod_i2c_request`: a nonzero transmit length writes bytes, a nonzero
receive length reads bytes, and specifying both performs a write followed by a
repeated START and read, with one final STOP. The buffers only need to remain
valid until the call returns. A 7-bit nonzero target address is required.

This API is for firmware-thread callers, including module startup; ISR and
concurrent calls are unsupported. It returns `FWK_E_BUSY` while the HAL has an
active asynchronous transaction and `FWK_E_SUPPORT` for drivers without the
optional callback. Synchronous-only drivers are also supported; the event-based
APIs return `FWK_E_SUPPORT` before queuing a request to such a driver.

DW APB I2C selects this path with `mod_dw_apb_i2c_dev_config.polled = true` and
a nonzero `transfer_timeout_us`. It binds the timer API, masks controller
interrupts, initializes the master/repeated-START configuration, and does not
register an ISR or bind the HAL completion callback. Its timer-bounded FIFO
polling handles repeated START, STOP, and transmit aborts. Timeout, timer failure,
or failure to disable the controller blocks later requests with `FWK_E_STATE`,
since a queued command may still complete after the failure. Recovery requires
controller/platform reinitialization. The default IRQ-based driver path is
unchanged.

# Restriction                             {#module_i2c_architecture_restriction}

The following features are unsupported. Support may be added in the future.

- Acting like a target.
- 10-bit target addressing.

# Flow                                           {#module_i2c_architecture_flow}

The following schematic describes the transaction flow for an I2C controller
transmission. It is assumed that all the sequence occurs without any
concurrent access and that the driver handles the transmission asynchonously.
The flow for a reception is similar.

    Client             I2C         I2C Driver     I2C ISR (Driver)
      |                 |               |               |
      |   transmit_     |               |               |
     +-+  as_controller |               |               |
     | +-------------->+-+              |               |
     | |               | +- - +         |               |
     | +<--------------+-+    |process_ |               |
     +-+                |     |event E1 |               |
      |                 |     |         |               |
      |                +-+<- -+         |               |
      |                | | transmit_    |               |
      |                | | as_controller|               |
      |                | +------------>+-+              |
      |                | +<------------+-+              |
      |                +-+              |               |
      |                 |               |  transaction +-+
      |                 |               |  _completed  | |
      |                +-+<-------------+--------------+ |
      |            +- -+ |              |              | |
      |  process_  |   +-+--------------+------------->+ |
      |  event E2  |    |               |              | |
      |            +- >+-+              |              +-+
      |                | |              |               |
     +-+<- - - - - - - +-+              |               |
     +-+  process_      |               |               |
      |   event R1      |               |               |

    E1   : Request event
    E2   : Request completed event
    R1   : Response to the request event E1
    ---> : Function call/return
    - -> : Asynchronous call via the event/notification interface

The client calls *transmit_as_controller* API of the I2C module.
This function creates and sends the targeted I2C device request event which
defines the selected target on the bus and the data to be transmitted. When
processing the request event, the I2C module initiates the transfer by
programming the I2C controller through the *transmit_as_controller* API of the
I2C driver.

An interrupt is asserted when the I2C transaction either completes or encounters
an error. The I2C ISR calls the *transaction_completed* API of the I2C HAL
module which sends a response event to indicate that transaction completed to
the client.

In the case of *transmit_then_receive_as_controller*, the I2C HAL does not send
a event at the end of the transmission. Instead it starts the reception by 
calling *receive_as_controller* driver function. The event is then sent when
the reception has completed.

# Concurrent accesses             {#module_i2c_architecture_concurrent_accesses}

In case of concurrent access, transaction requests are queued using the
framework delayed response facility. When the transaction request event is
processed, the transaction is not initiated and its response delayed. This is
illustrated by the following schematic:

    Client             I2C
      |                 |
      |   transmit_     |
     +-+  as_controller |
     | +-------------->+-+
     | |               | +- - +
     | +<--------------+-+    |process_
     +-+                |     |event E1
      |                 |     |
      |                +-+<- -+
      |                | |
      |                | | Request response delayed
      |                | |
      |                +-+
      |                 |
      |                 |

    E1 : Request event

When a transaction is completed on an I2C bus, the delayed response queue of
the associated I2C module element is checked. If it is not empty, the processing
of the request associated to the head of the queue is then initiated. This is
illustrated in the following schematic where the pending request which is
initiated is a reception request.

    Client             I2C         I2C Driver     I2C ISR (Driver)
                             ...
      |                 |               |               |
      |                 |               |  transaction +-+
      |                 |               |  _completed  | |
      |                +-+<-------------+--------------+ |
      |            +- -+ |              |              | |
      |  process_  |   +-+--------------+------------->+ |
      |  event E1  |    |               |              | |
      |            +- >+-+              |              +-+
      |                | |              |               |
      |                | | receive_     |               |
      |                | | as_controller|               |
      |                | +------------>+-+              |
      |                | +<------------+-+              |
     +-+<- - - - - - - + |              |               |
     | |  process_     | |              |               |
     | |  event R1     | |              |               |
     +-+               +-+              |               |
      |                 |               |               |
                             ...

    E1   : Request completed event
    R1   : Response to the completed request
    ---> : Function call/return
    - -> : Asynchronous call via the event/notification interface

Finally, in the case where the processing of the pending request completes
immediately (synchronous handling by the driver) and another transaction request
is pending, the processing of this last transaction request is not initiated
immediately to avoid multiple transactions being processed within the same event
processing. A reload event is then sent and the processing of the next pending
request is initiated as part of the processing of the reload event.

Asynchronous request events carry a native pointer to a module-owned copy of
the request descriptor. The event payload remains 16 bytes on both 32-bit and
64-bit targets; pointers are copied with `memcpy` to avoid event-parameter
alignment assumptions. The descriptor is allocated with `fwk_mm_alloc_notrap`
and freed when copied into the active device context, when event submission
fails, or when a request is rejected in the panic state. Allocation failure
returns `FWK_E_NOMEM` without submitting an event. Queued delayed requests keep
their own descriptor until they become active. Data buffers remain owned by the
caller and must stay valid until the response is delivered.

The device also tracks descriptors of requests dispatched while busy. If the
framework fails to allocate their delayed response, these orphaned descriptors
are reclaimed when the delayed queue becomes empty. Entering the terminal panic
state releases all tracked descriptors; undelivered request events keep their
own ownership until their panic response is handled. This does not change the
framework's existing request/response loss behavior on event-pool exhaustion.
