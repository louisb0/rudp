# rudp
The TCP state machine implemented on UDP.

Using the [Wireshark dissector](./rudp_dissector.lua), we can look at an example where the first of two 1024-byte packets are dropped, retransmitted, then cumulatively acknowledged.

```
1	0.000000000     v1 40641 → 8888  SEQ=0 ACK=0 (LEN=1) [SYN]
2	0.000215346     v1 43504 → 40641 SEQ=0 ACK=1 (LEN=1) [SYN,ACK]
3	0.000305460     v1 40641 → 43504 SEQ=1 ACK=1 (LEN=0) [ACK]
4	0.050519461     v1 40641 → 43504 SEQ=1025 ACK=1 (LEN=1024)
5	5.059826479     v1 40641 → 43504 SEQ=1025 ACK=1 (LEN=1024)      <- retransmit starts
6	5.059863733     v1 40641 → 43504 SEQ=1 ACK=1 (LEN=1024)
7	5.059973130	v1 43504 → 40641 SEQ=1 ACK=2049 (LEN=0) [ACK]
```

See [./examples/server-client/](./examples/server-client/) for the transmission of this data. 
