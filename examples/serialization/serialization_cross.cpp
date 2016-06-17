// serialization_simple.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
//#undef SOLID_HAS_DEBUG
#include "system/debug.hpp"
#include "system/cassert.hpp"

#include "serialization/binary.hpp"
#include "serialization/binarybasic.hpp"
#include "system/socketaddress.hpp"

using namespace std;
using namespace solid;


void test_basic_serialization();

int main(int argc, char *argv[]){
#ifdef SOLID_HAS_DEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr(false);
#endif
	cout<<"sizeof(map<int , string>::iterator): "<<sizeof(map<int32_t , string>::iterator)<<endl;
	cout<<"sizeof(list<string>::iterator): "<<sizeof(list<string>::iterator)<<endl;
	cout<<"sizeof(deque<string>::iterator): "<<sizeof(deque<string>::iterator)<<endl;
	cout<<"sizeof(uint64_t) = "<<sizeof(uint64_t)<<endl;
	cout<<"sizeof(uint32_t) = "<<sizeof(uint32_t)<<endl;
	cout<<"sizeof(uint16_t) = "<<sizeof(uint16_t)<<endl;
	cout<<"sizeof(uint8_t) = "<<sizeof(uint8_t)<<endl;
	const int	blen = 16;
	char 		bufs[1000][blen];
	int			rv = 3;
	
	typedef serialization::binary::Serializer<void>									BinSerializerT;
	typedef serialization::binary::Deserializer<void>								BinDeserializerT;
	
	
	cout<<"test basic serialization:"<<endl;
	test_basic_serialization();
	
	cout<<"test serialization:"<<endl;
	
	for(int i = 0; i < 1; ++i){
		{	
			idbg("");
			BinSerializerT 	ser;
			
			uint8_t			v08_00 = 0xff;
			uint16_t			v16_00 = 0xffff;
			uint32_t			v32_00 = 0xffffffff;
			uint64_t			v64_00 = 0xffffffffffffffffULL;
			
			cout<<"v08 = "<<(int)v08_00<<" v16 = "<<v16_00<<" v32 = "<<v32_00<<" v64 = "<<v64_00<<endl;
			//ser.pushCross(v08_00, "v08").pushCross(v16_00, "v16").pushCross(v32_00, "v32").pushCross(v64_00, "v64");
			ser.pushCross(v64_00, "v64").pushCross(v32_00, "v32").pushCross(v16_00, "v16").pushCross(v08_00, "v08");
			
			uint8_t			v08_01 = 0x0f;
			uint16_t			v16_01 = 0x0f0f;
			uint32_t			v32_01 = 0x0f0f0f0f;
			uint64_t			v64_01 = 0x0f0f0f0f0f0f0f0fULL;
			
			cout<<"v08 = "<<(int)v08_01<<" v16 = "<<v16_01<<" v32 = "<<v32_01<<" v64 = "<<v64_01<<endl;
			ser.pushCross(v08_01, "v08").pushCross(v16_01, "v16").pushCross(v32_01, "v32").pushCross(v64_01, "v64");
			
			
			uint8_t			v08_02 = 0x0f;
			uint16_t			v16_02 = 0x00ff;
			uint32_t			v32_02 = 0x0000ffff;
			uint64_t			v64_02 = 0x0000ffff0000ffffULL;
			
			cout<<"v08 = "<<(int)v08_02<<" v16 = "<<v16_02<<" v32 = "<<v32_02<<" v64 = "<<v64_02<<endl;
			ser.pushCross(v08_02, "v08").pushCross(v16_02, "v16").pushCross(v32_02, "v32").pushCross(v64_02, "v64");
			
			uint8_t			v08_03 = 0x0f;
			uint16_t			v16_03 = 0x00ff;
			uint32_t			v32_03 = 0x000000ff;
			uint64_t			v64_03 = 0x00000000ffffffffULL;
			
			cout<<"v08 = "<<(int)v08_03<<" v16 = "<<v16_03<<" v32 = "<<v32_03<<" v64 = "<<v64_03<<endl;
			ser.pushCross(v08_03, "v08").pushCross(v16_03, "v16").pushCross(v32_03, "v32").pushCross(v64_03, "v64");
			
			int v = 0, cnt = 0;
			idbg("");
			while((rv = ser.run(bufs[v], blen)) == blen){
				cnt += rv;
				++v;
			}
			if(rv < 0){
				cout<<"ERROR: serialization: "<<ser.error().message()<<endl;
				return 0;
			}
			idbg("");
			cnt += rv;
			cout<<"Write count: "<<cnt<<" buffnct = "<<v<<endl;
		}
		cout<<"Deserialization: =================================== "<<endl;
		{
			BinDeserializerT	des;
			
			uint64_t	v08_00, v16_00, v32_00, v64_00;
			des.pushCross(v08_00, "v08").pushCross(v16_00, "v16").pushCross(v32_00, "v32").pushCross(v64_00, "v64");
			
			uint64_t	v08_01, v16_01, v32_01, v64_01;
			des.pushCross(v08_01, "v08").pushCross(v16_01, "v16").pushCross(v32_01, "v32").pushCross(v64_01, "v64");
			
			uint64_t	v08_02, v16_02, v32_02, v64_02;
			des.pushCross(v08_02, "v08").pushCross(v16_02, "v16").pushCross(v32_02, "v32").pushCross(v64_02, "v64");
			
			uint64_t	v08_03, v16_03, v32_03, v64_03;
			des.pushCross(v08_03, "v08").pushCross(v16_03, "v16").pushCross(v32_03, "v32").pushCross(v64_03, "v64");
			
			int v = 0, cnt = 0;
			while((rv = des.run(bufs[v], blen)) == blen){
				cnt += rv;
				++v;
			}
			if(rv < 0){
				cout<<"ERROR: deserialization "<<des.error().message()<<endl;
				return 0;
			}
			
			cout<<"v08 = "<<v08_00<<" v16 = "<<v16_00<<" v32 = "<<v32_00<<" v64 = "<<v64_00<<endl;
			cout<<"v08 = "<<v08_01<<" v16 = "<<v16_01<<" v32 = "<<v32_01<<" v64 = "<<v64_01<<endl;
			cout<<"v08 = "<<v08_02<<" v16 = "<<v16_02<<" v32 = "<<v32_02<<" v64 = "<<v64_02<<endl;
			cout<<"v08 = "<<v08_03<<" v16 = "<<v16_03<<" v32 = "<<v32_03<<" v64 = "<<v64_03<<endl;
			
			cout<<"Read count: "<<cnt<<" buffnct = "<<v<<endl;
		}
	}//for
	idbg("Done");
	return 0;
}

