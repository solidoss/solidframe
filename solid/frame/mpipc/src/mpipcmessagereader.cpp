// solid/frame/ipc/src/ipcmessagereader.cpp
//
// Copyright (c) 2015 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "mpipcmessagereader.hpp"
#include "mpipcutility.hpp"
#include "solid/frame/mpipc/mpipcconfiguration.hpp"
#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcerror.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/system/debug.hpp"

namespace solid {
namespace frame {
namespace mpipc {

//-----------------------------------------------------------------------------
MessageReader::MessageReader()
    : state(HeaderReadStateE)
    , current_message_type_id(InvalidIndex())
{
}
//-----------------------------------------------------------------------------
MessageReader::~MessageReader()
{
}
//-----------------------------------------------------------------------------
void MessageReader::prepare(ReaderConfiguration const& _rconfig)
{
    message_q.push(MessageStub()); //start with an empty message
}
//-----------------------------------------------------------------------------
void MessageReader::unprepare()
{
}
//-----------------------------------------------------------------------------
uint32_t MessageReader::read(
    const char*                _pbuf,
    uint32_t                   _bufsz,
    const CompleteFunctionT&   _complete_fnc,
    ReaderConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{
    const char*  pbufpos = _pbuf;
    const char*  pbufend = _pbuf + _bufsz;
    PacketHeader packet_header;

    while (pbufpos != pbufend) {
        if (state == HeaderReadStateE) {
            //try read the header
            if ((pbufend - pbufpos) >= PacketHeader::SizeOfE) {
                state = DataReadStateE;
            } else {
                break;
            }
        }

        if (state == DataReadStateE) {
            //try read the data
            const char* tmpbufpos = packet_header.load(pbufpos, _rproto);
            if (static_cast<size_t>(pbufend - tmpbufpos) >= packet_header.size()) {
                pbufpos = tmpbufpos;
            } else {
                break;
            }
        }

        if (not packet_header.isOk()) {
            _rerror = error_reader_invalid_packet_header;
            SOLID_ASSERT(false);
            break;
        }

        if (packet_header.isTypeKeepAlive()) {
            MessagePointerT dummy_message_ptr;
            _complete_fnc(KeepaliveCompleteE, dummy_message_ptr, InvalidIndex());
            continue;
        }

        //parse the data
        doConsumePacket(
            pbufpos,
            packet_header,
            _complete_fnc,
            _rconfig,
            _rproto,
            _rctx,
            _rerror);

        if (!_rerror) {
            pbufpos += packet_header.size();
        } else {
            SOLID_ASSERT(false);
            break;
        }
        state = HeaderReadStateE;
    }

    return pbufpos - _pbuf;
}
//-----------------------------------------------------------------------------
void MessageReader::doConsumePacket(
    const char*                _pbuf,
    PacketHeader const&        _packet_header,
    const CompleteFunctionT&   _complete_fnc,
    ReaderConfiguration const& _rconfig,
    Protocol const&            _rproto,
    ConnectionContext&         _rctx,
    ErrorConditionT&           _rerror)
{
    const char* pbufpos = _pbuf;
    const char* pbufend = _pbuf + _packet_header.size();

    //decompress = TODO: try not to use so much stack
    char tmpbuf[Protocol::MaxPacketDataSize];

    if (_packet_header.isCompressed()) {
        size_t uncompressed_size = _rconfig.decompress_fnc(tmpbuf, pbufpos, pbufend - pbufpos, _rerror);

        if (!_rerror) {
            pbufpos = tmpbuf;
            pbufend = tmpbuf + uncompressed_size;
        } else {
            return;
        }
    }

    uint8_t crt_msg_type = _packet_header.type();
    //DeserializerPointerT  tmp_deserializer;

    while (pbufpos < pbufend) {

        bool canceled_message = false;

        switch (crt_msg_type) {
        case PacketHeader::SwitchToNewMessageTypeE:
        case PacketHeader::SwitchToNewEndMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToNewMessageTypeE " << message_q.size());

            if (message_q.front().message_ptr) {
                if (message_q.size() == _rconfig.max_message_count_multiplex) {
                    SOLID_ASSERT(false);
                    _rerror = error_reader_too_many_multiplex;
                    return;
                }
                message_q.front().packet_count = 0;
                //reschedule the current message for later
                message_q.push(std::move(message_q.front()));
            }

            if (not message_q.front().deserializer_ptr) {
                message_q.front().deserializer_ptr = _rproto.createDeserializer();
            }

            _rproto.reset(*message_q.front().deserializer_ptr);

            message_q.front().deserializer_ptr->push(message_q.front().message_header);
            message_q.front().is_reading_message_header = true;

            break;

        case PacketHeader::SwitchToOldMessageTypeE:
        case PacketHeader::SwitchToOldEndMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToOldMessageTypeE " << message_q.size());

            if (message_q.front().message_ptr) {
                message_q.push(std::move(message_q.front()));
                message_q.front().packet_count = 0;
            }
            message_q.pop();
            break;

        case PacketHeader::ContinuedMessageTypeE:
        case PacketHeader::ContinuedEndMessageTypeE:

            vdbgx(Debug::mpipc, "ContinuedMessageTypeE " << message_q.size());

            SOLID_ASSERT(message_q.size() and message_q.front().deserializer_ptr /* and message_q.front().message_ptr*/);

            if (not message_q.front().is_reading_message_header) {
                ++message_q.front().packet_count;
            }
            break;

        case PacketHeader::SwitchToOldCanceledMessageTypeE:

            vdbgx(Debug::mpipc, "SwitchToOldCanceledMessageTypeE " << message_q.size());

            if (message_q.front().message_ptr) {
                message_q.push(std::move(message_q.front()));
                message_q.front().packet_count = 0;
            }
            message_q.pop();
            canceled_message = true;
            message_q.front().clear();
            break;

        case PacketHeader::ContinuedCanceledMessageTypeE:

            vdbgx(Debug::mpipc, "ContinuedCanceledMessageTypeE " << message_q.size());

            SOLID_ASSERT(message_q.size() and message_q.front().deserializer_ptr and message_q.front().message_ptr);
            message_q.front().clear();

            canceled_message = true;
            break;
        default:
            SOLID_ASSERT(false);
            _rerror = error_reader_invalid_message_switch;
            return;
        }

        if (not canceled_message) {

            MessageStub& rmsgstub                            = message_q.front();
            const bool   is_currently_reading_message_header = rmsgstub.is_reading_message_header;
            uint16_t     message_size                        = 0;

            if (static_cast<size_t>(pbufend - pbufpos) >= sizeof(uint16_t)) {
                pbufpos = _rproto.loadValue(pbufpos, message_size);
            } else {
                //protocol error
                _rerror = error_reader_protocol;
                break;
            }

            _rctx.pmessage_header = &rmsgstub.message_header;

            int rv = rmsgstub.deserializer_ptr->run(_rctx, pbufpos, pbufend - pbufpos);

            if (rv >= 0) {

                pbufpos += rv;

                if ((crt_msg_type & PacketHeader::EndMessageTypeE) && rmsgstub.deserializer_ptr->empty()) {

                    if (is_currently_reading_message_header) {

                        rmsgstub.is_reading_message_header = false;
                        rmsgstub.deserializer_ptr->clear();

                        message_q.front().deserializer_ptr->push(rmsgstub.message_ptr);

                    } else {
                        SOLID_ASSERT(rv == message_size);
                        //done with the message
                        rmsgstub.deserializer_ptr->clear();

                        const size_t message_type_id = rmsgstub.message_ptr.get() ? _rproto.typeIndex(rmsgstub.message_ptr.get()) : InvalidIndex();

                        //complete the message waiting for this response
                        _complete_fnc(MessageCompleteE, rmsgstub.message_ptr, message_type_id);

                        message_q.front().message_ptr.reset();
                    }
                }
            } else {
                _rerror = message_q.front().deserializer_ptr->error();
                break;
            }
        }

        if (pbufpos < pbufend) {
            crt_msg_type = 0;
            pbufpos      = _rproto.loadValue(pbufpos, crt_msg_type);
        }
    } //while
}

} //namespace mpipc
} //namespace frame
} //namespace solid
