## 20250106
 * use std::variant in example_threadpool

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
