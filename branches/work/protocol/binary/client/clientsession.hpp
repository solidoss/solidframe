#ifndef SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP
#define SOLID_PROTOCOL_BINARY_CLIENT_SESSION_HPP

#include "frame/message.hpp"
#include "utility/dynamicpointer.hpp"

namespace solid{
namespace protocol{
namespace binary{
namespace client{

class Session{
public:
	void schedule(DynamicPointer<frame::Message> &_rmsgptr){
		
	}
	template <class Iter>
	void schedule(Iter _beg, const Iter _end){
		while(_beg != _end){
			schedule(*_beg);
			++_beg;
		}
	}
	bool consume(const char *_pb, size_t _bl){
		return false;
	}
	int fill(char *_pb, size_t _bl){
		return NOK;
	}
private:
};

}//namespace client
}//namespace binary
}//namespace protocol
}//namespace solid


#endif
