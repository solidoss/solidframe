## 20250119
 * release 12.3
 * Experimental Mutable/ConstSharedBuffer
 * Experimental Mutable/ConstIntrusivePtr
 * Fixed FreeBSD 14.2 build
 * Fixed aioreactor for FreeBSD

## 20250119
 * release 12.2

## 20250109
 * improvements and fixes on reflection

## 20250106
 * use std::variant in example_threadpool
 * TypeToType -> std::type_identity

## 20241214
 * mprpc: add support for ConnectionContext::pauseRead and Connection::Context::resumeRead

## 20241210
 * aio: stream fix not call doTrySend or doTryRecv after disconnect
 * mprpc: do not reset timer on onSend with error if connection already stopping
 * mprpc: add support for ConnectionContext::stop() for stopping the connection

## 20241116
 * mprpc::Connection fix too many keep alive messages

## 20241027
 * Create JOURNAL.md
