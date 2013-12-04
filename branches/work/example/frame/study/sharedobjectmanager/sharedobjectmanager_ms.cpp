#include "sharedobjectmanager.hpp"
//sharedobject manager - using mutualstore

using namespace solid;
using namespace std;

struct SharedObjectManager::Data{
	
};

//=========================================================================

SharedObjectManager::SharedObjectManager():d(*(new Data)){
	
}
SharedObjectManager::~SharedObjectManager(){
	delete &d;
}

bool SharedObjectManager::start(){
	return false;
}

void SharedObjectManager::insert(size_t _v){
	
}

bool SharedObjectManager::notify(size_t _idx, uint32 _flags){
	return false;
}
bool SharedObjectManager::notify(size_t _idx, uint32 _flags, size_t _v){
	return false;
}

bool SharedObjectManager::notifyAll(uint32 _flags){
	return false;
}
bool SharedObjectManager::notifyAll(uint32 _flags, size_t _v){
	return false;
}

void SharedObjectManager::stop(){
	
}