void test_basic(uint8_t _v){
	using namespace serialization::binary;
	char	buf[32];
	cout<<"store: "<<(int)_v<<" size = "<<crossSize(_v)<<endl;
	crossStore(buf, _v);
	//
	uint8_t	v08;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	crossLoad(buf, v08);
	crossLoad(buf, v16);
	crossLoad(buf, v32);
	crossLoad(buf, v64);
	cout<<"load: "<<(int)v08<<' '<<v16<<' '<<v32<<' '<<v64<<endl;
}

void test_basic(uint16_t _v){
	using namespace serialization::binary;
	char	buf[32];
	cout<<"store: "<<_v<<" size = "<<crossSize(_v)<<endl;
	crossStore(buf, _v);
	//
	uint8_t	v08;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	crossLoad(buf, v08);
	crossLoad(buf, v16);
	crossLoad(buf, v32);
	crossLoad(buf, v64);
	cout<<"load: "<<(int)v08<<' '<<v16<<' '<<v32<<' '<<v64<<endl;
}

void test_basic(uint32_t _v){
	using namespace serialization::binary;
	char	buf[32];
	cout<<"store: "<<_v<<" size = "<<crossSize(_v)<<endl;
	crossStore(buf, _v);
	//
	uint8_t	v08;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	crossLoad(buf, v08);
	crossLoad(buf, v16);
	crossLoad(buf, v32);
	crossLoad(buf, v64);
	cout<<"load: "<<(int)v08<<' '<<v16<<' '<<v32<<' '<<v64<<endl;
}

