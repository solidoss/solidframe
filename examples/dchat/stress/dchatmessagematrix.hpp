#ifndef DCHAT_MESSAGE_MATRIX_HPP
#define DCHAT_MESSAGE_MATRIX_HPP

#include "example/dchat/core/messages.hpp"
#include "utility/dynamicpointer.hpp"
#include <ostream>

struct TextMessage: TextMessageBase, solid::Dynamic<NoopMessage, solid::DynamicShared<solid::frame::Message> >{
	TextMessage(const std::string &_txt);
	TextMessage();
	~TextMessage();
};

typedef solid::DynamicPointer<TextMessage>	TextMessagePointerT;

class MessageMatrix{
public:
	static const MessageMatrix& the(MessageMatrix *_pmm = NULL);
	
	
	MessageMatrix();
	~MessageMatrix();
	
	void createRow(
		const size_t _row_idx,
		const size_t _cnt,
		const size_t _from_sz,
		const size_t _to_sz
	);
	
	void printRowInfo(std::ostream &_ros, const size_t _row_idx, const bool _verbose)const;
	
	TextMessagePointerT message(const size_t _row_idx, const size_t _idx)const;
	bool hasRow(const size_t _idx)const;
	bool check(TextMessage &_rmsg)const;
private:
	struct Data;
	Data	&d;
};

#endif
