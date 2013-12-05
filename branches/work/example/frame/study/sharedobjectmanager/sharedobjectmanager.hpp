#ifndef STUDY_SHAREDOBJECTMANAGER_HPP
#define STUDY_SHAREDOBJECTMANAGER_HPP

#include "utility/common.hpp"

struct WorkPoolController;

struct ObjectStub;

class SharedObjectManager{
public:
	SharedObjectManager();
	~SharedObjectManager();
	
	bool start();
	
	void insert(size_t _v);
	
	bool notify(size_t _idx, solid::uint32 _flags);
	bool notify(size_t _idx, solid::uint32 _flags, size_t _v);
	
	bool notifyAll(solid::uint32 _flags);
	bool notifyAll(solid::uint32 _flags, size_t _v);
	
	void stop();
	
private:
	void executeObject(ObjectStub &_robj);
	
	friend struct WorkPoolController;
	struct Data;
	Data &d;
};

#endif
