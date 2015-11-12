// frame/ipc/src/ipcmessagewriter.hpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP
#define SOLID_FRAME_IPC_SRC_IPC_MESSAGE_WRITER_HPP

#include <vector>

#include "system/common.hpp"
#include "system/error.hpp"
#include "system/specific.hpp"
#include "system/function.hpp"

#include "ipcutility.hpp"
#include "frame/ipc/ipcserialization.hpp"




namespace solid{
namespace frame{
namespace ipc{

struct Configuration;

struct Serializer: public SerializerT, public SpecificObject{
	Serializer(TypeIdMapT const &_ridmap): SerializerT(&_ridmap){}
};

typedef std::unique_ptr<Serializer>		SerializerPointerT;

typedef FUNCTION<void(
	MessagePointerT &/*_rmsgptr*/,
	const size_t /*_msg_type_idx*/,
	ResponseHandlerFunctionT &,
	ulong /*_flags*/
)>										MessageWriterVisitFunctionT;

class MessageWriter{
public:
	MessageWriter();
	~MessageWriter();
	
	//must be used under lock, i.e. under Connection's lock
	MessageUid safeNewMessageUid(Configuration const &_rconfig);
	
	bool isNonSafeCacheEmpty()const;
	
	void safeMoveCacheToSafety();
	
