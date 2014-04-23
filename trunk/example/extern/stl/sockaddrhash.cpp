#include <unordered_map>
#include <deque>
#include <algorithm>
#include <iostream>

#include "system/common.hpp"
#include "system/socketaddress.hpp"

using namespace std;
using namespace solid;

typedef std::deque<SocketAddressInet4>		SocketAddressVectorT;
typedef std::pair<size_t, size_t>			PositionHashPairT;
typedef std::deque<PositionHashPairT>		PositionHashVectorT;


namespace{

struct SockAddrHash{
	size_t operator()(const SocketAddressInet4* const &_psa)const{
		return _psa->hash();
	}
};

struct SockAddrEqual{
	bool operator()(const SocketAddressInet4* const &_psa1, const SocketAddressInet4* const &_psa2)const{
		return *_psa1 == *_psa2;
	}
};

struct PositionHashPairLess{
	bool operator()(PositionHashPairT const &_p1, PositionHashPairT const &_p2)const{
		return _p1.second < _p2.second;
	}
};

}

typedef std::unordered_map<SocketAddressInet4*, uint32, SockAddrHash, SockAddrEqual>	SocketAddressUnorderedMapT;

void generate_addresses(
	size_t _cnt,
	uint8 _start[4],
	uint16 _portstart,
	uint16 _portstep,
	uint16 _portcount,
	SocketAddressVectorT &_outv
);

// ostream& operator<<(ostream &_ros, const SocketAddressInet4 &_rsa){
// 	char				host[SocketInfo::HostStringCapacity];
// 	char				port[SocketInfo::ServiceStringCapacity];
// 	
// 	_rsa.toString(
// 		host,
// 		SocketInfo::HostStringCapacity,
// 		port,
// 		SocketInfo::ServiceStringCapacity,
// 		SocketInfo::NumericService | SocketInfo::NumericHost
// 	);
// 	_ros<<host<<':'<<port;
// 	return _ros;
// }

int main(int argc, char *argv[]){
	uint32 cnt = 3000;
	if(argc > 1){
		cnt = atoi(argv[1]);
	}
	cout<<"Count = "<<cnt<<endl;
	SocketAddressUnorderedMapT	addr_map;
	SocketAddressVectorT		addr_vec;
	PositionHashVectorT			hash_vec;
	uint8 startaddr[4] = {103, 102, 101, 100};
	
	generate_addresses(cnt, startaddr, 4000, 10, 3, addr_vec);
	
	for(size_t i(0); i < addr_vec.size(); ++i){
		size_t h(addr_vec[i].hash());
		hash_vec.push_back(PositionHashPairT(i, h));
	}
	
	std::sort(hash_vec.begin(), hash_vec.end(), PositionHashPairLess());
	
	uint32 collisionmax(0);
	uint32 collisioncnt(0);
	
	size_t crthash;
	
	uint32 crtcollisioncnt;
	for(PositionHashVectorT::const_iterator it(hash_vec.begin()); it != hash_vec.end(); ++it){
		if(it == hash_vec.begin()){
			crthash = it->second;
			crtcollisioncnt = 0;
			continue;
		}
		if(it->second == crthash){
			//collision
			++crtcollisioncnt;
			if(crtcollisioncnt == 1){
				PositionHashVectorT::const_iterator oldit(it - 1);
				cout<<"collision "<<crthash<<'['<<endl;
				cout<<addr_vec[oldit->first]<<endl;
			}
			cout<<addr_vec[it->first]<<endl;
		}else{
			if(crtcollisioncnt){
				cout<<']'<<endl;
				if(crtcollisioncnt > collisionmax){
					collisionmax = crtcollisioncnt;
				}
				++collisioncnt;
			}
			crtcollisioncnt = 0;
			crthash = it->second;
		}
	}
	
	if(crtcollisioncnt){
		cout<<']'<<endl;
		if(crtcollisioncnt > collisionmax){
			collisionmax = crtcollisioncnt;
		}
		++collisioncnt;
	}
	cout<<"Max collision count: "<<collisionmax<<endl;
	cout<<"Collisions count:    "<<collisioncnt<<endl;
	
	for(size_t i(0); i < addr_vec.size(); ++i){
		addr_map[&addr_vec[i]] = i;
	}
	for(size_t i(0); i < addr_vec.size(); ++i){
		SocketAddressUnorderedMapT::const_iterator it(addr_map.find(&addr_vec[i]));
		if(it != addr_map.end()){
		}else{
			cout<<"address not found: "<<addr_vec[i]<<endl;
		}
	}
	SocketAddressInet4 sa;
	{
		ResolveData	rd;
		char		buf[128];
		sprintf(buf, "%d.%d.%d.%d", (int)100, (int)100, (int)100, (int)100);
		rd = synchronous_resolve(buf, 4010);
		if(!rd.empty()){
			sa = rd.begin();
			cout<<"created address "<<sa<<endl;
		}
	}
	SocketAddressUnorderedMapT::const_iterator it(addr_map.find(&sa));
	if(it != addr_map.end()){
		cout<<"address found!!"<<endl;
	}else{
		cout<<"address not found: "<<sa<<endl;
	}
	return 0;
}

void increment(uint8 *_vec){
	++_vec[3];
	if(_vec[3] == 0){
		++_vec[2];
		if(_vec[2] == 0){
			++_vec[1];
			if(_vec[1] == 0){
				++_vec[0];
			}
		}
	}
}

void generate_addresses(
	size_t _cnt,
	uint8 _start[4],
	uint16 _portstart,
	uint16 _portstep,
	uint16 _portcount,
	SocketAddressVectorT &_outv
){
	uint8	vec[4];
	char	buf[128];
	vec[0] = _start[0];
	vec[1] = _start[1];
	vec[2] = _start[2];
	vec[3] = _start[3];
	ResolveData rd;
	for(size_t i(0); i < _cnt; ++i){
		sprintf(buf, "%d.%d.%d.%d", (int)vec[0], (int)vec[1], (int)vec[2], (int)vec[3]);
		uint32 maxport(_portstart + _portcount * _portstep);
		for(uint16 p(_portstart); p < maxport; p += _portstep){
			rd = synchronous_resolve(buf, p);
			if(!rd.empty()){
				_outv.push_back(SocketAddressInet4(rd.begin()));
				//cout<<"created address "<<_outv.back()<<endl;
			}
		}
		increment(vec);
	}
}