void test_basic(uint64_t _v){
	using namespace serialization::binary;
	char	buf[32];
	cout<<"store: "<<_v<<" size = "<<crossSize(_v)<<endl;
	crossStore(buf, _v);
	//
	uint8_t	v08;
	uint16_t	v16;
	uint32_t	v32;
	uint64_t	v64;
	crossLoad(buf, v08);
	crossLoad(buf, v16);
	crossLoad(buf, v32);
	crossLoad(buf, v64);
	cout<<"load: "<<(unsigned)v08<<' '<<v16<<' '<<v32<<' '<<v64<<endl;
}


void test_basic_serialization(){
	cout<<"8bits----------------"<<endl;
	test_basic(static_cast<uint8_t>(0xf));
	test_basic(static_cast<uint8_t>(0xff));
	test_basic(static_cast<uint8_t>(0x0f));
	
	cout<<"16bits---------------"<<endl;
	test_basic(static_cast<uint16_t>(0xf));
	test_basic(static_cast<uint16_t>(0xff));
	test_basic(static_cast<uint16_t>(0x0f));
	test_basic(static_cast<uint16_t>(0x0f0f));
	test_basic(static_cast<uint16_t>(0xf0f0));
	test_basic(static_cast<uint16_t>(0xff00));
	test_basic(static_cast<uint16_t>(0x00ff));
	test_basic(static_cast<uint16_t>(0xffff));
	
	cout<<"32bits---------------"<<endl;
	test_basic(static_cast<uint32_t>(0xf));
	test_basic(static_cast<uint32_t>(0xff));
	test_basic(static_cast<uint32_t>(0x0f));
	test_basic(static_cast<uint32_t>(0x0f0f));
	test_basic(static_cast<uint32_t>(0xf0f0));
	test_basic(static_cast<uint32_t>(0xff00));
	test_basic(static_cast<uint32_t>(0x00ff));
	test_basic(static_cast<uint32_t>(0xffff));
	test_basic(static_cast<uint32_t>(0xffffffff));
	test_basic(static_cast<uint32_t>(0x0000ffff));
	test_basic(static_cast<uint32_t>(0x00ff00ff));
	
	cout<<"64bits----------------"<<endl;
	test_basic(static_cast<uint64_t>(0xf));
	test_basic(static_cast<uint64_t>(0xff));
	test_basic(static_cast<uint64_t>(0x0f));
	test_basic(static_cast<uint64_t>(0x0f0f));
	test_basic(static_cast<uint64_t>(0xf0f0));
	test_basic(static_cast<uint64_t>(0xff00));
	test_basic(static_cast<uint64_t>(0x00ff));
	test_basic(static_cast<uint64_t>(0xffff));
	test_basic(static_cast<uint64_t>(0xffffffff));
	test_basic(static_cast<uint64_t>(0x0000ffff));
	test_basic(static_cast<uint64_t>(0x00ff00ff));
	test_basic(static_cast<uint64_t>(0x00ff00ff00ff00ffULL));
	test_basic(static_cast<uint64_t>(0xffffffffffffffffULL));
	test_basic(static_cast<uint64_t>(0xf0f0f0f0f0f0f0f0ULL));
	test_basic(static_cast<uint64_t>(0x0f0f0f0f0f0f0f0fULL));
	test_basic(static_cast<uint64_t>(0xffff0000ffff0000ULL));
	test_basic(static_cast<uint64_t>(0x0000ffff0000ffffULL));
	//exit(0);
}
