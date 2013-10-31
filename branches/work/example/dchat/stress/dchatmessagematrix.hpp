#ifndef DCHAT_MESSAGE_MATRIX_HPP
#define DCHAT_MESSAGE_MATRIX_HPP

#include "example/dchat/core/messages.hpp"

struct TextMessage: TextMessageBase, solid::Dynamic<NoopMessage, solid::DynamicShared<solid::frame::Message> >{
	TextMessage(const std::string &_txt):TextMessageBase(_txt){}
	TextMessage(){}
};


#endif
