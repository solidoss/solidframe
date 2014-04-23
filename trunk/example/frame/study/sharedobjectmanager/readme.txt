The sharedobjectmanager study is inteded for determining which alternative is speedier in case of frame::Manager or frame::SharedStore:

1) An architecture based on SharedMutex/ReadWriteMutex - write lock when inserting new object in Manager/SharedStore and read lock when accessing an object.
2) An architecture based on MutualStore<Mutex> - the one already implemented in frame::Manager.