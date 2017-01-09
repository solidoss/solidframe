#ifndef STUDY_SHAREDOBJECTMANAGER_HPP
#define STUDY_SHAREDOBJECTMANAGER_HPP

#include "utility/common.hpp"
#include <ostream>

struct WorkPoolController;

struct ObjectStub;

class SharedObjectManager{
public:
    enum{
        RaiseFlag = 1,
        EventFlag = 2,
        Flag1 = 4,
        Flag2 = 8,
        Flag3 = 16,
        Flag4 = 32
    };
    SharedObjectManager();
    ~SharedObjectManager();

    bool start();

    void insert(size_t _v);

    bool notify(size_t _idx, uint32_t _flags);
    bool notify(size_t _idx, uint32_t _flags, size_t _v);

    bool notifyAll(uint32_t _flags);
    bool notifyAll(uint32_t _flags, size_t _v);

    void stop(std::ostream &_ros);
private:
    void executeObject(ObjectStub &_robj);

    friend struct WorkPoolController;
    struct Data;
    Data &d;
};

#endif
