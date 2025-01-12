# rudp
The TCP state machine implemented on UDP.

To keep the focus on core networking and reliability concepts, this implementation omits:

* a thread safe user-interface
* handling of TCP edge-cases, such as the simultaneous open/close