	void enqueue(
		MessageBundle &_rmsgbundle,
		MessageUid const &_rmsguid,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	void cancel(
		MessageUid const &_rmsguid,
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx
	);
	
	void enqueueClose();
	
	uint32 write(
		char *_pbuf,
		uint32 _bufsz, const bool _keep_alive, 
		Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT &_rerror
	);
	
	void completeMessage(
		MessagePointerT &_rmsgptr,
		MessageUid const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	bool shouldTryFetchNewMessage(Configuration const &_rconfig)const;
	
	bool empty()const;
	
	void prepare(Configuration const &_rconfig);
	void unprepare();
	
	void visitAllMessages(MessageWriterVisitFunctionT const &_rvisit_fnc);
	
	bool hasUnsafeCache()const;
	
	void completeAllMessages(
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
private:
	
	struct InnerLink{
		InnerLink(
			const size_t _next = InvalidIndex(),
			const size_t _prev = InvalidIndex()
		):next(_next), prev(_prev){}
		
		void clear(){
			next = InvalidIndex();
			prev = InvalidIndex();
		}
		
		size_t next;
		size_t prev;
	};
	
	struct InnerList{
		InnerList(
			const size_t _front = InvalidIndex(),
			const size_t _back = InvalidIndex()
		):front(_front), back(_back), size(0){}
		
		bool empty()const{
			return front == InvalidIndex();
		}
		
		void clear(){
			front = InvalidIndex();
			back = InvalidIndex();
			size = 0;
		}
		
		size_t	front;
		size_t	back;
		size_t	size;
	};
	
	enum{
		InnerListOrder = 0,
		InnerListPending,
		InnerListSending,
		InnerListCache,
		
		InnerListCount //do not change
	};
	
	enum{
		InnerLinkOrder = 0,
		InnerLinkStatus,
		
		InnerLinkCount
	};
	
	enum InnerStatus{
		InnerStatusInvalid,
		InnerStatusPending = 1,
		InnerStatusSending,
		InnerStatusWaiting,
		InnerStatusCache,
	};
	
	struct MessageStub{
		MessageStub(
			MessageBundle &_rmsgbundle
		):	msgbundle(std::move(_rmsgbundle)), packet_count(0),
			inner_status(InnerStatusInvalid){}
		
		MessageStub(
		):	unique(0), packet_count(0),
			inner_status(InnerStatusInvalid){}
		
		void clear(){
			msgbundle.clear();
			++unique;
			packet_count = 0;
			
			inner_link[InnerLinkOrder].clear();
			//inner_link[InnerLinkStatus].clear();//must not be cleared
			
			inner_status = InnerStatusInvalid;
			
			serializer_ptr = nullptr;
		}
		
		MessageBundle				msgbundle;
		uint32						unique;
		size_t						packet_count;
		SerializerPointerT			serializer_ptr;
		InnerLink					inner_link[InnerLinkCount];
		InnerStatus					inner_status;
	};
	
//	typedef Queue<PendingMessageStub>							PendingMessageQueueT;
	typedef std::vector<MessageStub>							MessageVectorT;
	typedef std::vector<MessageUid>								MessageUidVectorT;
	
	struct PacketOptions{
		PacketOptions(): packet_type(PacketHeader::SwitchToNewMessageTypeE), force_no_compress(false){}
		
		PacketHeader::Types		packet_type;
		bool 					force_no_compress;
	};
	
	typedef Stack<size_t>										CacheStackT;
	
	char* doFillPacket(
		char* _pbufbeg,
		char* _pbufend,
		PacketOptions &_rpacket_options,
		bool &_rmore,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const & _ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT & _rerror
	);
	
	bool isSynchronousInSendingQueue()const;
	bool isAsynchronousInPendingQueue()const;
	
	void doTryMoveMessageFromPendingToWriteQueue(ipc::Configuration const &_rconfig);
	void doCompleteMessage(
		MessagePointerT &_rmsgptr,
		MessageUid const &_rmsguid,
		ipc::Configuration const &_rconfig,
		TypeIdMapT const &_ridmap,
		ConnectionContext &_rctx,
		ErrorConditionT const & _rerror
	);
	
	template <size_t List>
	bool innerListEmpty()const{
		return inner_list[List].empty();
	}
	
	template <size_t List, size_t Link>
	void innerListPushBack(const size_t _message_vec_index){
		MessageStub		&rmsgstub = message_vec[_message_vec_index];
	
		rmsgstub.inner_link[Link] = InnerLink(InvalidIndex(), inner_list[List].back);
		
		if(inner_list[List].back != InvalidIndex()){
			message_vec[inner_list[List].back].inner_link[Link].next = _message_vec_index;
			inner_list[List].back = _message_vec_index;
		}else{
			inner_list[List] = InnerList(_message_vec_index, _message_vec_index);
		}
		++inner_list[List].size;
	}
	
	template <size_t List, size_t Link>
	void innerListErase(const size_t _message_vec_index){
		MessageStub		&rmsgstub = message_vec[_message_vec_index];
	
		if(rmsgstub.inner_link[Link].prev != InvalidIndex()){
			message_vec[rmsgstub.inner_link[Link].prev].inner_link[Link].next = rmsgstub.inner_link[Link].next;
		}else{
			//first message in the q
			inner_list[List].front = rmsgstub.inner_link[Link].next;
		}
		
		if(rmsgstub.inner_link[Link].next != InvalidIndex()){
			message_vec[rmsgstub.inner_link[Link].next].inner_link[Link].prev = rmsgstub.inner_link[Link].prev;
		}else{
			//last message in the q
			inner_list[List].front = rmsgstub.inner_link[Link].prev;
		}
		--inner_list[List].size;
		rmsgstub.inner_link[Link].clear();
	}
	
	template <size_t List, size_t Link>
	void innerListPopFront(){
		if(inner_list[List].front != InvalidIndex()){
			innerListErase<List, Link>(inner_list[List].front);
		}
	}
	
	void innerListClearAll(){
		inner_list[InnerListOrder].clear();
		inner_list[InnerListPending].clear();
		inner_list[InnerListSending].clear();
		inner_list[InnerListCache].clear();
	}
	
private:
	MessageVectorT				message_vec;
	MessageUidVectorT			message_uid_cache_vec;
	std::atomic<size_t>			message_idx_cache;
	uint32						current_message_type_id;
	uint32						flags;
	InnerList					inner_list[InnerListCount];
};


inline bool MessageWriter::isNonSafeCacheEmpty()const{
	return inner_list[InnerListCache].empty();
}

}//namespace ipc
}//namespace frame
}//namespace solid


#endif
