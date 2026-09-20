# my-redis

A single-threaded, `epoll`-based TCP key-value server in C++17: a hand-written
TLV wire protocol, an RAII byte buffer for partial/pipelined reads, and a
custom hash table with incremental (progressive) rehashing. Supports
`GET`/`SET`/`DEL`.

## Build

```
make          # debug build (server, client, bench)
make release  # optimised build (-O2 -DNDEBUG) -- use this for benchmarking
```

## Run

```
./server            # starts listening on 127.0.0.1:1234
./client SET foo bar
./client GET foo
```

## Benchmark

`bench` is a single-connection, sequential (non-pipelined) throughput test:
it sends N `SET` requests followed by N `GET` requests over one connection,
waiting for each reply before sending the next, and reports ops/sec.

```
make clean release   # never benchmark a debug build
./server &
./bench 1234 200000  # <port> <num requests per command>
```

Sample output on this machine:

```
SET: 200000 ops in 1.80s -> 111000 ops/sec
GET: 200000 ops in 1.78s -> 112000 ops/sec
```

This measures round-trip latency on one connection, not concurrent or
pipelined throughput -- there's no multi-connection or pipelining benchmark
yet.
